#include <algorithm>
#include <assert.h>
#include "interface.h"
#include "htif_csim.h"

#define HTIF_MAX_DATA_SIZE C_MAX_DATA_SIZE
#include "htif-packet.h"

htif_csim_t::htif_csim_t(int _fdin, int _fdout)
: fdin(_fdin), fdout(_fdout), seqno(1)
{
}

htif_csim_t::~htif_csim_t()
{
}

void htif_csim_t::read_packet(packet_t* p, int expected_seqno)
{
  int bytes = read(fdin, p, sizeof(packet_t));
  if (bytes == -1 || bytes < (int)offsetof(packet_t, data))
    throw io_error("read failed");
  if (p->seqno != expected_seqno)
    throw bad_seqno_error(); 
  switch (p->cmd)
  {
    case HTIF_CMD_ACK:
      if (p->data_size != bytes - offsetof(packet_t, data))
        throw packet_error("bad payload size!");
      break;
    case HTIF_CMD_NACK:
      throw packet_error("nack!");
    default:
      throw packet_error("illegal command");
  }
}

void htif_csim_t::write_packet(const packet_t* p)
{
  int size = offsetof(packet_t, data);
  if(p->cmd == HTIF_CMD_WRITE_MEM || p->cmd == HTIF_CMD_WRITE_CONTROL_REG)
    size += p->data_size;
    
  int bytes = write(fdout, p, size);
  if (bytes < size)
    throw io_error("write failed");
}

void htif_csim_t::start(int coreid)
{
  packet_t p = {HTIF_CMD_START, seqno, 0, 0};
  write_packet(&p);
  read_packet(&p, seqno);
  seqno++;
}

void htif_csim_t::stop(int coreid)
{
  packet_t p = {HTIF_CMD_STOP, seqno, 0, 0};
  write_packet(&p);
  read_packet(&p, seqno);
  seqno++;
}

void htif_csim_t::read_chunk(addr_t taddr, size_t len, uint8_t* dst, int cmd)
{
  assert(cmd == IF_CREG || taddr % chunk_align() == 0);
  assert(len % chunk_align() == 0);

  packet_t req;
  packet_t resp;

  if (cmd == IF_MEM)
    req.cmd = HTIF_CMD_READ_MEM;
  else if (cmd == IF_CREG)
    req.cmd = HTIF_CMD_READ_CONTROL_REG;
  else
    assert(0);

  while (len)
  {
    size_t sz = std::min(len, C_MAX_DATA_SIZE);

    req.seqno = seqno;
    req.data_size = sz;
    req.addr = taddr;

    write_packet(&req);
    read_packet(&resp, seqno);
    seqno++;

    memcpy(dst, resp.data, sz);

    len -= sz;
    taddr += sz;
    dst += sz;
  }
}

void htif_csim_t::write_chunk(addr_t taddr, size_t len, const uint8_t* src, int cmd)
{
  assert(cmd == IF_CREG || taddr % chunk_align() == 0);
  assert(len % chunk_align() == 0);

  packet_t req;
  packet_t resp;

  if (cmd == IF_MEM)
    req.cmd = HTIF_CMD_WRITE_MEM;
  else if (cmd == IF_CREG)
    req.cmd = HTIF_CMD_WRITE_CONTROL_REG;
  else
    assert(0);

  while (len)
  {
    size_t sz = std::min(len, C_MAX_DATA_SIZE);

    req.seqno = seqno;
    req.data_size = sz;
    req.addr = taddr;

    memcpy(req.data, src, sz);

    write_packet(&req);
    read_packet(&resp, seqno);
    seqno++;

    len -= sz;
    taddr += sz;
    src += sz;
  }
}

reg_t htif_csim_t::read_cr(int coreid, int regnum)
{
  reg_t val;
  read_chunk((addr_t)coreid<<32|regnum, sizeof(reg_t), (uint8_t*)&val, IF_CREG);
  return val;
}

void htif_csim_t::write_cr(int coreid, int regnum, reg_t val)
{
  write_chunk((addr_t)coreid<<32|regnum, sizeof(reg_t), (uint8_t*)&val, IF_CREG);
}

size_t htif_csim_t::chunk_align()
{
  return C_DATA_ALIGN;
}