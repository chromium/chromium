// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: status.h
// -----------------------------------------------------------------------------
//
// This header file defines the Abseil `status` library, consisting of:
//
//   * An `absl::Status` class for holding error handling information
//   * A set of canonical `absl::StatusCode` error codes, and associated
//     utilities for generating and propogating status codes.
//   * A set of helper functions for creating status codes and checking their
//     values
//
// Within Google, `absl::Status` is the primary mechanism for indicating
// recoverable errors across API boundaries (and in particular across RPC
// boundaries). Most functions which can produce a recoverable error should
// be designed to return an `absl::Status` (or `absl::StatusOr`).
//
// Example:
//
// absl::Status myFunction(absl::string_view fname, ...) {
//   ...
//   // encounter error
//   if (error condition) {
//     return absl::InvalidArgumentError("bad mode");
//   }
//   // else, return OK
//   return absl::OkStatus();
// }
//
// An `absl::Status` is designed to either return "OK" or one of a number of
// different error codes, corresponding to typical error conditions.
// In almost all cases, when using `absl::Status` you should use the canonical
// error codes (of type `absl::StatusCode`) enumerated in this header file.
// These canonical codes are understood across the codebase and will be
// accepted across all API and RPC boundaries.
//
// An `absl::Status` can optionally include a payload with more information
// about the error. Typically, this payload serves one of several purposes:
//
//   * It may provide more fine-grained semantic information about the error to
//     facilitate actionable remedies.
//   * It may provide human-readable contexual information that is more
//     appropriate
//     to display to an end user.
//
#ifndef ABSL_STATUS_STATUS_H_
#define ABSL_STATUS_STATUS_H_

#include <iostream>
#include <string>

#include "absl/container/inlined_vector.h"
#include "absl/status/internal/status_internal.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// absl::StatusCode
//
// An `absl::StatusCode` is an enumerated type indicating either no error ("OK")
// or an error condition. In most cases, an `absl::Status` indicates a
// recoverable error, and the purpose of signalling an error is to indicate what
// action to take in response to that error. These error codes map to the proto
// RPC error codes indicated in https://cloud.google.com/apis/design/errors.
//
// The errors listed below are the canonical errors associated with
// `absl::Status` and are used throughout the codebase. As a result, these
// error codes are somewhat generic.
//
// In general, try to return the most specific error that applies if more than
// one error may pertain. For example, prefer `kOutOfRange` over
// `kFailedPrecondition` if both codes apply. Similarly prefer `kNotFound` or
// `kAlreadyExists` over `kFailedPrecondition`.
//
// Because these errors may travel RPC boundaries, these codes are tied to the
// `google.rpc.Code` definitions within
// https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto
// The string value of these RPC codes is denoted within each enum below.
//
// If your error handling code requires more context, you can attach payloads
// to your status. See `absl::Status::SetPayload()` and
// `absl::Status::GetPayload()` below.
enum class StatusCode : int {
  // StatusCode::kOk
  //
  // kOK (gRPC code "OK") does not indicate an error; this value is returned on
  // success. It is typical to check for this value before proceeding on any
  // given call across an API or RPC boundary. To check this value, use the
  // `absl::Status::ok()` member function rather than inspecting the raw code.
  kOk = 0,

  // StatusCode::kCancelled
  //
  // kCanelled (gRPC code "CANCELLED") indicates the operation was cancelled,
  // typically by the caller.
  kCancelled = 1,

  // StatusCode::kUnknown
  //
  // kUnknown (gRPC code "UNKNOWN") indicates an unknown error occurred. In
  // general, more specific errors should be raised, if possible. Errors raised
  // by APIs that do not return enough error information may be converted to
  // this error.
  kUnknown = 2,

  // StatusCode::kInvalidArgument
  //
  // kInvalidArgument (gRPC code "INVALID_ARGUMENT") indicates the caller
  // specified an invalid argument, such a malformed filename. Note that such
  // errors should be narrowly limited to indicate to the invalid nature of the
  // arguments themselves. Errors with validly formed arguments that may cause
  // errors with the state of the receiving system should be denoted with
  // `kFailedPrecondition` instead.
  kInvalidArgument = 3,

