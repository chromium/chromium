// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport_channel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/channel_socket_adapter.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/transport_context.h"
#include "third_party/webrtc/p2p/base/p2p_constants.h"
#include "third_party/webrtc/p2p/base/p2p_transport_channel.h"
#include "third_party/webrtc/p2p/base/packet_transport_internal.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc/rtc_base/crypto_random.h"

namespace remoting::protocol {

namespace {

const int kIceUfragLength = 16;

// Utility function to map a cricket::Candidate string type to a
// TransportRoute::RouteType enum value.
TransportRoute::RouteType CandidateTypeToTransportRouteType(
    const cricket::Candidate& c) {
  if (c.is_local()) {
    return TransportRoute::DIRECT;
  } else if (c.is_stun() || c.is_prflx()) {
    return TransportRoute::STUN;
  } else if (c.is_relay()) {
    return TransportRoute::RELAY;
  } else {
    LOG(FATAL) << "Unknown candidate type: " << c.type_name();
  }
}

}  // namespace

IceTransportChannel::IceTransportChannel(
    scoped_refptr<TransportContext> transport_context,
    const NetworkSettings& network_settings)
    : transport_context_(transport_context),
      ice_username_fragment_(rtc::CreateRandomString(kIceUfragLength)),
      connect_attempts_left_(network_settings.ice_reconnect_attempts),
      network_settings_(network_settings) {
  DCHECK(!ice_username_fragment_.empty());
}

IceTransportChannel::~IceTransportChannel() {
  DCHECK(delegate_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_->OnChannelDeleted(this);

  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  if (channel_) {
    task_runner->DeleteSoon(FROM_HERE, channel_.release());
  }
  if (port_allocator_) {
    task_runner->DeleteSoon(FROM_HERE, port_allocator_.release());
  }
}

void IceTransportChannel::Connect(const std::string& name,
                                  Delegate* delegate,
                                  ConnectedCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!name.empty());
  DCHECK(delegate);
  DCHECK(!callback.is_null());

  DCHECK(name_.empty());
  name_ = name;
  delegate_ = delegate;
  callback_ = std::move(callback);

  auto create_port_allocator_result =
      transport_context_->port_allocator_factory()->CreatePortAllocator(
          transport_context_, nullptr);
  port_allocator_ = std::move(create_port_allocator_result.allocator);
  std::move(create_port_allocator_result.apply_network_settings)
      .Run(network_settings_);

  // Create P2PTransportChannel, attach signal handlers and connect it.
  // TODO(sergeyu): Specify correct component ID for the channel.
  channel_ = std::make_unique<cricket::P2PTransportChannel>(
      std::string(), 0, port_allocator_.get());
  std::string ice_password = rtc::CreateRandomString(cricket::ICE_PWD_LENGTH);
  channel_->SetIceRole((transport_context_->role() == TransportRole::CLIENT)
                           ? cricket::ICEROLE_CONTROLLING
                           : cricket::ICEROLE_CONTROLLED);
  delegate_->OnChannelIceCredentials(this, ice_username_fragment_,
                                     ice_password);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password);
  channel_->SignalCandidateGathered.connect(
      this, &IceTransportChannel::OnCandidateGathered);
  channel_->SignalRouteChange.connect(this,
                                      &IceTransportChannel::OnRouteChange);
  channel_->SignalWritableState.connect(this,
                                        &IceTransportChannel::OnWritableState);
  channel_->set_incoming_only(
      !(network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_OUTGOING));

  channel_->Connect();
  channel_->MaybeStartGathering();

  // Pass pending ICE credentials and candidates to the channel.
  if (!remote_ice_username_fragment_.empty()) {
    channel_->SetRemoteIceCredentials(remote_ice_username_fragment_,
                                      remote_ice_password_);
  }

  while (!pending_candidates_.empty()) {
    channel_->AddRemoteCandidate(pending_candidates_.front());
    pending_candidates_.pop_front();
  }

  --connect_attempts_left_;

  // Start reconnection timer.
  reconnect_timer_.Start(FROM_HERE, network_settings_.ice_timeout, this,
                         &IceTransportChannel::TryReconnect);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&IceTransportChannel::NotifyConnected,
                                weak_factory_.GetWeakPtr()));
}

