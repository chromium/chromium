// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/control_message_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {
namespace {

bool ValidateControlRequestWithResponse(Message* message) {
  ValidationContext validation_context(message->payload(),
                                       message->payload_num_bytes(), 0, 0,
                                       message, "ControlRequestValidator");
  if (!ValidateMessageIsRequestExpectingResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunMessageId:
      return ValidateMessagePayload<
          interface_control::internal::RunMessageParams_Data>(
          message, &validation_context);
  }
  return false;
}

bool ValidateControlRequestWithoutResponse(Message* message) {
  ValidationContext validation_context(message->payload(),
                                       message->payload_num_bytes(), 0, 0,
                                       message, "ControlRequestValidator");
  if (!ValidateMessageIsRequestWithoutResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunOrClosePipeMessageId:
      return ValidateMessageIsRequestWithoutResponse(message,
                                                     &validation_context) &&
             ValidateMessagePayload<
                 interface_control::internal::RunOrClosePipeMessageParams_Data>(
                 message, &validation_context);
  }
  return false;
}

}  // namespace

// static
bool ControlMessageHandler::IsControlMessage(const Message* message) {
  return message->header()->name == interface_control::kRunMessageId ||
         message->header()->name == interface_control::kRunOrClosePipeMessageId;
}

ControlMessageHandler::ControlMessageHandler(InterfaceEndpointClient* owner,
                                             uint32_t interface_version)
    : owner_(owner), interface_version_(interface_version) {}

ControlMessageHandler::~ControlMessageHandler() {
}

bool ControlMessageHandler::Accept(Message* message) {
  if (!ValidateControlRequestWithoutResponse(message))
    return false;

  if (message->header()->name == interface_control::kRunOrClosePipeMessageId)
    return RunOrClosePipe(message);

  NOTREACHED();
  return false;
}

bool ControlMessageHandler::AcceptWithResponder(
    Message* message,
    std::unique_ptr<MessageReceiverWithStatus> responder) {
  if (!ValidateControlRequestWithResponse(message))
    return false;

  if (message->header()->name == interface_control::kRunMessageId)
    return Run(message, std::move(responder));

  NOTREACHED();
  return false;
}

bool ControlMessageHandler::Run(
    Message* message,
    std::unique_ptr<MessageReceiverWithStatus> responder) {
  interface_control::internal::RunMessageParams_Data* params =
      reinterpret_cast<interface_control::internal::RunMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunMessageParamsPtr params_ptr;
  Deserialize<interface_control::RunMessageParamsDataView>(params, &params_ptr,
                                                           &context_);
  auto& input = *params_ptr->input;
  interface_control::RunOutputPtr output = interface_control::RunOutput::New();
  if (input.is_query_version()) {
    output->set_query_version_result(
        interface_control::QueryVersionResult::New());
    output->get_query_version_result()->version = interface_version_;
  } else if (input.is_flush_for_testing()) {
    output.reset();
  } else {
    output.reset();
  }

  auto response_params_ptr = interface_control::RunResponseMessageParams::New();
  response_params_ptr->output = std::move(output);
  Message response_message(interface_control::kRunMessageId,
                           Message::kFlagIsResponse, 0, 0, nullptr);
  response_message.set_request_id(message->request_id());
  interface_control::internal::RunResponseMessageParams_Data::BufferWriter
      response_params;
  Serialize<interface_control::RunResponseMessageParamsDataView>(
      response_params_ptr, response_message.payload_buffer(), &response_params,
      &context_);
  ignore_result(responder->Accept(&response_message));
  return true;
}

bool ControlMessageHandler::RunOrClosePipe(Message* message) {
  interface_control::internal::RunOrClosePipeMessageParams_Data* params =
      reinterpret_cast<
          interface_control::internal::RunOrClosePipeMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunOrClosePipeMessageParamsPtr params_ptr;
  Deserialize<interface_control::RunOrClosePipeMessageParamsDataView>(
      params, &params_ptr, &context_);
  auto& input = *params_ptr->input;
  if (input.is_require_version())
    return interface_version_ >= input.get_require_version()->version;
  if (input.is_enable_idle_tracking()) {
    return owner_->AcceptEnableIdleTracking(base::TimeDelta::FromMicroseconds(
        input.get_enable_idle_tracking()->timeout_in_microseconds));
  }
  if (input.is_message_ack())
    return owner_->AcceptMessageAck();
  if (input.is_notify_idle())
    return owner_->AcceptNotifyIdle();
  return false;
}

}  // namespace internal
}  // namespace mojo
