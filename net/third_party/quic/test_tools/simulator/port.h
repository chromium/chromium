// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PORT_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PORT_H_

#include <string>
#include <utility>

#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/test_tools/simulator/actor.h"

namespace quic {
namespace simulator {

struct Packet {
  Packet();
  ~Packet();
  Packet(const Packet& packet);

  QuicString source;
  QuicString destination;
  QuicTime tx_timestamp;

  QuicString contents;
  QuicByteCount size;
};

// An interface for anything that accepts packets at arbitrary rate.
class UnconstrainedPortInterface {
 public:
  virtual ~UnconstrainedPortInterface() {}
  virtual void AcceptPacket(std::unique_ptr<Packet> packet) = 0;
};

// An interface for any device that accepts packets at a specific rate.
// Typically one would use a Queue object in order to write into a constrained
// port.
class ConstrainedPortInterface {
 public:
  virtual ~ConstrainedPortInterface() {}

  // Accept a packet for a port.  TimeUntilAvailable() must be zero before this
  // method is called.
  virtual void AcceptPacket(std::unique_ptr<Packet> packet) = 0;

  // Time until write for the next port is available.  Cannot be infinite.
  virtual QuicTime::Delta TimeUntilAvailable() = 0;
};

// A convenience class for any network endpoints, i.e. the objects which can
// both accept and send packets.
class Endpoint : public Actor {
 public:
  virtual UnconstrainedPortInterface* GetRxPort() = 0;
  virtual void SetTxPort(ConstrainedPortInterface* port) = 0;

 protected:
  Endpoint(Simulator* simulator, QuicString name);
};

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PORT_H_