void IceTransportChannel::NotifyConnected() {
  // Create P2PDatagramSocket adapter for the P2PTransportChannel.
  std::unique_ptr<TransportChannelSocketAdapter> socket(
      new TransportChannelSocketAdapter(channel_.get()));
  socket->SetOnDestroyedCallback(base::BindOnce(
      &IceTransportChannel::OnChannelDestroyed, base::Unretained(this)));
  std::move(callback_).Run(std::move(socket));
}

void IceTransportChannel::SetRemoteCredentials(const std::string& ufrag,
                                               const std::string& password) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  remote_ice_username_fragment_ = ufrag;
  remote_ice_password_ = password;

  if (channel_) {
    channel_->SetRemoteIceCredentials(ufrag, password);
  }
}

void IceTransportChannel::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // To enforce the no-relay setting, it's not enough to not produce relay
  // candidates. It's also necessary to discard remote relay candidates.
  bool relay_allowed =
      (network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_RELAY) != 0;
  if (!relay_allowed && candidate.is_relay()) {
    return;
  }

  if (channel_) {
    channel_->AddRemoteCandidate(candidate);
  } else {
    pending_candidates_.push_back(candidate);
  }
}

const std::string& IceTransportChannel::name() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return name_;
}

bool IceTransportChannel::is_connected() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return callback_.is_null();
}

void IceTransportChannel::OnCandidateGathered(
    cricket::IceTransportInternal* ice_transport,
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnChannelCandidate(this, candidate);
}

void IceTransportChannel::OnRouteChange(
    cricket::IceTransportInternal* ice_transport,
    const cricket::Candidate& candidate) {
  // Ignore notifications if the channel is not writable.
  if (channel_->writable()) {
    NotifyRouteChanged();
  }
}

void IceTransportChannel::OnWritableState(
    rtc::PacketTransportInternal* transport) {
  DCHECK_EQ(transport,
            static_cast<rtc::PacketTransportInternal*>(channel_.get()));

  if (transport->writable()) {
    connect_attempts_left_ = network_settings_.ice_reconnect_attempts;
    reconnect_timer_.Stop();

    // Route change notifications are ignored when the |channel_| is not
    // writable. Notify the event handler about the current route once the
    // channel is writable.
    NotifyRouteChanged();
  } else {
    reconnect_timer_.Reset();
    TryReconnect();
  }
}

void IceTransportChannel::OnChannelDestroyed() {
  // The connection socket is being deleted, so delete the transport too.
  delete this;
}

void IceTransportChannel::NotifyRouteChanged() {
  TransportRoute route;

  DCHECK(channel_->best_connection());
  const cricket::Connection* connection = channel_->best_connection();

  // A connection has both a local and a remote candidate. For our purposes, the
  // route type is determined by the most indirect candidate type. For example:
  // it's possible for the local candidate be a "relay" type, while the remote
  // candidate is "local". In this case, we still want to report a RELAY route
  // type.
  static_assert(TransportRoute::DIRECT < TransportRoute::STUN &&
                    TransportRoute::STUN < TransportRoute::RELAY,
                "Route type enum values are ordered by 'indirectness'");
  route.type = std::max(
      CandidateTypeToTransportRouteType(connection->local_candidate()),
      CandidateTypeToTransportRouteType(connection->remote_candidate()));

  if (!webrtc::SocketAddressToIPEndPoint(
          connection->remote_candidate().address(), &route.remote_address)) {
    LOG(FATAL) << "Failed to convert peer IP address.";
  }

  const cricket::Candidate& local_candidate =
      channel_->best_connection()->local_candidate();
  if (!webrtc::SocketAddressToIPEndPoint(local_candidate.address(),
                                         &route.local_address)) {
    LOG(FATAL) << "Failed to convert local IP address.";
  }

  delegate_->OnChannelRouteChange(this, route);
}

void IceTransportChannel::TryReconnect() {
  DCHECK(!channel_->writable());

  if (connect_attempts_left_ <= 0) {
    reconnect_timer_.Stop();

    // Notify the caller that ICE connection has failed - normally that will
    // terminate Jingle connection (i.e. the transport will be destroyed).
    delegate_->OnChannelFailed(this);
    return;
  }
  --connect_attempts_left_;

  // Restart ICE by resetting ICE password.
  std::string ice_password = rtc::CreateRandomString(cricket::ICE_PWD_LENGTH);
  delegate_->OnChannelIceCredentials(this, ice_username_fragment_,
                                     ice_password);
  channel_->SetIceCredentials(ice_username_fragment_, ice_password);
}

}  // namespace remoting::protocol
