// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"

#include <utility>

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_packet_transport_adapter.h"
#include "third_party/webrtc/api/ice_transport_factory.h"

namespace blink {

IceTransportAdapterImpl::IceTransportAdapterImpl(
    Delegate* delegate,
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory)
    : delegate_(delegate),
      port_allocator_(std::move(port_allocator)),
      async_resolver_factory_(std::move(async_resolver_factory)) {
  // These settings are copied from PeerConnection:
  // https://codesearch.chromium.org/chromium/src/third_party/webrtc/pc/peerconnection.cc?l=4708&rcl=820ebd0f661696043959b5105b2814e0edd8b694
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_flags(port_allocator_->flags() |
                             cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                             cricket::PORTALLOCATOR_ENABLE_IPV6 |
                             cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
  port_allocator_->Initialize();

  webrtc::IceTransportInit ice_transport_init;
  ice_transport_init.set_port_allocator(port_allocator_.get());
  ice_transport_init.set_async_resolver_factory(async_resolver_factory_.get());
  ice_transport_channel_ =
      webrtc::CreateIceTransport(std::move(ice_transport_init));
  SetupIceTransportChannel();
  // We need to set the ICE role even before Start is called since the Port
  // assumes that the role has been set before receiving incoming connectivity
  // checks. These checks can race with the information signaled for Start.
  ice_transport_channel()->SetIceRole(cricket::ICEROLE_CONTROLLING);
  // The ICE tiebreaker is used to determine which side is controlling/
  // controlled when both sides start in the same role. The number is randomly
  // generated so that each peer can calculate a.tiebreaker <= b.tiebreaker
  // consistently.
  ice_transport_channel()->SetIceTiebreaker(rtc::CreateRandomId64());

  quic_packet_transport_adapter_ =
      std::make_unique<QuicPacketTransportAdapter>(ice_transport_channel());
}

IceTransportAdapterImpl::IceTransportAdapterImpl(
    Delegate* delegate,
    rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport)
    : delegate_(delegate), ice_transport_channel_(ice_transport) {
  // The native webrtc peer connection might have been closed in the meantime,
  // clearing the ice transport channel; don't do anything in that case. |this|
  // will eventually be destroyed when the blink layer gets notified by the
  // webrtc layer that the transport has been cleared.
  if (ice_transport_channel())
    SetupIceTransportChannel();
}

IceTransportAdapterImpl::~IceTransportAdapterImpl() = default;

static uint32_t GetCandidateFilterForPolicy(IceTransportPolicy policy) {
  switch (policy) {
    case IceTransportPolicy::kRelay:
      return cricket::CF_RELAY;
    case IceTransportPolicy::kAll:
      return cricket::CF_ALL;
  }
  NOTREACHED();
  return 0;
}

void IceTransportAdapterImpl::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const WebVector<cricket::RelayServerConfig>& turn_servers,
    IceTransportPolicy policy) {
  if (port_allocator_) {
    port_allocator_->set_candidate_filter(GetCandidateFilterForPolicy(policy));
    port_allocator_->SetConfiguration(
        stun_servers,
        const_cast<WebVector<cricket::RelayServerConfig>&>(turn_servers)
            .ReleaseVector(),
        port_allocator_->candidate_pool_size(),
        port_allocator_->prune_turn_ports());
  }
  if (!ice_transport_channel()) {
    LOG(ERROR) << "StartGathering called, but ICE transport released";
    return;
  }
  ice_transport_channel()->SetIceParameters(local_parameters);
  ice_transport_channel()->MaybeStartGathering();
  DCHECK_EQ(ice_transport_channel()->gathering_state(),
            cricket::kIceGatheringGathering);
}

void IceTransportAdapterImpl::Start(
    const cricket::IceParameters& remote_parameters,
    cricket::IceRole role,
    const Vector<cricket::Candidate>& initial_remote_candidates) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "Start called, but ICE transport released";
    return;
  }
  ice_transport_channel()->SetRemoteIceParameters(remote_parameters);
  ice_transport_channel()->SetIceRole(role);
  for (const auto& candidate : initial_remote_candidates) {
    ice_transport_channel()->AddRemoteCandidate(candidate);
  }
}

