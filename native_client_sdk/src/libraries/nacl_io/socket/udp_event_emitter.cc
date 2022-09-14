// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/udp_event_emitter.h"

#include "sdk_util/auto_lock.h"

namespace nacl_io {

UdpEventEmitter::UdpEventEmitter(size_t rsize, size_t wsize)
    : in_fifo_(rsize), out_fifo_(wsize) {
  UpdateStatus_Locked();
}

Packet* UdpEventEmitter::ReadRXPacket_Locked() {
  Packet* packet = in_fifo_.ReadPacket();

  UpdateStatus_Locked();
  return packet;
}

void UdpEventEmitter::WriteRXPacket_Locked(Packet* packet) {
  in_fifo_.WritePacket(packet);

  UpdateStatus_Locked();
}

Packet* UdpEventEmitter::ReadTXPacket_Locked() {
  Packet* packet = out_fifo_.ReadPacket();

  UpdateStatus_Locked();
  return packet;
}

void UdpEventEmitter::WriteTXPacket_Locked(Packet* packet) {
  out_fifo_.WritePacket(packet);

  UpdateStatus_Locked();
}

}  // namespace nacl_io
