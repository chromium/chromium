// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_echo_message_listener.h"

#include <string>

#include "base/logging.h"
#include "remoting/base/logging.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/signaling_address.h"

namespace {
constexpr int kMaxEchoMessageLength = 16;
}

namespace remoting {

FtlEchoMessageListener::FtlEchoMessageListener(
    CheckAccessPermissionCallback check_access_permission_callback,
    SignalStrategy* signal_strategy)
    : check_access_permission_callback_(check_access_permission_callback),
      signal_strategy_(signal_strategy) {
  DCHECK(signal_strategy_);
  signal_strategy_->AddListener(this);
}

FtlEchoMessageListener::~FtlEchoMessageListener() {
  signal_strategy_->RemoveListener(this);
}

void FtlEchoMessageListener::OnSignalStrategyStateChange(
    SignalStrategy::State state) {}

bool FtlEchoMessageListener::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

bool FtlEchoMessageListener::OnSignalStrategyIncomingMessage(
    const ftl::Id& sender_id,
    const std::string& sender_registration_id,
    const ftl::ChromotingMessage& request_message) {
  if (!request_message.has_echo() || !request_message.echo().has_message()) {
    return false;
  }

  // Only respond to echo messages from the machine owner.
  if (sender_id.type() != ftl::IdType_Type_EMAIL ||
      !check_access_permission_callback_.Run(sender_id.id())) {
    LOG(WARNING) << "Dropping echo message from " << sender_id.id();
    return false;
  }

  std::string request_message_payload(request_message.echo().message());
  HOST_LOG << "Handling echo message: '" << request_message_payload << "'";

  std::string response_message_payload =
      request_message_payload.substr(0, kMaxEchoMessageLength);
  ftl::ChromotingMessage response_message;
  response_message.mutable_echo()->set_message(response_message_payload);

  signal_strategy_->SendMessage(SignalingAddress::CreateFtlSignalingAddress(
                                    sender_id.id(), sender_registration_id),
                                response_message);

  return true;
}

}  // namespace remoting
