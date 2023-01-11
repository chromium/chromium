// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/action_message_handler.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/host/action_executor.h"
#include "remoting/proto/action.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

using protocol::ActionResponse;

ActionMessageHandler::ActionMessageHandler(
    const std::string& name,
    const std::vector<protocol::ActionRequest::Action>& actions,
    std::unique_ptr<protocol::MessagePipe> pipe,
    std::unique_ptr<ActionExecutor> action_executor)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      action_executor_(std::move(action_executor)),
      supported_actions_(actions) {
  DCHECK(action_executor_);
}

ActionMessageHandler::~ActionMessageHandler() = default;

void ActionMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK(message);
  std::unique_ptr<protocol::ActionRequest> request =
      protocol::ParseMessage<protocol::ActionRequest>(message.get());

  ActionResponse response;
  response.set_request_id(request ? request->request_id() : 0);

  if (!request) {
    response.set_code(ActionResponse::PROTOCOL_ERROR);
    response.set_protocol_error_type(ActionResponse::INVALID_MESSAGE_ERROR);
  } else if (!request->has_action()) {
    // |has_action()| will return false if either the field is not set or the
    // value is out of range.  Unfortunately we can't distinguish between these
    // two conditions so we return the same error for both.
    response.set_code(ActionResponse::PROTOCOL_ERROR);
    response.set_protocol_error_type(ActionResponse::INVALID_ACTION_ERROR);
  } else if (supported_actions_.count(request->action()) == 0) {
    // We received an action which is valid, but not supported by this platform
    // or connection mode.
    response.set_code(ActionResponse::PROTOCOL_ERROR);
    response.set_protocol_error_type(ActionResponse::UNSUPPORTED_ACTION_ERROR);
  } else {
    // Valid action request received.  None of the supported actions at this
    // time support return codes, if we add actions in the future which could
    // fail in an observable way, we should consider returning that info to the
    // client.
    action_executor_->ExecuteAction(*request);
    response.set_code(ActionResponse::ACTION_SUCCESS);
  }

  Send(response, base::DoNothing());
}

}  // namespace remoting
