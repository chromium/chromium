// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_ERRORS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_ERRORS_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/logging.h"

namespace mojo {

class Message;

namespace internal {

class ValidationContext;

enum ValidationError {
  // There is no validation error.
  VALIDATION_ERROR_NONE,
  // An object (struct or array) is not 8-byte aligned.
  VALIDATION_ERROR_MISALIGNED_OBJECT,
  // An object is not contained inside the message data, or it overlaps other
  // objects.
  VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE,
  // A struct header doesn't make sense, for example:
  // - |num_bytes| is smaller than the size of the struct header.
  // - |num_bytes| and |version| don't match.
  // TODO(yzshen): Consider splitting it into two different error codes. Because
  // the former indicates someone is misbehaving badly whereas the latter could
  // be due to an inappropriately-modified .mojom file.
  VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER,
  // An array header doesn't make sense, for example:
  // - |num_bytes| is smaller than the size of the header plus the size required
  // to store |num_elements| elements.
  // - For fixed-size arrays, |num_elements| is different than the specified
  // size.
  VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
  // An encoded handle is illegal.
  VALIDATION_ERROR_ILLEGAL_HANDLE,
  // A non-nullable handle field is set to invalid handle.
  VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
  // An encoded pointer is illegal.
  VALIDATION_ERROR_ILLEGAL_POINTER,
  // A non-nullable pointer field is set to null.
  VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
  // An interface ID is illegal.
  VALIDATION_ERROR_ILLEGAL_INTERFACE_ID,
  // A non-nullable interface ID field is set to invalid.
  VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
  // |flags| in the message header is invalid. The flags are either
  // inconsistent with one another, inconsistent with other parts of the
  // message, or unexpected for the message receiver.  For example the
  // receiver is expecting a request message but the flags indicate that
  // the message is a response message.
  VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS,
  // |flags| in the message header indicates that a request ID is required but
  // there isn't one.
  VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID,
  // The |name| field in a message header contains an unexpected value.
  VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD,
  // Two parallel arrays which are supposed to represent a map have different
  // lengths.
  VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP,
  // Attempted to deserialize a tagged union with an unknown tag.
  VALIDATION_ERROR_UNKNOWN_UNION_TAG,
  // A value of a non-extensible enum type is unknown.
  VALIDATION_ERROR_UNKNOWN_ENUM_VALUE,
  // Message deserialization failure, for example due to rejection by custom
  // validation logic.
  VALIDATION_ERROR_DESERIALIZATION_FAILED,
  // The message contains a too deeply nested value, for example a recursively
  // defined field which runtime value is too large.
  VALIDATION_ERROR_MAX_RECURSION_DEPTH,
};

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
const char* ValidationErrorToString(ValidationError error);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void ReportValidationError(ValidationContext* context,
                           ValidationError error,
                           const char* description = nullptr);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void ReportValidationErrorForMessage(mojo::Message* message,
                                     ValidationError error,
                                     const char* interface_name,
                                     unsigned int method_ordinal,
                                     bool is_response);

// This class may be used by tests to suppress validation error logging. This is
// not thread-safe and must only be instantiated on the main thread with no
// other threads using Mojo bindings at the time of construction or destruction.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
    ScopedSuppressValidationErrorLoggingForTests {
 public:
  ScopedSuppressValidationErrorLoggingForTests();

  ScopedSuppressValidationErrorLoggingForTests(
      const ScopedSuppressValidationErrorLoggingForTests&) = delete;
  ScopedSuppressValidationErrorLoggingForTests& operator=(
      const ScopedSuppressValidationErrorLoggingForTests&) = delete;

  ~ScopedSuppressValidationErrorLoggingForTests();

 private:
  const bool was_suppressed_;
};

// Only used by validation tests and when there is only one thread doing message
// validation.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
    ValidationErrorObserverForTesting {
 public:
  explicit ValidationErrorObserverForTesting(base::RepeatingClosure callback);

  ValidationErrorObserverForTesting(const ValidationErrorObserverForTesting&) =
      delete;
  ValidationErrorObserverForTesting& operator=(
      const ValidationErrorObserverForTesting&) = delete;

  ~ValidationErrorObserverForTesting();

  ValidationError last_error() const { return last_error_; }
  void set_last_error(ValidationError error) {
    last_error_ = error;
    callback_.Run();
  }

 private:
  ValidationError last_error_;
  base::RepeatingClosure callback_;
};

// Used only by MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING. Don't use it directly.
//
// The function returns true if the error is recorded (by a
// SerializationWarningObserverForTesting object), false otherwise.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ReportSerializationWarning(ValidationError error);

// Only used by serialization tests and when there is only one thread doing
// message serialization.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
    SerializationWarningObserverForTesting {
 public:
  SerializationWarningObserverForTesting();

  SerializationWarningObserverForTesting(
      const SerializationWarningObserverForTesting&) = delete;
  SerializationWarningObserverForTesting& operator=(
      const SerializationWarningObserverForTesting&) = delete;

  ~SerializationWarningObserverForTesting();

  ValidationError last_warning() const { return last_warning_; }
  void set_last_warning(ValidationError error) { last_warning_ = error; }

 private:
  ValidationError last_warning_;
};

// Used to record that Deserialize() of a Mojo string failed because it was not
// valid UTF-8.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void RecordInvalidStringDeserialization();

}  // namespace internal
}  // namespace mojo

// In debug build, logs a serialization warning if |condition| evaluates to
// true:
//   - if there is a SerializationWarningObserverForTesting object alive,
//     records |error| in it;
//   - otherwise, logs a fatal-level message.
// |error| is the validation error that will be triggered by the receiver
// of the serialzation result.
//
// In non-debug build, does nothing (not even compiling |condition|).
#define MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(condition, error,    \
                                                 description)         \
  DLOG_IF(FATAL, (condition) && !ReportSerializationWarning(error))   \
      << "The outgoing message will trigger "                         \
      << ValidationErrorToString(error) << " at the receiving side (" \
      << description << ")."

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_ERRORS_H_