  // StatusCode::kDeadlineExceeded
  //
  // kDeadlineExceeded (gRPC code "DEADLINE_EXCEEDED") indicates a deadline
  // expired before the operation could complete. For operations that may change
  // state within a system, this error may be returned even if the operation has
  // completed successfully. For example, a successful response from a server
  // could have been delayed long enough for the deadline to expire.
  kDeadlineExceeded = 4,

  // StatusCode::kNotFound
  //
  // kNotFound (gRPC code "NOT_FOUND") indicates some requested entity (such as
  // a file or directory) was not found.
  //
  // `kNotFound` is useful if a request should be denied for an entire class of
  // users, such as during a gradual feature rollout or undocumented allow list.
  // If, instead, a request should be denied for specific sets of users, such as
  // through user-based access control, use `kPermissionDenied` instead.
  kNotFound = 5,

  // StatusCode::kAlreadyExists
  //
  // kAlreadyExists (gRPC code "ALREADY_EXISTS") indicates the entity that a
  // caller attempted to create (such as file or directory) is already present.
  kAlreadyExists = 6,

  // StatusCode::kPermissionDenied
  //
  // kPermissionDenied (gRPC code "PERMISSION_DENIED") indicates that the caller
  // does not have permission to execute the specified operation. Note that this
  // error is different than an error due to an *un*authenticated user. This
  // error code does not imply the request is valid or the requested entity
  // exists or satisfies any other pre-conditions.
  //
  // `kPermissionDenied` must not be used for rejections caused by exhausting
  // some resource. Instead, use `kResourceExhausted` for those errors.
  // `kPermissionDenied` must not be used if the caller cannot be identified.
  // Instead, use `kUnauthenticated` for those errors.
  kPermissionDenied = 7,

  // StatusCode::kResourceExhausted
  //
  // kResourceExhausted (gRPC code "RESOURCE_EXHAUSTED") indicates some resource
  // has been exhausted, perhaps a per-user quota, or perhaps the entire file
  // system is out of space.
  kResourceExhausted = 8,

  // StatusCode::kFailedPrecondition
  //
  // kFailedPrecondition (gRPC code "FAILED_PRECONDITION") indicates that the
  // operation was rejected because the system is not in a state required for
  // the operation's execution. For example, a directory to be deleted may be
  // non-empty, an "rmdir" operation is applied to a non-directory, etc.
  //
  // Some guidelines that may help a service implementer in deciding between
  // `kFailedPrecondition`, `kAborted`, and `kUnavailable`:
  //
  //  (a) Use `kUnavailable` if the client can retry just the failing call.
  //  (b) Use `kAborted` if the client should retry at a higher transaction
  //      level (such as when a client-specified test-and-set fails, indicating
  //      the client should restart a read-modify-write sequence).
  //  (c) Use `kFailedPrecondition` if the client should not retry until
  //      the system state has been explicitly fixed. For example, if an "rmdir"
  //      fails because the directory is non-empty, `kFailedPrecondition`
  //      should be returned since the client should not retry unless
  //      the files are deleted from the directory.
  kFailedPrecondition = 9,

  // StatusCode::kAborted
  //
  // kAborted (gRPC code "ABORTED") indicates the operation was aborted,
  // typically due to a concurrency issue such as a sequencer check failure or a
  // failed transaction.
  //
  // See the guidelines above for deciding between `kFailedPrecondition`,
  // `kAborted`, and `kUnavailable`.
  kAborted = 10,

  // StatusCode::kOutofRange
  //
  // kOutofRange (gRPC code "OUT_OF_RANGE") indicates the operation was
  // attempted past the valid range, such as seeking or reading past an
  // end-of-file.
  //
  // Unlike `kInvalidArgument`, this error indicates a problem that may
  // be fixed if the system state changes. For example, a 32-bit file
  // system will generate `kInvalidArgument` if asked to read at an
  // offset that is not in the range [0,2^32-1], but it will generate
  // `kOutOfRange` if asked to read from an offset past the current
  // file size.
  //
  // There is a fair bit of overlap between `kFailedPrecondition` and
  // `kOutOfRange`.  We recommend using `kOutOfRange` (the more specific
  // error) when it applies so that callers who are iterating through
  // a space can easily look for an `kOutOfRange` error to detect when
  // they are done.
  kOutOfRange = 11,

