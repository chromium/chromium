// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_FIFO_PACKET_H_
#define LIBRARIES_NACL_IO_SOCKET_FIFO_PACKET_H_

#include <stdint.h>
#include <string.h>

#include <list>

#include "nacl_io/fifo_interface.h"
#include "ppapi/c/pp_resource.h"

#include "sdk_util/macros.h"

namespace nacl_io {

class Packet;

// FIFOPacket
//
// A FIFOPackiet is linked list of packets.  Data is stored and returned
// in packet size increments.  FIFOPacket signals EMPTY where there are
// no packets, and FULL when the total bytes of all packets meets or
// exceeds the max size hint.
class FIFOPacket : public FIFOInterface {
 public:
  explicit FIFOPacket(size_t size);

  FIFOPacket(const FIFOPacket&) = delete;
  FIFOPacket& operator=(const FIFOPacket&) = delete;

  virtual ~FIFOPacket();

  virtual bool IsEmpty();
  virtual bool IsFull();
  virtual bool Resize(size_t len);

  size_t ReadAvailable();
  size_t WriteAvailable();

  // Return a pointer to the top packet without releasing ownership.
  Packet* PeekPacket();

  // Relinquish top packet, and remove it from the FIFO.
  Packet* ReadPacket();

  // Take ownership of packet and place it in the FIFO.
  void WritePacket(Packet* packet);

  // Read out the top packet into a byte buffer.
  size_t Read(void* buf, size_t len);

  // Enqueue a new packet from a byte buffer.
  size_t Write(const void* buf, size_t len);

 private:
  std::list<Packet*> packets_;
  uint32_t max_bytes_;
  uint32_t cur_bytes_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_SOCKET_FIFO_PACKET_H_
