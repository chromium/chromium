// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_

#include <memory>

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"
#include "third_party/webrtc/api/ice_transport_interface.h"

namespace blink {

// IceTransportAdapter implementation backed by the WebRTC PortAllocator /
// P2PTransportChannel.
class IceTransportAdapterImpl final : public IceTransportAdapter,
                                      public sigslot::has_slots<> {
 public:
  // Must be constructed on the WebRTC worker thread.
  // |delegate| must outlive the IceTransportAdapter.
  IceTransportAdapterImpl(
      Delegate* delegate,
      std::unique_ptr<cricket::PortAllocator> port_allocator,
      std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory);

  // Create an IceTransportAdapter for an existing |ice_transport_channel|
  // object. In this case, |port_allocator_|, |async_resolver_factory_| and
  // |quic_packet_transport_adapter_| are not used (and null).
  IceTransportAdapterImpl(
      Delegate* delegate,
      rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport_channel);

  ~IceTransportAdapterImpl() override;

  // IceTransportAdapter overrides.
  void StartGathering(const cricket::IceParameters& local_parameters,
                      const cricket::ServerAddresses& stun_servers,
                      const WebVector<cricket::RelayServerConfig>& turn_servers,
                      IceTransportPolicy policy) override;
  void Start(
      const cricket::IceParameters& remote_parameters,
      cricket::IceRole role,
      const Vector<cricket::Candidate>& initial_remote_candidates) override;
  void HandleRemoteRestart(
      const cricket::IceParameters& new_remote_parameters) override;
  void AddRemoteCandidate(const cricket::Candidate& candidate) override;
  P2PQuicPacketTransport* packet_transport() const override;

 private:
  cricket::IceTransportInternal* ice_transport_channel() {
    return ice_transport_channel_->internal();
  }
  void SetupIceTransportChannel();
  // Callbacks from P2PTransportChannel.
  void OnGatheringStateChanged(cricket::IceTransportInternal* transport);
  void OnCandidateGathered(cricket::IceTransportInternal* transport,
                           const cricket::Candidate& candidate);
  void OnStateChanged(cricket::IceTransportInternal* transport);
  void OnNetworkRouteChanged(
      absl::optional<rtc::NetworkRoute> new_network_route);
  void OnRoleConflict(cricket::IceTransportInternal* transport);

  Delegate* const delegate_;
  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory_;
  rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport_channel_;
  std::unique_ptr<P2PQuicPacketTransport> quic_packet_transport_adapter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
