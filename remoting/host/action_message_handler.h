// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACTION_MESSAGE_HANDLER_H_
#define REMOTING_HOST_ACTION_MESSAGE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "remoting/proto/action.pb.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class ActionExecutor;

constexpr char kActionDataChannelPrefix[] = "actions";

class ActionMessageHandler : public protocol::NamedMessagePipeHandler {
 public:
  ActionMessageHandler(
      const std::string& name,
      const std::vector<protocol::ActionRequest::Action>& actions,
      std::unique_ptr<protocol::MessagePipe> pipe,
      std::unique_ptr<ActionExecutor> action_executor);

  ActionMessageHandler(const ActionMessageHandler&) = delete;
  ActionMessageHandler& operator=(const ActionMessageHandler&) = delete;

  ~ActionMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

 private:
  std::unique_ptr<ActionExecutor> action_executor_;

  // Populated via the negotiated capabilities between host and client.
  base::flat_set<protocol::ActionRequest::Action> supported_actions_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACTION_MESSAGE_HANDLER_H_