  // StatusCode::kUnimplemented
  //
  // kUnimplemented (gRPC code "UNIMPLEMENTED") indicates the operation is not
  // implemented or supported in this service. In this case, the operation
  // should not be re-attempted.
  kUnimplemented = 12,

  // StatusCode::kInternal
  //
  // kInternal (gRPC code "INTERNAL") indicates an internal error has occurred
  // and some invariants expected by the underlying system have not been
  // satisfied. This error code is reserved for serious errors.
  kInternal = 13,

  // StatusCode::kUnavailable
  //
  // kUnavailable (gRPC code "UNAVAILABLE") indicates the service is currently
  // unavailable and that this is most likely a transient condition. An error
  // such as this can be corrected by retrying with a backoff scheme. Note that
  // it is not always safe to retry non-idempotent operations.
  //
  // See the guidelines above for deciding between `kFailedPrecondition`,
  // `kAborted`, and `kUnavailable`.
  kUnavailable = 14,

  // StatusCode::kDataLoss
  //
  // kDataLoss (gRPC code "DATA_LOSS") indicates that unrecoverable data loss or
  // corruption has occurred. As this error is serious, proper alerting should
  // be attached to errors such as this.
  kDataLoss = 15,

  // StatusCode::kUnauthenticated
  //
  // kUnauthenticated (gRPC code "UNAUTHENTICATED") indicates that the request
  // does not have valid authentication credentials for the operation. Correct
  // the authentication and try again.
  kUnauthenticated = 16,

  // StatusCode::DoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_
  //
  // NOTE: this error code entry should not be used and you should not rely on
  // its value, which may change.
  //
  // The purpose of this enumerated value is to force people who handle status
  // codes with `switch()` statements to *not* simply enumerate all possible
  // values, but instead provide a "default:" case. Providing such a default
  // case ensures that code will compile when new codes are added.
  kDoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_ = 20
};

// StatusCodeToString()
//
// Returns the name for the status code, or "" if it is an unknown value.
std::string StatusCodeToString(StatusCode code);

// operator<<
//
// Streams StatusCodeToString(code) to `os`.
std::ostream& operator<<(std::ostream& os, StatusCode code);

// absl::Status
//
class ABSL_MUST_USE_RESULT Status final {
 public:
  // Constructors

  // Creates an OK status with no message or payload.
  Status();

  // Create a status in the canonical error space with the specified code and
  // error message.  If `code == absl::StatusCode::kOk`, `msg` is ignored and an
  // object identical to an OK status is constructed.
  //
  // `msg` must be in UTF-8. The implementation may complain (e.g.,
  // by printing a warning) if it is not.
  Status(absl::StatusCode code, absl::string_view msg);

  Status(const Status&);
  Status& operator=(const Status& x);

  // Move operators

  // The moved-from state is valid but unspecified.
  Status(Status&&) noexcept;
  Status& operator=(Status&&);

  ~Status();

  // Status::Update()
  //
  // If `this->ok()`, stores `new_status` into *this. If `!this->ok()`,
  // preserves the current data. May, in the future, augment the current status
  // with additional information about `new_status`.
  //
  // Convenient way of keeping track of the first error encountered.
  // Instead of:
  //   if (overall_status.ok()) overall_status = new_status
  // Use:
  //   overall_status.Update(new_status);
  //
  // Style guide exception for rvalue reference granted in CL 153567220.
  void Update(const Status& new_status);
  void Update(Status&& new_status);

  // Status::ok()
  //
  // Returns true if the Status is OK.
  ABSL_MUST_USE_RESULT bool ok() const;

  // Status::code()
  //
  // Returns the (canonical) error code.
  absl::StatusCode code() const;

  // Status::raw_code()
  //
  // Returns the raw (canonical) error code which could be out of the range of
  // the local `absl::StatusCode` enum. NOTE: This should only be called when
  // converting to wire format. Use `code` for error handling.
  int raw_code() const;

  // Status::message()
  //
  // Returns the error message.  Note: prefer ToString() for debug logging.
  // This message rarely describes the error code.  It is not unusual for the
  // error message to be the empty string.
  absl::string_view message() const;

