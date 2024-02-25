// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

static const int kMaxRecursionDepth = 200;

namespace mojo {

class Message;

namespace internal {

// ValidationContext is used when validating object sizes, pointers and handle
// indices in the payload of incoming messages.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ValidationContext {
 public:
  enum ValidatorType {
    kUnspecifiedValidator,
    kRequestValidator,
    kResponseValidator
  };

  // [data, data + data_num_bytes) specifies the initial valid memory range.
  // [0, num_handles) specifies the initial valid range of handle indices.
  // [0, num_associated_endpoint_handles) specifies the initial valid range of
  // associated endpoint handle indices.
  //
  // If provided, |message| and |description| provide additional information
  // to use when reporting validation errors. In addition if |message| is
  // provided, the MojoNotifyBadMessage API will be used to notify the system of
  // such errors.
  ValidationContext(const void* data,
                    size_t data_num_bytes,
                    size_t num_handles,
                    size_t num_associated_endpoint_handles,
                    Message* message = nullptr,
                    const char* description = "",
                    int stack_depth = 0,
                    ValidatorType validator_type = kUnspecifiedValidator);

  // As above, but infers most of the parameters from the Message payload.
  // Used heavily in generated code and so affects binary size.
  ValidationContext(Message* message,
                    const char* description,
                    ValidatorType validator_type);

  ValidationContext(const ValidationContext&) = delete;
  ValidationContext& operator=(const ValidationContext&) = delete;

  ~ValidationContext();

  // Claims the specified memory range.
  // The method succeeds if the range is valid to claim. (Please see
  // the comments for IsValidRange().)
  // On success, the valid memory range is shrinked to begin right after the end
  // of the claimed range.
  bool ClaimMemory(const void* position, uint32_t num_bytes) {
    uintptr_t begin = reinterpret_cast<uintptr_t>(position);
    uintptr_t end = begin + num_bytes;

    if (!InternalIsValidRange(begin, end))
      return false;

    data_begin_ = end;
    return true;
  }

  // Claims the specified encoded handle (which is basically a handle index).
  // The method succeeds if:
  // - |encoded_handle|'s value is |kEncodedInvalidHandleValue|.
  // - the handle is contained inside the valid range of handle indices. In this
  // case, the valid range is shinked to begin right after the claimed handle.
  bool ClaimHandle(const Handle_Data& encoded_handle) {
    uint32_t index = encoded_handle.value;
    if (index == kEncodedInvalidHandleValue)
      return true;

    if (index < handle_begin_ || index >= handle_end_)
      return false;

    // |index| + 1 shouldn't overflow, because |index| is not the max value of
    // uint32_t (it is less than |handle_end_|).
    handle_begin_ = index + 1;
    return true;
  }

  // Claims the specified encoded associated endpoint handle.
  // The method succeeds if:
  // - |encoded_handle|'s value is |kEncodedInvalidHandleValue|.
  // - the handle is contained inside the valid range of associated endpoint
  //   handle indices. In this case, the valid range is shinked to begin right
  //   after the claimed handle.
  bool ClaimAssociatedEndpointHandle(
      const AssociatedEndpointHandle_Data& encoded_handle) {
    uint32_t index = encoded_handle.value;
    if (index == kEncodedInvalidHandleValue)
      return true;

    if (index < associated_endpoint_handle_begin_ ||
        index >= associated_endpoint_handle_end_)
      return false;

    // |index| + 1 shouldn't overflow, because |index| is not the max value of
    // uint32_t (it is less than |associated_endpoint_handle_end_|).
    associated_endpoint_handle_begin_ = index + 1;
    return true;
  }

  // Returns true if the specified range is not empty, and the range is
  // contained inside the valid memory range.
  bool IsValidRange(const void* position, uint32_t num_bytes) const {
    uintptr_t begin = reinterpret_cast<uintptr_t>(position);
    uintptr_t end = begin + num_bytes;

    return InternalIsValidRange(begin, end);
  }

  // This object should be created on the stack once every time we recurse down
  // into a subfield during validation to make sure we don't recurse too deep
  // and blow the stack.
  class ScopedDepthTracker {
   public:
    // |ctx| must outlive this object.
    explicit ScopedDepthTracker(ValidationContext* ctx) : ctx_(ctx) {
      ++ctx_->stack_depth_;
    }

    ScopedDepthTracker(const ScopedDepthTracker&) = delete;
    ScopedDepthTracker& operator=(const ScopedDepthTracker&) = delete;

    ~ScopedDepthTracker() { --ctx_->stack_depth_; }

   private:
    // `ctx_` is not a raw_ptr<...> for performance reasons: On-stack pointee
    // (i.e. not covered by BackupRefPtr protection).
    RAW_PTR_EXCLUSION ValidationContext* ctx_;
  };

  // Returns true if the recursion depth limit has been reached.
  [[nodiscard]] bool ExceedsMaxDepth() {
    return stack_depth_ > kMaxRecursionDepth;
  }

  Message* message() const { return message_; }
  const char* description() const { return description_; }
  std::string GetFullDescription() const;

 private:
  bool InternalIsValidRange(uintptr_t begin, uintptr_t end) const {
    return end > begin && begin >= data_begin_ && end <= data_end_;
  }

  // RAW_PTR_EXCLUSION: Performance reasons: on-stack pointer + based on
  // analysis of sampling profiler data (MultiplexRouter::ProcessIncomingMessage
  // -> PipeControlMessageHandler::Accept -> PipeControlMessageHandler::Validate
  // -> constructs ValidationContext).
  RAW_PTR_EXCLUSION Message* const message_;
  const char* const description_;
  const ValidatorType validator_type_;

  // [data_begin_, data_end_) is the valid memory range.
  uintptr_t data_begin_;
  uintptr_t data_end_;

  // [handle_begin_, handle_end_) is the valid handle index range.
  uint32_t handle_begin_;
  uint32_t handle_end_;

  // [associated_endpoint_handle_begin_, associated_endpoint_handle_end_) is the
  // valid associated endpoint handle index range.
  uint32_t associated_endpoint_handle_begin_;
  uint32_t associated_endpoint_handle_end_;

  int stack_depth_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_CONTEXT_H_
