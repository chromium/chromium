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

bool FtlEchoMessageListener::OnSignalStrategyIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  const ftl::ChromotingMessage* request_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (!request_message || !request_message->has_echo() ||
      !request_message->echo().has_message()) {
    return false;
  }

  std::string sender_email;
  if (!sender_address.GetFtlSenderEmail(&sender_email)) {
    LOG(WARNING) << "Dropping echo message from non-FTL address "
                 << sender_address.id();
    return false;
  }

  // Only respond to echo messages from the machine owner.
  if (!check_access_permission_callback_.Run(sender_email)) {
    LOG(WARNING) << "Dropping echo message from " << sender_email;
    return false;
  }

  std::string request_message_payload(request_message->echo().message());
  HOST_LOG << "Handling echo message: '" << request_message_payload << "'";

  std::string response_message_payload =
      request_message_payload.substr(0, kMaxEchoMessageLength);
  ftl::ChromotingMessage response_message;
  response_message.mutable_echo()->set_message(response_message_payload);

  signal_strategy_->SendMessage(sender_address,
                                SignalingMessage{response_message});

  return true;
}

}  // namespace remoting
