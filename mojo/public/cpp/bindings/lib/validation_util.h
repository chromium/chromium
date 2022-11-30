// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

struct StructVersionSize {
  uint32_t version;
  uint32_t num_bytes;
};

// Calls ReportValidationError() with a constructed error string.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void ReportNonNullableValidationError(ValidationContext* validation_context,
                                      ValidationError error,
                                      int field_index);

// Checks whether decoding the pointer will overflow and produce a pointer
// smaller than |offset|.
inline bool ValidateEncodedPointer(const uint64_t* offset) {
  // - Make sure |*offset| is no more than 32-bits.
  // - Cast |offset| to uintptr_t so overflow behavior is well defined across
  //   32-bit and 64-bit systems.
  return *offset <= std::numeric_limits<uint32_t>::max() &&
         (reinterpret_cast<uintptr_t>(offset) +
              static_cast<uint32_t>(*offset) >=
          reinterpret_cast<uintptr_t>(offset));
}

template <typename T>
bool ValidatePointer(const Pointer<T>& input,
                     ValidationContext* validation_context) {
  bool result = ValidateEncodedPointer(&input.offset);
  if (!result)
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);

  return result;
}

// Validates that |data| contains a valid struct header, in terms of alignment
// and size (i.e., the |num_bytes| field of the header is sufficient for storing
// the header itself). Besides, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |validation_context|. On success, the memory range is marked as occupied.
// Note: Does not verify |version| or that |num_bytes| is correct for the
// claimed version.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        ValidationContext* validation_context);

// Same as above, but also validates the struct's purported size and version to
// ensure they match expectations from the mojom definition.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateStructHeaderAndVersionSizeAndClaimMemory(
    const void* data,
    base::span<const StructVersionSize> version_sizes,
    ValidationContext* validation_context);

// Same as above, but for the simplest and most common case where a struct only
// defines a single version (0) with expected size of `v0_size`.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateUnversionedStructHeaderAndSizeAndClaimMemory(
    const void* data,
    size_t v0_size,
    ValidationContext* validation_context);

// Validates that |data| contains a valid union header, in terms of alignment
// and size. It checks that the memory range [data, data + kUnionDataSize) is
// not marked as occupied by other objects in |validation_context|. On success,
// the memory range is marked as occupied.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonInlinedUnionHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context);

// Validates that the message is a request which doesn't expect a response.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a request expecting a response.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a response.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMessageIsResponse(const Message* message,
                               ValidationContext* validation_context);

// Validates that the message payload is a valid struct of type ParamsType.
template <typename ParamsType>
bool ValidateMessagePayload(const Message* message,
                            ValidationContext* validation_context) {
  return ParamsType::Validate(message->payload(), validation_context);
}

// The following Validate.*NonNullable() functions validate that the given
// |input| is not null/invalid.
template <typename T>
bool ValidatePointerNonNullable(const T& input,
                                int field_index,
                                ValidationContext* validation_context) {
  if (input.offset)
    return true;
  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                                   field_index);
  return false;
}

template <typename T>
bool ValidateInlinedUnionNonNullable(const T& input,
                                     int field_index,
                                     ValidationContext* validation_context) {
  if (!input.is_null())
    return true;
  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                                   field_index);
  return false;
}

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const AssociatedEndpointHandle_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const Interface_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const Handle_Data& input);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    int field_index,
    ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedEndpointHandle_Data& input,
    int field_index,
    ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    int field_index,
    ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    int field_index,
    ValidationContext* validation_context);

template <typename T>
bool ValidateParams(const Pointer<T>& input,
                    ValidationContext* validation_context) {
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }

  return ValidatePointer(input, validation_context);
}

template <typename T>
bool ValidateContainer(const Pointer<T>& input,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
  return ValidateParams(input, validation_context) &&
         T::Validate(input.Get(), validation_context, validate_params);
}

template <typename T>
bool ValidateStruct(const Pointer<T>& input,
                    ValidationContext* validation_context) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);

  return ValidateParams(input, validation_context) &&
         T::Validate(input.Get(), validation_context);
}

template <typename T>
bool ValidateInlinedUnion(const T& input,
                          ValidationContext* validation_context) {
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return T::Validate(&input, validation_context, true);
}

template <typename T>
bool ValidateNonInlinedUnion(const Pointer<T>& input,
                             ValidationContext* validation_context) {
  return ValidateParams(input, validation_context) &&
         T::Validate(input.Get(), validation_context, false);
}

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterface(const AssociatedInterface_Data& input,
                               ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterface(const AssociatedEndpointHandle_Data& input,
                               ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterface(const Interface_Data& input,
                               ValidationContext* validation_context);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateHandleOrInterface(const Handle_Data& input,
                               ValidationContext* validation_context);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