  friend bool operator==(const Status&, const Status&);
  friend bool operator!=(const Status&, const Status&);

  // Status::ToString()
  //
  // Returns a combination of the error code name, the message and the payloads.
  // You can expect the code name and the message to be substrings of the
  // result, and the payloads to be printed by the registered printer extensions
  // if they are recognized.
  // WARNING: Do not depend on the exact format of the result of `ToString()`
  // which is subject to change.
  std::string ToString() const;

  // Status::IgnoreError()
  //
  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const;

  // swap()
  //
  // Swap the contents of `a` with `b`
  friend void swap(Status& a, Status& b);

  //----------------------------------------------------------------------------
  // Payload management APIs
  //----------------------------------------------------------------------------

  // Type URL should be unique and follow the naming convention below:
  // The idea of type URL comes from `google.protobuf.Any`
  // (https://developers.google.com/protocol-buffers/docs/proto3#any). The
  // type URL should be globally unique and follow the format of URL
  // (https://en.wikipedia.org/wiki/URL). The default type URL for a given
  // protobuf message type is "type.googleapis.com/packagename.messagename". For
  // other custom wire formats, users should define the format of type URL in a
  // similar practice so as to minimize the chance of conflict between type
  // URLs. Users should make sure that the type URL can be mapped to a concrete
  // C++ type if they want to deserialize the payload and read it effectively.

  // Status::GetPayload()
  //
  // Gets the payload based for `type_url` key, if it is present.
  absl::optional<absl::Cord> GetPayload(absl::string_view type_url) const;

  // Status::SetPayload()
  //
  // Sets the payload for `type_url` key for a non-ok status, overwriting any
  // existing payload for `type_url`.
  //
  // NOTE: Does nothing if the Status is ok.
  void SetPayload(absl::string_view type_url, absl::Cord payload);

  // Status::ErasePayload()
  //
  // Erases the payload corresponding to the `type_url` key.  Returns true if
  // the payload was present.
  bool ErasePayload(absl::string_view type_url);

  // Status::ForEachPayload()
  //
  // Iterates over the stored payloads and calls `visitor(type_key, payload)`
  // for each one.
  //
  // NOTE: The order of calls to `visitor` is not specified and may change at
  // any time.
  //
  // NOTE: Any mutation on the same 'Status' object during visitation is
  // forbidden and could result in undefined behavior.
  void ForEachPayload(
      const std::function<void(absl::string_view, const absl::Cord&)>& visitor)
      const;

 private:
  friend Status CancelledError();

  // Creates a status in the canonical error space with the specified
  // code, and an empty error message.
  explicit Status(absl::StatusCode code);

  static void UnrefNonInlined(uintptr_t rep);
  static void Ref(uintptr_t rep);
  static void Unref(uintptr_t rep);

  // REQUIRES: !ok()
  // Ensures rep_ is not shared with any other Status.
  void PrepareToModify();

  const status_internal::Payloads* GetPayloads() const;
  status_internal::Payloads* GetPayloads();

  // Takes ownership of payload.
  static uintptr_t NewRep(absl::StatusCode code, absl::string_view msg,
                          std::unique_ptr<status_internal::Payloads> payload);
  static bool EqualsSlow(const absl::Status& a, const absl::Status& b);

  // MSVC 14.0 limitation requires the const.
  static constexpr const char kMovedFromString[] =
      "Status accessed after move.";

  static const std::string* EmptyString();
  static const std::string* MovedFromString();

  // Returns whether rep contains an inlined representation.
  // See rep_ for details.
  static bool IsInlined(uintptr_t rep);

  // Indicates whether this Status was the rhs of a move operation. See rep_
  // for details.
  static bool IsMovedFrom(uintptr_t rep);
  static uintptr_t MovedFromRep();

  // Convert between error::Code and the inlined uintptr_t representation used
  // by rep_. See rep_ for details.
  static uintptr_t CodeToInlinedRep(absl::StatusCode code);
  static absl::StatusCode InlinedRepToCode(uintptr_t rep);

