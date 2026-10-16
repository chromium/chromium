// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/agent_integration_control_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/logging.h"
#include "remoting/host/agent_integration_configurator.h"
#include "remoting/proto/agent_integration_control.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

// static
constexpr char AgentIntegrationControlMessageHandler::kDataChannelName[];

AgentIntegrationControlMessageHandler::AgentIntegrationControlMessageHandler(
    std::unique_ptr<AgentIntegrationConfigurator>
        agent_integration_configurator,
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      agent_integration_configurator_(
          std::move(agent_integration_configurator)) {
  DCHECK_EQ(kDataChannelName, name);
  DCHECK(agent_integration_configurator_);
}

AgentIntegrationControlMessageHandler::
    ~AgentIntegrationControlMessageHandler() = default;

void AgentIntegrationControlMessageHandler::OnConnected() {
  HOST_LOG << "Channel " << kDataChannelName << " is connected.";
}

void AgentIntegrationControlMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  auto control_message =
      protocol::ParseMessage<protocol::AgentIntegrationControl>(message.get());
  if (!control_message) {
    LOG(ERROR) << "Failed to parse AgentIntegrationControl message.";
    return;
  }
  if (control_message->has_query_state_request()) {
    agent_integration_configurator_->IsAgentIntegrationEnabled(
        base::BindOnce(&AgentIntegrationControlMessageHandler::
                           OnIsAgentIntegrationEnabledResult,
                       weak_factory_.GetWeakPtr()));
  } else if (control_message->has_set_state_request()) {
    if (!control_message->set_state_request().has_enabled()) {
      LOG(ERROR) << "SetStateRequest is missing the enabled field.";
      return;
    }
    bool enabled = control_message->set_state_request().enabled();
    agent_integration_configurator_->SetAgentIntegrationEnabled(
        enabled, base::BindOnce(&AgentIntegrationControlMessageHandler::
                                    OnSetAgentIntegrationEnabledResult,
                                weak_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << "Unrecognized AgentIntegrationControl message.";
  }
}

void AgentIntegrationControlMessageHandler::OnDisconnecting() {
  HOST_LOG << "Channel " << kDataChannelName << " is disconnecting.";
}

void AgentIntegrationControlMessageHandler::OnIsAgentIntegrationEnabledResult(
    bool enabled) {
  protocol::AgentIntegrationControl message;
  message.mutable_query_state_response()->set_enabled(enabled);
  Send(message, base::DoNothing());
}

void AgentIntegrationControlMessageHandler::OnSetAgentIntegrationEnabledResult(
    bool success) {
  protocol::AgentIntegrationControl message;
  message.mutable_set_state_response()->set_success(success);
  Send(message, base::DoNothing());
}

}  // namespace remoting
