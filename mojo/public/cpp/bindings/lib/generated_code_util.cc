// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/generated_code_util.h"

#include <cstring>

#include "mojo/public/cpp/bindings/lib/control_message_handler.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

namespace {

GenericValidationInfo FindGenericValidationInfo(
    uint32_t name,
    base::span<const std::pair<uint32_t, GenericValidationInfo>> info) {
  for (const auto& pair : info) {
    if (pair.first == name)
      return pair.second;
  }
  return {nullptr, nullptr};
}

GenericValidationInfo FindGenericValidationInfo(
    uint32_t name,
    base::span<const GenericValidationInfo> info) {
  if (name >= info.size())
    return {nullptr, nullptr};
  return info[name];
}

template <typename T>
bool ValidateRequestGenericT(Message* message,
                             const char* class_name,
                             base::span<const T> info) {
  if (!message->is_serialized() ||
      ControlMessageHandler::IsControlMessage(message)) {
    return true;
  }

  ValidationContext validation_context(message, class_name,
                                       ValidationContext::kRequestValidator);

  auto entry = FindGenericValidationInfo(message->header()->name, info);
  if (!entry.request_validator) {
    ReportValidationError(&validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD);
    return false;
  }

  const bool message_is_request =
      entry.response_validator ? ValidateMessageIsRequestExpectingResponse(
                                     message, &validation_context)
                               : ValidateMessageIsRequestWithoutResponse(
                                     message, &validation_context);
  if (!message_is_request)
    return false;

  return entry.request_validator(message->payload(), &validation_context);
}

template <typename T>
bool ValidateResponseGenericT(Message* message,
                              const char* class_name,
                              base::span<const T> info) {
  if (!message->is_serialized() ||
      ControlMessageHandler::IsControlMessage(message)) {
    return true;
  }

  ValidationContext validation_context(message, class_name,
                                       ValidationContext::kResponseValidator);

  if (!ValidateMessageIsResponse(message, &validation_context))
    return false;

  auto entry = FindGenericValidationInfo(message->header()->name, info);
  if (!entry.response_validator) {
    ReportValidationError(&validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD);
    return false;
  }

  return entry.response_validator(message->payload(), &validation_context);
}

}  // namespace

bool ValidateRequestGeneric(
    Message* message,
    const char* class_name,
    base::span<const std::pair<uint32_t, GenericValidationInfo>> info) {
  return ValidateRequestGenericT(message, class_name, info);
}

bool ValidateRequestGenericPacked(
    Message* message,
    const char* class_name,
    base::span<const GenericValidationInfo> info) {
  return ValidateRequestGenericT(message, class_name, info);
}

bool ValidateResponseGeneric(
    Message* message,
    const char* class_name,
    base::span<const std::pair<uint32_t, GenericValidationInfo>> info) {
  return ValidateResponseGenericT(message, class_name, info);
}

bool ValidateResponseGenericPacked(
    Message* message,
    const char* class_name,
    base::span<const GenericValidationInfo> info) {
  return ValidateResponseGenericT(message, class_name, info);
}

}  // namespace internal
}  // namespace mojo
