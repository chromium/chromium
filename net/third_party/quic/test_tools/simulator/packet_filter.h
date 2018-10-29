// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PACKET_FILTER_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PACKET_FILTER_H_

#include "net/third_party/quic/test_tools/simulator/port.h"

namespace quic {
namespace simulator {

// Packet filter allows subclasses to filter out the packets that enter the
// input port and exit the output port.  Packets in the other direction are
// always passed through.
//
// The filter wraps around the input endpoint, and exposes the resulting
// filtered endpoint via the output() method.  For example, if initially there
// are two endpoints, A and B, connected via a symmetric link:
//
//   QuicEndpoint endpoint_a;
//   QuicEndpoint endpoint_b;
//
//   [...]
//
//   SymmetricLink a_b_link(&endpoint_a, &endpoint_b, ...);
//
// and the goal is to filter the traffic from A to B, then the new invocation
// would be as follows:
//
//   PacketFilter filter(&simulator, "A-to-B packet filter", endpoint_a);
//   SymmetricLink a_b_link(&filter, &endpoint_b, ...);
//
// Note that the filter drops the packet instanteneously, without it ever
// reaching the output wire.  This means that in a direct endpoint-to-endpoint
// scenario, whenever the packet is dropped, the link would become immediately
// available for the next packet.
class PacketFilter : public Endpoint, public ConstrainedPortInterface {
 public:
  // Initialize the filter by wrapping around |input|.  Does not take the
  // ownership of |input|.
  PacketFilter(Simulator* simulator, QuicString name, Endpoint* input);
  PacketFilter(const PacketFilter&) = delete;
  PacketFilter& operator=(const PacketFilter&) = delete;
  ~PacketFilter() override;

  // Implementation of ConstrainedPortInterface.
  void AcceptPacket(std::unique_ptr<Packet> packet) override;
  QuicTime::Delta TimeUntilAvailable() override;

  // Implementation of Endpoint interface methods.
  UnconstrainedPortInterface* GetRxPort() override;
  void SetTxPort(ConstrainedPortInterface* port) override;

  // Implementation of Actor interface methods.
  void Act() override;

 protected:
  // Returns true if the packet should be passed through, and false if it should
  // be dropped.  The function is called once per packet, in the order that the
  // packets arrive, so it is safe for the function to alter the internal state
  // of the filter.
  virtual bool FilterPacket(const Packet& packet) = 0;

 private:
  // The port onto which the filtered packets are egressed.
  ConstrainedPortInterface* output_tx_port_;

  // The original network endpoint wrapped by the class.
  Endpoint* input_;
};

}  // namespace simulator
}  // namespace quic
#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_PACKET_FILTER_H_
