// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
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
  if (!result) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
  }

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
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidatePointerNonNullableImpl(const uint64_t* offset,
                                    int field_index,
                                    ValidationContext* validation_context);

template <typename T>
bool ValidatePointerNonNullable(const T& input,
                                int field_index,
                                ValidationContext* validation_context) {
  return ValidatePointerNonNullableImpl(&input.offset, field_index,
                                        validation_context);
}

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const AssociatedEndpointHandle_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const Interface_Data& input);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsHandleOrInterfaceValid(const Handle_Data& input);

using ValidateStructFunc = bool(const void*, ValidationContext*);
using ValidateUnionFunc = bool(const void*, ValidationContext*, bool);
using ValidateContainerFunc = bool(const void*,
                                   ValidationContext*,
                                   const ContainerValidateParams*);

// `element_size == 0` indicates a packed-bit `bool` array.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
const ArrayHeader* ValidateArrayHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidatePrimitiveOrEnumArray(
    const void* data,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateBoolArray(const void* data,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateStructArray(const void* data,
                         ValidationContext* validation_context,
                         const ContainerValidateParams* validate_params,
                         ValidateStructFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateContainerArray(const void* data,
                            ValidationContext* validation_context,
                            const ContainerValidateParams* validate_params,
                            ValidateContainerFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateUnionArray(const void* data,
                        ValidationContext* validation_context,
                        const ContainerValidateParams* validate_params,
                        ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMapImpl(const void* data,
                     ValidationContext* validation_context,
                     const ContainerValidateParams* validate_params,
                     ValidateContainerFunc* validate_keys_func,
                     ValidateContainerFunc* validate_values_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateContainerImpl(const uint64_t* offset,
                           ValidationContext* validation_context,
                           const ContainerValidateParams* validate_params,
                           ValidateContainerFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullContainerImpl(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateContainerFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateByteArrayContainer(const uint64_t* offset,
                                ValidationContext* validation_context,
                                const ContainerValidateParams* validate_params);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullByteArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidatePrimitiveOrEnumArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullPrimitiveOrEnumArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    uint32_t element_size);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateBoolArrayContainer(const uint64_t* offset,
                                ValidationContext* validation_context,
                                const ContainerValidateParams* validate_params);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullBoolArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateStructArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    ValidateStructFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullStructArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateStructFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateUnionArrayContainer(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 const ContainerValidateParams* validate_params,
                                 ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullUnionArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMapContainer(const uint64_t* offset,
                          ValidationContext* validation_context,
                          const ContainerValidateParams* validate_params,
                          ValidateContainerFunc* validate_keys_func,
                          ValidateContainerFunc* validate_values_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullMapContainer(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 const ContainerValidateParams* validate_params,
                                 int field_index,
                                 ValidateContainerFunc* validate_keys_func,
                                 ValidateContainerFunc* validate_values_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateStructImpl(const uint64_t* offset,
                        ValidationContext* validation_context,
                        ValidateStructFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullStructImpl(const uint64_t* offset,
                               ValidationContext* validation_context,
                               int field_index,
                               ValidateStructFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateInlinedUnionImpl(const void* data,
                              ValidationContext* validation_context,
                              ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullInlinedUnionImpl(const void* data,
                                     ValidationContext* validation_context,
                                     int field_index,
                                     ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonInlinedUnionImpl(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 ValidateUnionFunc* validate_func);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullNonInlinedUnionImpl(const uint64_t* offset,
                                        ValidationContext* validation_context,
                                        int field_index,
                                        ValidateUnionFunc* validate_func);

template <typename T>
bool ValidateContainer(const Pointer<T>& input,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
  return T::ValidateContainer(&input.offset, validation_context,
                              validate_params);
}

template <typename T>
bool ValidateStruct(const Pointer<T>& input,
                    ValidationContext* validation_context) {
  return ValidateStructImpl(&input.offset, validation_context, &T::Validate);
}

template <typename T>
bool ValidateInlinedUnion(const T& input,
                          ValidationContext* validation_context) {
  return ValidateInlinedUnionImpl(&input, validation_context, &T::Validate);
}

template <typename T>
bool ValidateNonInlinedUnion(const Pointer<T>& input,
                             ValidationContext* validation_context) {
  return ValidateNonInlinedUnionImpl(&input.offset, validation_context,
                                     &T::Validate);
}

// Same as the above functions, but additionally validate that `input` is not
// null, reporting an error for `field_index` otherwise. Used by generated code
// for non-nullable fields, since a single (non-inlined) call per field results
// in smaller code than calling ValidatePointerNonNullable() separately.
template <typename T>
bool ValidateNonNullContainer(const Pointer<T>& input,
                              ValidationContext* validation_context,
                              const ContainerValidateParams* validate_params,
                              int field_index) {
  return T::ValidateNonNullContainer(&input.offset, validation_context,
                                     validate_params, field_index);
}

template <typename T>
bool ValidateNonNullStruct(const Pointer<T>& input,
                           ValidationContext* validation_context,
                           int field_index) {
  return ValidateNonNullStructImpl(&input.offset, validation_context,
                                   field_index, &T::Validate);
}

template <typename T>
bool ValidateNonNullInlinedUnion(const T& input,
                                 ValidationContext* validation_context,
                                 int field_index) {
  return ValidateNonNullInlinedUnionImpl(&input, validation_context,
                                         field_index, &T::Validate);
}

template <typename T>
bool ValidateNonNullNonInlinedUnion(const Pointer<T>& input,
                                    ValidationContext* validation_context,
                                    int field_index) {
  return ValidateNonNullNonInlinedUnionImpl(&input.offset, validation_context,
                                            field_index, &T::Validate);
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

// Same as the above functions, but additionally validate that `input` is not an
// invalid handle / interface, reporting an error for `field_index` otherwise.
// Used by generated code for non-nullable fields, since a single call results
// in smaller code.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullHandleOrInterface(const AssociatedInterface_Data& input,
                                      ValidationContext* validation_context,
                                      int field_index);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullHandleOrInterface(
    const AssociatedEndpointHandle_Data& input,
    ValidationContext* validation_context,
    int field_index);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullHandleOrInterface(const Interface_Data& input,
                                      ValidationContext* validation_context,
                                      int field_index);
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateNonNullHandleOrInterface(const Handle_Data& input,
                                      ValidationContext* validation_context,
                                      int field_index);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
