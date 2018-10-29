// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_LINK_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_LINK_H_

#include <utility>

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/test_tools/simulator/actor.h"
#include "net/third_party/quic/test_tools/simulator/port.h"

namespace quic {
namespace simulator {

// A reliable simplex link between two endpoints with constrained bandwidth.  A
// few microseconds of random delay are added for every packet to avoid
// synchronization issues.
class OneWayLink : public Actor, public ConstrainedPortInterface {
 public:
  OneWayLink(Simulator* simulator,
             QuicString name,
             UnconstrainedPortInterface* sink,
             QuicBandwidth bandwidth,
             QuicTime::Delta propagation_delay);
  OneWayLink(const OneWayLink&) = delete;
  OneWayLink& operator=(const OneWayLink&) = delete;
  ~OneWayLink() override;

  void AcceptPacket(std::unique_ptr<Packet> packet) override;
  QuicTime::Delta TimeUntilAvailable() override;
  void Act() override;

  inline QuicBandwidth bandwidth() const { return bandwidth_; }

 private:
  struct QueuedPacket {
    std::unique_ptr<Packet> packet;
    QuicTime dequeue_time;

    QueuedPacket(std::unique_ptr<Packet> packet, QuicTime dequeue_time);
    QueuedPacket(QueuedPacket&& other);
    ~QueuedPacket();
  };

  // Schedule the next packet to be egressed out of the link if there are
  // packets on the link.
  void ScheduleNextPacketDeparture();

  // Get the value of a random delay imposed on each packet in order to avoid
  // artifical synchronization artifacts during the simulation.
  QuicTime::Delta GetRandomDelay(QuicTime::Delta transfer_time);

  UnconstrainedPortInterface* sink_;
  QuicQueue<QueuedPacket> packets_in_transit_;

  const QuicBandwidth bandwidth_;
  const QuicTime::Delta propagation_delay_;

  QuicTime next_write_at_;
};

// A full-duplex link between two endpoints, functionally equivalent to two
// OneWayLink objects tied together.
class SymmetricLink {
 public:
  SymmetricLink(Simulator* simulator,
                QuicString name,
                UnconstrainedPortInterface* sink_a,
                UnconstrainedPortInterface* sink_b,
                QuicBandwidth bandwidth,
                QuicTime::Delta propagation_delay);
  SymmetricLink(Endpoint* endpoint_a,
                Endpoint* endpoint_b,
                QuicBandwidth bandwidth,
                QuicTime::Delta propagation_delay);
  SymmetricLink(const SymmetricLink&) = delete;
  SymmetricLink& operator=(const SymmetricLink&) = delete;

  inline QuicBandwidth bandwidth() { return a_to_b_link_.bandwidth(); }

 private:
  OneWayLink a_to_b_link_;
  OneWayLink b_to_a_link_;
};

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_LINK_H_
