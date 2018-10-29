// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simulator/port.h"

namespace quic {
namespace simulator {

Packet::Packet()
    : source(), destination(), tx_timestamp(QuicTime::Zero()), size(0) {}

Packet::~Packet() {}

Packet::Packet(const Packet& packet) = default;

Endpoint::Endpoint(Simulator* simulator, QuicString name)
    : Actor(simulator, name) {}

}  // namespace simulator
}  // namespace quic
