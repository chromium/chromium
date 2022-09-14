// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_UDP_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_SOCKET_UDP_EVENT_EMITTER_H_

#include "nacl_io/socket/fifo_packet.h"
#include "nacl_io/stream/stream_event_emitter.h"

#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class UdpEventEmitter;
typedef sdk_util::ScopedRef<UdpEventEmitter> ScopedUdpEventEmitter;

class UdpEventEmitter : public StreamEventEmitter {
 public:
  UdpEventEmitter(size_t rsize, size_t wsize);

  UdpEventEmitter(const UdpEventEmitter&) = delete;
  UdpEventEmitter& operator=(const UdpEventEmitter&) = delete;

  // Takes or gives away ownership of the packet.
  Packet* ReadRXPacket_Locked();
  void WriteRXPacket_Locked(Packet* packet);

  Packet* ReadTXPacket_Locked();
  void WriteTXPacket_Locked(Packet* packet);

 protected:
  virtual FIFOPacket* in_fifo() { return &in_fifo_; }
  virtual FIFOPacket* out_fifo() { return &out_fifo_; }

 private:
  FIFOPacket in_fifo_;
  FIFOPacket out_fifo_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_SOCKET_UDP_EVENT_EMITTER_H_
