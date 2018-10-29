// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"

namespace blink {

// IceTransportAdapter implementation backed by the WebRTC PortAllocator /
// P2PTransportChannel.
class IceTransportAdapterImpl final : public IceTransportAdapter,
                                      public sigslot::has_slots<> {
 public:
  // Must be constructed on the WebRTC worker thread.
  // |delegate| must outlive the IceTransportAdapter.
  // |thread| should be the rtc::Thread instance associated with the WebRTC
  // worker thread.
  IceTransportAdapterImpl(
      Delegate* delegate,
      std::unique_ptr<cricket::PortAllocator> port_allocator,
      rtc::Thread* thread);
  ~IceTransportAdapterImpl() override;

  // IceTransportAdapter overrides.
  void StartGathering(
      const cricket::IceParameters& local_parameters,
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportPolicy policy) override;
  void Start(const cricket::IceParameters& remote_parameters,
             cricket::IceRole role,
             const std::vector<cricket::Candidate>& initial_remote_candidates)
      override;
  void HandleRemoteRestart(
      const cricket::IceParameters& new_remote_parameters) override;
  void AddRemoteCandidate(const cricket::Candidate& candidate) override;
  P2PQuicPacketTransport* packet_transport() const override;

 private:
  // Callbacks from P2PTransportChannel.
  void OnGatheringStateChanged(cricket::IceTransportInternal* transport);
  void OnCandidateGathered(cricket::IceTransportInternal* transport,
                           const cricket::Candidate& candidate);
  void OnStateChanged(cricket::IceTransportInternal* transport);
  void OnNetworkRouteChanged(
      absl::optional<rtc::NetworkRoute> new_network_route);

  Delegate* const delegate_;
  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  std::unique_ptr<cricket::P2PTransportChannel> p2p_transport_channel_;
  std::unique_ptr<P2PQuicPacketTransport> quic_packet_transport_adapter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
