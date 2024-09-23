// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"
#include "third_party/webrtc/api/ice_transport_interface.h"

namespace blink {

// IceTransportAdapter implementation backed by the WebRTC PortAllocator /
// P2PTransportChannel.
class IceTransportAdapterImpl final : public IceTransportAdapter,
                                      public sigslot::has_slots<> {
 public:
  // Create an IceTransportAdapter for an existing |ice_transport_channel|
  // object.
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
      std::optional<rtc::NetworkRoute> new_network_route);
  void OnRoleConflict(cricket::IceTransportInternal* transport);

  const raw_ptr<Delegate> delegate_;
  rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport_channel_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_ADAPTER_IMPL_H_
