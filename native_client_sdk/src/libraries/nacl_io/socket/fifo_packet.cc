// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/fifo_packet.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/socket/packet.h"

namespace nacl_io {

FIFOPacket::FIFOPacket(size_t size) : max_bytes_(size), cur_bytes_(0) {
}

FIFOPacket::~FIFOPacket() {
  while (!IsEmpty())
    delete ReadPacket();
}

bool FIFOPacket::IsEmpty() {
  return packets_.empty();
}

bool FIFOPacket::Resize(size_t len) {
  max_bytes_ = len;
  return true;
}

size_t FIFOPacket::ReadAvailable() {
  return cur_bytes_;
}

size_t FIFOPacket::WriteAvailable() {
  if (cur_bytes_ > max_bytes_)
    return 0;

  return max_bytes_ - cur_bytes_;
}

bool FIFOPacket::IsFull() {
  return cur_bytes_ >= max_bytes_;
}

Packet* FIFOPacket::PeekPacket() {
  if (packets_.empty())
    return NULL;

  return packets_.back();
}

Packet* FIFOPacket::ReadPacket() {
  if (packets_.empty())
    return NULL;

  Packet* out = packets_.back();
  packets_.pop_back();

  cur_bytes_ -= out->len();
  return out;
}

void FIFOPacket::WritePacket(Packet* packet) {
  cur_bytes_ += packet->len();
  packets_.push_front(packet);
}

size_t FIFOPacket::Read(void* buf, size_t len) {
  Packet* packet = ReadPacket();
  if (!packet)
    return 0;

  size_t bytes = packet->len();
  if (bytes > len)
    bytes = len;
  memcpy(buf, packet->buffer(), bytes);

  delete packet;
  return bytes;
}

size_t FIFOPacket::Write(const void* buf, size_t len) {
  if (len > WriteAvailable())
    return 0;

  Packet* packet = new Packet(NULL);
  packet->Copy(buf, len, 0);
  WritePacket(packet);
  return len;
}

}  // namespace nacl_io
