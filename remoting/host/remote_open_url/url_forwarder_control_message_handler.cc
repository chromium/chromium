// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/url_forwarder_control_message_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/logging.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

// static
constexpr char UrlForwarderControlMessageHandler::kDataChannelName[];

UrlForwarderControlMessageHandler::UrlForwarderControlMessageHandler(
    std::unique_ptr<UrlForwarderConfigurator> url_forwarder_configurator,
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      url_forwarder_configurator_(std::move(url_forwarder_configurator)) {
  DCHECK_EQ(kDataChannelName, name);
}

UrlForwarderControlMessageHandler::~UrlForwarderControlMessageHandler() =
    default;

void UrlForwarderControlMessageHandler::OnConnected() {
  HOST_LOG << "Channel " << kDataChannelName << " is connected.";
}

void UrlForwarderControlMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  auto url_forwarder_config =
      protocol::ParseMessage<protocol::UrlForwarderControl>(message.get());
  if (url_forwarder_config->has_query_config_state_request()) {
    url_forwarder_configurator_->IsUrlForwarderSetUp(base::BindOnce(
        &UrlForwarderControlMessageHandler::OnIsUrlForwarderSetUpResult,
        weak_factory_.GetWeakPtr()));
  } else if (url_forwarder_config->has_set_up_url_forwarder_request()) {
    url_forwarder_configurator_->SetUpUrlForwarder(base::BindRepeating(
        &UrlForwarderControlMessageHandler::OnSetUpUrlForwarderResult,
        weak_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << "Unrecognized UrlForwarderControl message.";
  }
}

void UrlForwarderControlMessageHandler::OnDisconnecting() {
  HOST_LOG << "Channel " << kDataChannelName << " is disconnecting.";
}

void UrlForwarderControlMessageHandler::OnIsUrlForwarderSetUpResult(
    bool is_set_up) {
  protocol::UrlForwarderControl message;
  message.mutable_query_config_state_response()->set_is_url_forwarder_set_up(
      is_set_up);
  Send(message, base::DoNothing());
}

void UrlForwarderControlMessageHandler::OnSetUpUrlForwarderResult(
    protocol::UrlForwarderControl::SetUpUrlForwarderResponse::State state) {
  protocol::UrlForwarderControl message;
  message.mutable_set_up_url_forwarder_response()->set_state(state);
  Send(message, base::DoNothing());
}

}  // namespace remoting
