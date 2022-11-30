// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_connection_helper.h"
#include "base/no_destructor.h"

namespace net {

namespace {
quiche::QuicheBufferAllocator* GetBufferAllocator() {
  static base::NoDestructor<quiche::SimpleBufferAllocator> allocator;
  return &*allocator;
}
}  // namespace

QuicChromiumConnectionHelper::QuicChromiumConnectionHelper(
    const quic::QuicClock* clock,
    quic::QuicRandom* random_generator)
    : clock_(clock), random_generator_(random_generator) {}

QuicChromiumConnectionHelper::~QuicChromiumConnectionHelper() = default;

const quic::QuicClock* QuicChromiumConnectionHelper::GetClock() const {
  return clock_;
}

quic::QuicRandom* QuicChromiumConnectionHelper::GetRandomGenerator() {
  return random_generator_;
}

quiche::QuicheBufferAllocator*
QuicChromiumConnectionHelper::GetStreamSendBufferAllocator() {
  return GetBufferAllocator();
}

}  // namespace net
