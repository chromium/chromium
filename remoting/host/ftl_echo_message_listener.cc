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
    FtlSignalStrategy* ftl_signal_strategy)
    : check_access_permission_callback_(check_access_permission_callback),
      ftl_signal_strategy_(ftl_signal_strategy) {
  DCHECK(ftl_signal_strategy_);
  ftl_signal_strategy_->AddFtlListener(this);
}

FtlEchoMessageListener::~FtlEchoMessageListener() {
  ftl_signal_strategy_->RemoveFtlListener(this);
}

bool FtlEchoMessageListener::OnIncomingFtlMessage(
    const SignalingAddress& sender_address,
    const ftl::ChromotingMessage& message) {
  if (!message.has_echo() || !message.echo().has_message()) {
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

  std::string request_message_payload(message.echo().message());
  HOST_LOG << "Handling echo message: '" << request_message_payload << "'";

  std::string response_message_payload =
      request_message_payload.substr(0, kMaxEchoMessageLength);
  ftl::ChromotingMessage response_message;
  response_message.mutable_echo()->set_message(response_message_payload);

  ftl_signal_strategy_->SendFtlMessage(sender_address,
                                       std::move(response_message));

  return true;
}

}  // namespace remoting
