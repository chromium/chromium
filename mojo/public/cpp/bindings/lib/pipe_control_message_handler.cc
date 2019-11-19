// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pipe_control_message_handler.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler_delegate.h"
#include "mojo/public/interfaces/bindings/pipe_control_messages.mojom.h"

namespace mojo {

PipeControlMessageHandler::PipeControlMessageHandler(
    PipeControlMessageHandlerDelegate* delegate)
    : delegate_(delegate) {}

PipeControlMessageHandler::~PipeControlMessageHandler() {}

void PipeControlMessageHandler::SetDescription(const std::string& description) {
  description_ = description;
}

// static
bool PipeControlMessageHandler::IsPipeControlMessage(const Message* message) {
  return !IsValidInterfaceId(message->interface_id());
}

bool PipeControlMessageHandler::Accept(Message* message) {
  if (!Validate(message))
    return false;

  if (message->name() == pipe_control::kRunOrClosePipeMessageId)
    return RunOrClosePipe(message);

  NOTREACHED();
  return false;
}

bool PipeControlMessageHandler::Validate(Message* message) {
  internal::ValidationContext validation_context(
      message->payload(), message->payload_num_bytes(), 0, 0, message,
      description_.c_str());

  if (message->name() == pipe_control::kRunOrClosePipeMessageId) {
    if (!internal::ValidateMessageIsRequestWithoutResponse(
            message, &validation_context)) {
      return false;
    }
    return internal::ValidateMessagePayload<
        pipe_control::internal::RunOrClosePipeMessageParams_Data>(
            message, &validation_context);
  }

  return false;
}

bool PipeControlMessageHandler::RunOrClosePipe(Message* message) {
  internal::SerializationContext context;
  pipe_control::internal::RunOrClosePipeMessageParams_Data* params =
      reinterpret_cast<
          pipe_control::internal::RunOrClosePipeMessageParams_Data*>(
          message->mutable_payload());
  pipe_control::RunOrClosePipeMessageParamsPtr params_ptr;
  internal::Deserialize<pipe_control::RunOrClosePipeMessageParamsDataView>(
      params, &params_ptr, &context);

  if (params_ptr->input->is_peer_associated_endpoint_closed_event()) {
    const auto& event =
        params_ptr->input->get_peer_associated_endpoint_closed_event();

    base::Optional<DisconnectReason> reason;
    if (event->disconnect_reason) {
      reason.emplace(event->disconnect_reason->custom_reason,
                     event->disconnect_reason->description);
    }
    return delegate_->OnPeerAssociatedEndpointClosed(event->id, reason);
  }

  DVLOG(1) << "Unsupported command in a RunOrClosePipe message pipe control "
           << "message. Closing the pipe.";
  return false;
}

}  // namespace mojo