  // Converts between StatusRep* and the external uintptr_t representation used
  // by rep_. See rep_ for details.
  static uintptr_t PointerToRep(status_internal::StatusRep* r);
  static status_internal::StatusRep* RepToPointer(uintptr_t r);

  // Returns string for non-ok Status.
  std::string ToStringSlow() const;

  // Status supports two different representations.
  //  - When the low bit is off it is an inlined representation.
  //    It uses the canonical error space, no message or payload.
  //    The error code is (rep_ >> 2).
  //    The (rep_ & 2) bit is the "moved from" indicator, used in IsMovedFrom().
  //  - When the low bit is on it is an external representation.
  //    In this case all the data comes from a heap allocated Rep object.
  //    (rep_ - 1) is a status_internal::StatusRep* pointer to that structure.
  uintptr_t rep_;
};

// OkStatus()
//
// Returns an OK status, equivalent to a default constructed instance.
Status OkStatus();

// operator<<()
//
// Prints a human-readable representation of `x` to `os`.
std::ostream& operator<<(std::ostream& os, const Status& x);

// IsAborted()
// IsAlreadyExists()
// IsCancelled()
// IsDataLoss()
// IsDeadlineExceeded()
// IsFailedPrecondition()
// IsInternal()
// IsInvalidArgument()
// IsNotFound()
// IsOutOfRange()
// IsPermissionDenied()
// IsResourceExhausted()
// IsUnauthenticated()
// IsUnavailable()
// IsUnimplemented()
// IsUnknown()
//
// These convenience functions return `true` if a given status matches the
// `absl::StatusCode` error code of its associated function.
ABSL_MUST_USE_RESULT bool IsAborted(const Status& status);
ABSL_MUST_USE_RESULT bool IsAlreadyExists(const Status& status);
ABSL_MUST_USE_RESULT bool IsCancelled(const Status& status);
ABSL_MUST_USE_RESULT bool IsDataLoss(const Status& status);
ABSL_MUST_USE_RESULT bool IsDeadlineExceeded(const Status& status);
ABSL_MUST_USE_RESULT bool IsFailedPrecondition(const Status& status);
ABSL_MUST_USE_RESULT bool IsInternal(const Status& status);
ABSL_MUST_USE_RESULT bool IsInvalidArgument(const Status& status);
ABSL_MUST_USE_RESULT bool IsNotFound(const Status& status);
ABSL_MUST_USE_RESULT bool IsOutOfRange(const Status& status);
ABSL_MUST_USE_RESULT bool IsPermissionDenied(const Status& status);
ABSL_MUST_USE_RESULT bool IsResourceExhausted(const Status& status);
ABSL_MUST_USE_RESULT bool IsUnauthenticated(const Status& status);
ABSL_MUST_USE_RESULT bool IsUnavailable(const Status& status);
ABSL_MUST_USE_RESULT bool IsUnimplemented(const Status& status);
ABSL_MUST_USE_RESULT bool IsUnknown(const Status& status);

// AbortedError()
// AlreadyExistsError()
// CancelledError()
// DataLossError()
// DeadlineExceededError()
// FailedPreconditionError()
// InternalError()
// InvalidArgumentError()
// NotFoundError()
// OutOfRangeError()
// PermissionDeniedError()
// ResourceExhaustedError()
// UnauthenticatedError()
// UnavailableError()
// UnimplementedError()
// UnknownError()
//
// These convenience functions create an `absl::Status` object with an error
// code as indicated by the associated function name, using the error message
// passed in `message`.
Status AbortedError(absl::string_view message);
Status AlreadyExistsError(absl::string_view message);
Status CancelledError(absl::string_view message);
Status DataLossError(absl::string_view message);
Status DeadlineExceededError(absl::string_view message);
Status FailedPreconditionError(absl::string_view message);
Status InternalError(absl::string_view message);
Status InvalidArgumentError(absl::string_view message);
Status NotFoundError(absl::string_view message);
Status OutOfRangeError(absl::string_view message);
Status PermissionDeniedError(absl::string_view message);
Status ResourceExhaustedError(absl::string_view message);
Status UnauthenticatedError(absl::string_view message);
Status UnavailableError(absl::string_view message);
Status UnimplementedError(absl::string_view message);
Status UnknownError(absl::string_view message);

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

