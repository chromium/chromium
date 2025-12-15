// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/webrtc/api/ice_transport_factory.h"

namespace blink {

IceTransportAdapterImpl::IceTransportAdapterImpl(
    Delegate* delegate,
    webrtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport)
    : delegate_(delegate), ice_transport_channel_(ice_transport) {
  // The native webrtc peer connection might have been closed in the meantime,
  // clearing the ice transport channel; don't do anything in that case. |this|
  // will eventually be destroyed when the blink layer gets notified by the
  // webrtc layer that the transport has been cleared.
  if (ice_transport_channel())
    SetupIceTransportChannel();
}

IceTransportAdapterImpl::~IceTransportAdapterImpl() {
  if (!ice_transport_channel()) {
    return;
  }
  ice_transport_channel()->RemoveGatheringStateCallback(this);
}

void IceTransportAdapterImpl::StartGathering(
    const webrtc::IceParameters& local_parameters,
    const webrtc::ServerAddresses& stun_servers,
    const std::vector<webrtc::RelayServerConfig>& turn_servers,
    IceTransportPolicy policy) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "StartGathering called, but ICE transport released";
    return;
  }
  ice_transport_channel()->SetIceParameters(local_parameters);
  ice_transport_channel()->MaybeStartGathering();
  DCHECK_EQ(ice_transport_channel()->gathering_state(),
            webrtc::kIceGatheringGathering);
}

void IceTransportAdapterImpl::Start(
    const webrtc::IceParameters& remote_parameters,
    webrtc::IceRole role,
    const Vector<webrtc::Candidate>& initial_remote_candidates) {
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
    const webrtc::IceParameters& new_remote_parameters) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "HandleRemoteRestart called, but ICE transport released";
    return;
  }
  ice_transport_channel()->RemoveAllRemoteCandidates();
  ice_transport_channel()->SetRemoteIceParameters(new_remote_parameters);
}

void IceTransportAdapterImpl::AddRemoteCandidate(
    const webrtc::Candidate& candidate) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "AddRemoteCandidate called, but ICE transport released";
    return;
  }
  ice_transport_channel()->AddRemoteCandidate(candidate);
}

void IceTransportAdapterImpl::SetupIceTransportChannel() {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "SetupIceTransportChannel called, but ICE transport released";
    return;
  }
  ice_transport_channel()->AddGatheringStateCallback(
      this, [that = weak_factory_.GetWeakPtr()](
                webrtc::IceTransportInternal* transport) {
        if (that) {
          that->OnGatheringStateChanged(transport);
        }
      });
  ice_transport_channel()->SubscribeCandidateGathered(
      this, [that = weak_factory_.GetWeakPtr()](
                webrtc::IceTransportInternal* transport,
                const webrtc::Candidate& candidate) {
        if (that) {
          that->OnCandidateGathered(transport, candidate);
        }
      });
  ice_transport_channel()->SubscribeIceTransportStateChanged(
      this, [that = weak_factory_.GetWeakPtr()](
                webrtc::IceTransportInternal* transport) {
        if (that) {
          that->OnStateChanged(transport);
        }
      });
  ice_transport_channel()->SubscribeNetworkRouteChanged(
      this, [that = weak_factory_.GetWeakPtr()](
                std::optional<webrtc::NetworkRoute> route) {
        if (that) {
          that->OnNetworkRouteChanged(route);
        }
      });
  ice_transport_channel()->SubscribeRoleConflict(
      this, [that = weak_factory_.GetWeakPtr()](
                webrtc::IceTransportInternal* transport) {
        if (that) {
          that->OnRoleConflict(transport);
        }
      });
}

void IceTransportAdapterImpl::OnGatheringStateChanged(
    webrtc::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnGatheringStateChanged(
      ice_transport_channel()->gathering_state());
}

void IceTransportAdapterImpl::OnCandidateGathered(
    webrtc::IceTransportInternal* transport,
    const webrtc::Candidate& candidate) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnCandidateGathered(candidate);
}

void IceTransportAdapterImpl::OnStateChanged(
    webrtc::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  delegate_->OnStateChanged(ice_transport_channel()->GetIceTransportState());
}

void IceTransportAdapterImpl::OnNetworkRouteChanged(
    std::optional<webrtc::NetworkRoute> new_network_route) {
  if (!ice_transport_channel()) {
    LOG(ERROR) << "OnNetworkRouteChanged called, but ICE transport released";
    return;
  }
  const std::optional<const webrtc::CandidatePair> selected_pair =
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

static const char* IceRoleToString(webrtc::IceRole role) {
  switch (role) {
    case webrtc::ICEROLE_CONTROLLING:
      return "controlling";
    case webrtc::ICEROLE_CONTROLLED:
      return "controlled";
    default:
      return "unknown";
  }
}

static webrtc::IceRole IceRoleReversed(webrtc::IceRole role) {
  switch (role) {
    case webrtc::ICEROLE_CONTROLLING:
      return webrtc::ICEROLE_CONTROLLED;
    case webrtc::ICEROLE_CONTROLLED:
      return webrtc::ICEROLE_CONTROLLING;
    default:
      return webrtc::ICEROLE_UNKNOWN;
  }
}

void IceTransportAdapterImpl::OnRoleConflict(
    webrtc::IceTransportInternal* transport) {
  DCHECK_EQ(transport, ice_transport_channel());
  // This logic is copied from JsepTransportController.
  webrtc::IceRole reversed_role =
      IceRoleReversed(ice_transport_channel()->GetIceRole());
  LOG(INFO) << "Got role conflict; switching to "
            << IceRoleToString(reversed_role) << " role.";
  ice_transport_channel()->SetIceRole(reversed_role);
}

}  // namespace blink
