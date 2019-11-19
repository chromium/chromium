// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_errors.h"

#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {
namespace {

ValidationErrorObserverForTesting* g_validation_error_observer = nullptr;
SerializationWarningObserverForTesting* g_serialization_warning_observer =
    nullptr;
bool g_suppress_logging = false;

}  // namespace

const char* ValidationErrorToString(ValidationError error) {
  switch (error) {
    case VALIDATION_ERROR_NONE:
      return "VALIDATION_ERROR_NONE";
    case VALIDATION_ERROR_MISALIGNED_OBJECT:
      return "VALIDATION_ERROR_MISALIGNED_OBJECT";
    case VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE:
      return "VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE";
    case VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER:
      return "VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER";
    case VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER:
      return "VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER";
    case VALIDATION_ERROR_ILLEGAL_HANDLE:
      return "VALIDATION_ERROR_ILLEGAL_HANDLE";
    case VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE:
      return "VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE";
    case VALIDATION_ERROR_ILLEGAL_POINTER:
      return "VALIDATION_ERROR_ILLEGAL_POINTER";
    case VALIDATION_ERROR_UNEXPECTED_NULL_POINTER:
      return "VALIDATION_ERROR_UNEXPECTED_NULL_POINTER";
    case VALIDATION_ERROR_ILLEGAL_INTERFACE_ID:
      return "VALIDATION_ERROR_ILLEGAL_INTERFACE_ID";
    case VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID:
      return "VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID";
    case VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS:
      return "VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS";
    case VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID:
      return "VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID";
    case VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD:
      return "VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD";
    case VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP:
      return "VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP";
    case VALIDATION_ERROR_UNKNOWN_UNION_TAG:
      return "VALIDATION_ERROR_UNKNOWN_UNION_TAG";
    case VALIDATION_ERROR_UNKNOWN_ENUM_VALUE:
      return "VALIDATION_ERROR_UNKNOWN_ENUM_VALUE";
    case VALIDATION_ERROR_DESERIALIZATION_FAILED:
      return "VALIDATION_ERROR_DESERIALIZATION_FAILED";
    case VALIDATION_ERROR_MAX_RECURSION_DEPTH:
      return "VALIDATION_ERROR_MAX_RECURSION_DEPTH";
  }

  return "Unknown error";
}

void ReportValidationError(ValidationContext* context,
                           ValidationError error,
                           const char* description) {
  if (g_validation_error_observer) {
    g_validation_error_observer->set_last_error(error);
    return;
  }

  if (description) {
    if (!g_suppress_logging) {
      LOG(ERROR) << "Invalid message: " << ValidationErrorToString(error)
                 << " (" << description << ")";
    }
    if (context->message()) {
      context->message()->NotifyBadMessage(
          base::StringPrintf("Validation failed for %s [%s (%s)]",
                             context->GetFullDescription().c_str(),
                             ValidationErrorToString(error), description));
    }
  } else {
    if (!g_suppress_logging)
      LOG(ERROR) << "Invalid message: " << ValidationErrorToString(error);
    if (context->message()) {
      context->message()->NotifyBadMessage(
          base::StringPrintf("Validation failed for %s [%s]",
                             context->GetFullDescription().c_str(),
                             ValidationErrorToString(error)));
    }
  }
}

void ReportValidationErrorForMessage(mojo::Message* message,
                                     ValidationError error,
                                     const char* interface_name,
                                     unsigned int method_ordinal,
                                     bool is_response) {
  std::string description =
      base::StringPrintf("%s.%d %s", interface_name, method_ordinal,
                         is_response ? " response" : "");
  ValidationContext validation_context(nullptr, 0, 0, 0, message,
                                       description.c_str());
  ReportValidationError(&validation_context, error, description.c_str());
}

ScopedSuppressValidationErrorLoggingForTests
    ::ScopedSuppressValidationErrorLoggingForTests()
    : was_suppressed_(g_suppress_logging) {
  g_suppress_logging = true;
}

ScopedSuppressValidationErrorLoggingForTests
    ::~ScopedSuppressValidationErrorLoggingForTests() {
  g_suppress_logging = was_suppressed_;
}

ValidationErrorObserverForTesting::ValidationErrorObserverForTesting(
    base::RepeatingClosure callback)
    : last_error_(VALIDATION_ERROR_NONE), callback_(std::move(callback)) {
  DCHECK(!g_validation_error_observer);
  g_validation_error_observer = this;
}

ValidationErrorObserverForTesting::~ValidationErrorObserverForTesting() {
  DCHECK(g_validation_error_observer == this);
  g_validation_error_observer = nullptr;
}

bool ReportSerializationWarning(ValidationError error) {
  if (g_serialization_warning_observer) {
    g_serialization_warning_observer->set_last_warning(error);
    return true;
  }

  return false;
}

SerializationWarningObserverForTesting::SerializationWarningObserverForTesting()
    : last_warning_(VALIDATION_ERROR_NONE) {
  DCHECK(!g_serialization_warning_observer);
  g_serialization_warning_observer = this;
}

SerializationWarningObserverForTesting::
    ~SerializationWarningObserverForTesting() {
  DCHECK(g_serialization_warning_observer == this);
  g_serialization_warning_observer = nullptr;
}

}  // namespace internal
}  // namespace mojo