inline Status::Status() : rep_(CodeToInlinedRep(absl::StatusCode::kOk)) {}

inline Status::Status(absl::StatusCode code) : rep_(CodeToInlinedRep(code)) {}

inline Status::Status(const Status& x) : rep_(x.rep_) { Ref(rep_); }

inline Status& Status::operator=(const Status& x) {
  uintptr_t old_rep = rep_;
  if (x.rep_ != old_rep) {
    Ref(x.rep_);
    rep_ = x.rep_;
    Unref(old_rep);
  }
  return *this;
}

inline Status::Status(Status&& x) noexcept : rep_(x.rep_) {
  x.rep_ = MovedFromRep();
}

inline Status& Status::operator=(Status&& x) {
  uintptr_t old_rep = rep_;
  rep_ = x.rep_;
  x.rep_ = MovedFromRep();
  Unref(old_rep);
  return *this;
}

inline void Status::Update(const Status& new_status) {
  if (ok()) {
    *this = new_status;
  }
}

inline void Status::Update(Status&& new_status) {
  if (ok()) {
    *this = std::move(new_status);
  }
}

inline Status::~Status() { Unref(rep_); }

inline bool Status::ok() const {
  return rep_ == CodeToInlinedRep(absl::StatusCode::kOk);
}

inline absl::string_view Status::message() const {
  return !IsInlined(rep_)
             ? RepToPointer(rep_)->message
             : (IsMovedFrom(rep_) ? absl::string_view(kMovedFromString)
                                  : absl::string_view());
}

inline bool operator==(const Status& lhs, const Status& rhs) {
  return lhs.rep_ == rhs.rep_ || Status::EqualsSlow(lhs, rhs);
}

inline bool operator!=(const Status& lhs, const Status& rhs) {
  return !(lhs == rhs);
}

inline std::string Status::ToString() const {
  return ok() ? "OK" : ToStringSlow();
}

inline void Status::IgnoreError() const {
  // no-op
}

inline void swap(absl::Status& a, absl::Status& b) {
  using std::swap;
  swap(a.rep_, b.rep_);
}

inline const status_internal::Payloads* Status::GetPayloads() const {
  return IsInlined(rep_) ? nullptr : RepToPointer(rep_)->payloads.get();
}

inline status_internal::Payloads* Status::GetPayloads() {
  return IsInlined(rep_) ? nullptr : RepToPointer(rep_)->payloads.get();
}

inline bool Status::IsInlined(uintptr_t rep) { return (rep & 1) == 0; }

inline bool Status::IsMovedFrom(uintptr_t rep) {
  return IsInlined(rep) && (rep & 2) != 0;
}

inline uintptr_t Status::MovedFromRep() {
  return CodeToInlinedRep(absl::StatusCode::kInternal) | 2;
}

inline uintptr_t Status::CodeToInlinedRep(absl::StatusCode code) {
  return static_cast<uintptr_t>(code) << 2;
}

inline absl::StatusCode Status::InlinedRepToCode(uintptr_t rep) {
  assert(IsInlined(rep));
  return static_cast<absl::StatusCode>(rep >> 2);
}

inline status_internal::StatusRep* Status::RepToPointer(uintptr_t rep) {
  assert(!IsInlined(rep));
  return reinterpret_cast<status_internal::StatusRep*>(rep - 1);
}

inline uintptr_t Status::PointerToRep(status_internal::StatusRep* rep) {
  return reinterpret_cast<uintptr_t>(rep) + 1;
}

inline void Status::Ref(uintptr_t rep) {
  if (!IsInlined(rep)) {
    RepToPointer(rep)->ref.fetch_add(1, std::memory_order_relaxed);
  }
}

inline void Status::Unref(uintptr_t rep) {
  if (!IsInlined(rep)) {
    UnrefNonInlined(rep);
  }
}

inline Status OkStatus() { return Status(); }

// Creates a `Status` object with the `absl::StatusCode::kCancelled` error code
// and an empty message. It is provided only for efficiency, given that
// message-less kCancelled errors are common in the infrastructure.
inline Status CancelledError() { return Status(absl::StatusCode::kCancelled); }

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STATUS_STATUS_H_
