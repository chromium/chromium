// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport.h"

#include <memory>

#include "base/functional/bind.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/channel_multiplexer.h"
#include "remoting/protocol/pseudotcp_channel_factory.h"
#include "remoting/protocol/secure_channel_factory.h"
#include "remoting/protocol/stream_channel_factory.h"
#include "remoting/protocol/stream_message_pipe_adapter.h"
#include "remoting/protocol/transport_context.h"

namespace remoting::protocol {

// Delay after candidate creation before sending transport-info message to
// accumulate multiple candidates. This is an optimization to reduce number of
// transport-info messages.
const int kTransportInfoSendDelayMs = 20;

// Name of the multiplexed channel.
static const char kMuxChannelName[] = "mux";

IceTransport::IceTransport(scoped_refptr<TransportContext> transport_context,
                           EventHandler* event_handler)
    : transport_context_(transport_context), event_handler_(event_handler) {}

IceTransport::~IceTransport() {
  channel_multiplexer_.reset();
  DCHECK(channels_.empty());
}

void IceTransport::Start(
    Authenticator* authenticator,
    SendTransportInfoCallback send_transport_info_callback) {
  DCHECK(!pseudotcp_channel_factory_);

  send_transport_info_callback_ = std::move(send_transport_info_callback);
  pseudotcp_channel_factory_ = std::make_unique<PseudoTcpChannelFactory>(this);
  secure_channel_factory_ = std::make_unique<SecureChannelFactory>(
      pseudotcp_channel_factory_.get(), authenticator);
  message_channel_factory_ =
      std::make_unique<StreamMessageChannelFactoryAdapter>(
          secure_channel_factory_.get(),
          base::BindRepeating(&IceTransport::OnChannelError,
                              weak_factory_.GetWeakPtr()));
}

bool IceTransport::ProcessTransportInfo(
    jingle_xmpp::XmlElement* transport_info_xml) {
  IceTransportInfo transport_info;
  if (!transport_info.ParseXml(transport_info_xml)) {
    return false;
  }

  for (auto it = transport_info.ice_credentials.begin();
       it != transport_info.ice_credentials.end(); ++it) {
    auto channel = channels_.find(it->channel);
    if (channel != channels_.end()) {
      channel->second->SetRemoteCredentials(it->ufrag, it->password);
    } else {
      // Transport info was received before the channel was created.
      // This could happen due to messages being reordered on the wire.
      pending_remote_ice_credentials_.push_back(*it);
    }
  }

  for (auto it = transport_info.candidates.begin();
       it != transport_info.candidates.end(); ++it) {
    auto channel = channels_.find(it->name);
    if (channel != channels_.end()) {
      channel->second->AddRemoteCandidate(it->candidate);
    } else {
      // Transport info was received before the channel was created.
      // This could happen due to messages being reordered on the wire.
      pending_remote_candidates_.push_back(*it);
    }
  }

  return true;
}

MessageChannelFactory* IceTransport::GetChannelFactory() {
  return message_channel_factory_.get();
}

MessageChannelFactory* IceTransport::GetMultiplexedChannelFactory() {
  if (!channel_multiplexer_) {
    channel_multiplexer_ = std::make_unique<ChannelMultiplexer>(
        secure_channel_factory_.get(), kMuxChannelName);
    mux_channel_factory_ = std::make_unique<StreamMessageChannelFactoryAdapter>(
        channel_multiplexer_.get(),
        base::BindRepeating(&IceTransport::OnChannelError,
                            weak_factory_.GetWeakPtr()));
  }
  return mux_channel_factory_.get();
}

void IceTransport::ApplyNetworkSettings(const NetworkSettings& settings) {
  DCHECK(!network_settings_);
  network_settings_ = std::make_unique<NetworkSettings>(settings);
  for (auto& [name, callback] : pending_channel_created_callbacks_) {
    CreateChannel(name, std::move(callback));
  }
  pending_channel_created_callbacks_.clear();
}

void IceTransport::CreateChannel(const std::string& name,
                                 ChannelCreatedCallback callback) {
  DCHECK(!channels_[name]);

  if (!network_settings_) {
    DCHECK(!pending_channel_created_callbacks_[name]);
    pending_channel_created_callbacks_[name] = std::move(callback);
    return;
  }

  auto channel = std::make_unique<IceTransportChannel>(transport_context_,
                                                       *network_settings_);
  channel->Connect(name, this, std::move(callback));
  AddPendingRemoteTransportInfo(channel.get());
  channels_[name] = channel.release();
}

void IceTransport::CancelChannelCreation(const std::string& name) {
  auto it = channels_.find(name);
  if (it != channels_.end()) {
    DCHECK(!it->second->is_connected());
    delete it->second;
    DCHECK(channels_.find(name) == channels_.end());
  }
}

void IceTransport::AddPendingRemoteTransportInfo(IceTransportChannel* channel) {
  auto credentials = pending_remote_ice_credentials_.begin();
  while (credentials != pending_remote_ice_credentials_.end()) {
    if (credentials->channel == channel->name()) {
      channel->SetRemoteCredentials(credentials->ufrag, credentials->password);
      credentials = pending_remote_ice_credentials_.erase(credentials);
    } else {
      ++credentials;
    }
  }

  auto candidate = pending_remote_candidates_.begin();
  while (candidate != pending_remote_candidates_.end()) {
    if (candidate->name == channel->name()) {
      channel->AddRemoteCandidate(candidate->candidate);
      candidate = pending_remote_candidates_.erase(candidate);
    } else {
      ++candidate;
    }
  }
}

void IceTransport::OnChannelIceCredentials(IceTransportChannel* channel,
                                           const std::string& ufrag,
                                           const std::string& password) {
  EnsurePendingTransportInfoMessage();
  pending_transport_info_message_->ice_credentials.push_back(
      IceTransportInfo::IceCredentials(channel->name(), ufrag, password));
}

void IceTransport::OnChannelCandidate(IceTransportChannel* channel,
                                      const cricket::Candidate& candidate) {
  EnsurePendingTransportInfoMessage();
  pending_transport_info_message_->candidates.push_back(
      IceTransportInfo::NamedCandidate(channel->name(), candidate));
}

void IceTransport::OnChannelRouteChange(IceTransportChannel* channel,
                                        const TransportRoute& route) {
  if (event_handler_) {
    event_handler_->OnIceTransportRouteChange(channel->name(), route);
  }
}

void IceTransport::OnChannelFailed(IceTransportChannel* channel) {
  event_handler_->OnIceTransportError(ErrorCode::CHANNEL_CONNECTION_ERROR);
}

void IceTransport::OnChannelDeleted(IceTransportChannel* channel) {
  auto it = channels_.find(channel->name());
  DCHECK_EQ(it->second, channel);
  channels_.erase(it);
}

void IceTransport::EnsurePendingTransportInfoMessage() {
  // |transport_info_timer_| must be running iff
  // |pending_transport_info_message_| exists.
  DCHECK_EQ(pending_transport_info_message_ != nullptr,
            transport_info_timer_.IsRunning());

  if (!pending_transport_info_message_) {
    pending_transport_info_message_ = std::make_unique<IceTransportInfo>();
    // Delay sending the new candidates in case we get more candidates
    // that we can send in one message.
    transport_info_timer_.Start(FROM_HERE,
                                base::Milliseconds(kTransportInfoSendDelayMs),
                                this, &IceTransport::SendTransportInfo);
  }
}

void IceTransport::SendTransportInfo() {
  DCHECK(pending_transport_info_message_);

  std::unique_ptr<jingle_xmpp::XmlElement> transport_info_xml =
      pending_transport_info_message_->ToXml();
  pending_transport_info_message_.reset();
  send_transport_info_callback_.Run(std::move(transport_info_xml));
}

void IceTransport::OnChannelError(int error) {
  LOG(ERROR) << "Data channel failed, error=" << error;
  event_handler_->OnIceTransportError(
      error ? ErrorCode::CHANNEL_CONNECTION_ERROR : ErrorCode::OK);
}

}  // namespace remoting::protocol
