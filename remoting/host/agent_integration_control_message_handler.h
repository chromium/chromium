// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AGENT_INTEGRATION_CONTROL_MESSAGE_HANDLER_H_
#define REMOTING_HOST_AGENT_INTEGRATION_CONTROL_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class AgentIntegrationConfigurator;

// Message handler for the agent-integration-control data channel.
class AgentIntegrationControlMessageHandler final
    : public protocol::NamedMessagePipeHandler {
 public:
  static constexpr char kDataChannelName[] = "agent-integration-control";

  AgentIntegrationControlMessageHandler(
      std::unique_ptr<AgentIntegrationConfigurator>
          agent_integration_configurator,
      const std::string& name,
      std::unique_ptr<protocol::MessagePipe> pipe);
  ~AgentIntegrationControlMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  AgentIntegrationControlMessageHandler(
      const AgentIntegrationControlMessageHandler&) = delete;
  AgentIntegrationControlMessageHandler& operator=(
      const AgentIntegrationControlMessageHandler&) = delete;

 private:
  void OnIsAgentIntegrationEnabledResult(bool enabled);
  void OnSetAgentIntegrationEnabledResult(bool success);

  std::unique_ptr<AgentIntegrationConfigurator> agent_integration_configurator_;

  base::WeakPtrFactory<AgentIntegrationControlMessageHandler> weak_factory_{
      this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_AGENT_INTEGRATION_CONTROL_MESSAGE_HANDLER_H_