void IceTransportAdapterImpl::HandleRemoteRestart(
    const cricket::IceParameters& new_remote_parameters) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "HandleRemoteRestart called, but ICE transport released";
    return;
  }
  ice_transport_channel()->RemoveAllRemoteCandidates();
  ice_transport_channel()->SetRemoteIceParameters(new_remote_parameters);
}

void IceTransportAdapterImpl::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "AddRemoteCandidate called, but ICE transport released";
    return;
  }
  ice_transport_channel()->AddRemoteCandidate(candidate);
}

P2PQuicPacketTransport* IceTransportAdapterImpl::packet_transport() const {
  return quic_packet_transport_adapter_.get();
}

void IceTransportAdapterImpl::SetupIceTransportChannel() {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "SetupIceTransportChannel called, but ICE transport released";
    return;
  }
  ice_transport_channel()->SignalGatheringState.connect(
      this, &IceTransportAdapterImpl::OnGatheringStateChanged);
  ice_transport_channel()->SignalCandidateGathered.connect(
      this, &IceTransportAdapterImpl::OnCandidateGathered);
  ice_transport_channel()->SignalIceTransportStateChanged.connect(
      this, &IceTransportAdapterImpl::OnStateChanged);
  ice_transport_channel()->SignalNetworkRouteChanged.connect(
      this, &IceTransportAdapterImpl::OnNetworkRouteChanged);
  ice_transport_channel()->SignalRoleConflict.connect(
      this, &IceTransportAdapterImpl::OnRoleConflict);
}

void IceTransportAdapterImpl::OnGatheringStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnGatheringStateChanged(
      ice_transport_channel()->gathering_state());
}

void IceTransportAdapterImpl::OnCandidateGathered(
    cricket::IceTransportInternal* transport,
    const cricket::Candidate& candidate) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnCandidateGathered(candidate);
}

void IceTransportAdapterImpl::OnStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnStateChanged(ice_transport_channel()->GetIceTransportState());
}

void IceTransportAdapterImpl::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> new_network_route) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "OnNetworkRouteChanged called, but ICE transport released";
    return;
  }
  const absl::optional<const cricket::CandidatePair> selected_pair =
      ice_transport_channel()->GetSelectedCandidatePair();
  if (!selected_pair) {
    // The selected connection will only be null if the ICE connection has
    // totally failed, at which point we'll get a StateChanged signal. The
    // client will implicitly clear the selected candidate pair when it receives
    // the failed state change, so we don't need to give an explicit callback
    // here.
    return;
  }
  delegate_->OnSelectedCandidatePairChanged(std::make_pair(
      selected_pair->local_candidate(), selected_pair->remote_candidate()));
}

static const char* IceRoleToString(cricket::IceRole role) {
  switch (role) {
    case cricket::ICEROLE_CONTROLLING:
      return "controlling";
    case cricket::ICEROLE_CONTROLLED:
      return "controlled";
    default:
      return "unknown";
  }
}

static cricket::IceRole IceRoleReversed(cricket::IceRole role) {
  switch (role) {
    case cricket::ICEROLE_CONTROLLING:
      return cricket::ICEROLE_CONTROLLED;
    case cricket::ICEROLE_CONTROLLED:
      return cricket::ICEROLE_CONTROLLING;
    default:
      return cricket::ICEROLE_UNKNOWN;
  }
}

void IceTransportAdapterImpl::OnRoleConflict(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  // This logic is copied from JsepTransportController.
  cricket::IceRole reversed_role =
      IceRoleReversed(ice_transport_channel()->GetIceRole());
  LOG(INFO) << "Got role conflict; switching to "
            << IceRoleToString(reversed_role) << " role.";
  ice_transport_channel()->SetIceRole(reversed_role);
}

}  // namespace blink
