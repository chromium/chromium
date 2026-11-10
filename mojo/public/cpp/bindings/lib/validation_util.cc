// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_util.h"

#include <stdint.h>

#include <ranges>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/map_data_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {
namespace internal {

namespace {

bool ValidatePointerOffset(const uint64_t* offset,
                           ValidationContext* validation_context) {
  if (!ValidateEncodedPointer(offset)) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }
  return true;
}

bool ValidateParamsOffset(const uint64_t* offset,
                          ValidationContext* validation_context) {
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return ValidatePointerOffset(offset, validation_context);
}

bool CheckPointerNonNullable(const uint64_t* offset,
                             int field_index,
                             ValidationContext* validation_context) {
  // Encoded `Pointer<T>` values store a relative byte offset where `0` encodes
  // a null pointer (see `EncodePointer()` / `Pointer<T>::is_null()`).
  if (*offset) [[likely]] {
    return true;
  }
  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                                   field_index);
  return false;
}

}  // namespace

void ReportNonNullableValidationError(ValidationContext* validation_context,
                                      ValidationError error,
                                      int field_index) {
  const char* null_or_invalid =
      error == VALIDATION_ERROR_UNEXPECTED_NULL_POINTER ? "null" : "invalid";

  std::string error_message =
      base::StringPrintf("%s field %d", null_or_invalid, field_index);
  ReportValidationError(validation_context, error, error_message.c_str());
}

bool ValidatePointerNonNullableImpl(const uint64_t* offset,
                                    int field_index,
                                    ValidationContext* validation_context) {
  return CheckPointerNonNullable(offset, field_index, validation_context);
}

const ArrayHeader* ValidateArrayHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size) {
  DCHECK(data);
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return nullptr;
  }
  if (!validation_context->IsValidRange(data, sizeof(ArrayHeader))) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return nullptr;
  }
  const ArrayHeader* header = static_cast<const ArrayHeader*>(data);
  if (element_size == 0) {
    if (header->num_elements >
            MessageFragmentArrayTraits<bool>::kMaxNumElements ||
        header->num_bytes < MessageFragmentArrayTraits<bool>::GetStorageSize(
                                header->num_elements)) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);
      return nullptr;
    }
  } else {
    const uint64_t min_num_bytes =
        sizeof(ArrayHeader) + uint64_t{header->num_elements} * element_size;
    if (header->num_bytes < min_num_bytes) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);
      return nullptr;
    }
  }
  if (validate_params->expected_num_elements != 0 &&
      header->num_elements != validate_params->expected_num_elements) {
    ReportValidationError(
        validation_context, VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
        MakeMessageWithExpectedArraySize(
            "fixed-size array has wrong number of elements",
            header->num_elements, validate_params->expected_num_elements));
    return nullptr;
  }
  if (!validation_context->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return nullptr;
  }
  return header;
}

bool ValidatePrimitiveOrEnumArray(
    const void* data,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size) {
  if (!data) {
    return true;
  }
  const ArrayHeader* header = ValidateArrayHeaderAndClaimMemory(
      data, validation_context, validate_params, element_size);
  if (!header) {
    return false;
  }
  DCHECK(!validate_params->element_is_nullable)
      << "Primitive type should be non-nullable";
  DCHECK(!validate_params->element_validate_params)
      << "Primitive type should not have array validate params";

  if (!validate_params->validate_enum_func) {
    return true;
  }
  DCHECK_EQ(element_size, sizeof(int32_t));
  const int32_t* elements =
      reinterpret_cast<const int32_t*>(UNSAFE_TODO(header + 1));
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    if (!validate_params->validate_enum_func(UNSAFE_TODO(elements[i]),
                                             validation_context)) {
      return false;
    }
  }
  return true;
}

bool ValidateBoolArray(const void* data,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
  if (!data) {
    return true;
  }
  return ValidateArrayHeaderAndClaimMemory(data, validation_context,
                                           validate_params, 0) != nullptr;
}

bool ValidateStructArray(const void* data,
                         ValidationContext* validation_context,
                         const ContainerValidateParams* validate_params,
                         ValidateStructFunc* validate_func) {
  if (!data) {
    return true;
  }
  const ArrayHeader* header = ValidateArrayHeaderAndClaimMemory(
      data, validation_context, validate_params, sizeof(GenericPointer));
  if (!header) {
    return false;
  }
  DCHECK(!validate_params->element_validate_params)
      << "Struct type should not have array validate params";
  const uint64_t* elements =
      reinterpret_cast<const uint64_t*>(UNSAFE_TODO(header + 1));
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    // Each element is an encoded 8-byte `Pointer<T>` offset (`0` encodes null).
    if (!validate_params->element_is_nullable && !UNSAFE_TODO(elements[i])) {
      ReportValidationError(
          validation_context, VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    header->num_elements, i));
      return false;
    }
    if (!ValidateStructImpl(UNSAFE_TODO(&elements[i]), validation_context,
                            validate_func)) {
      return false;
    }
  }
  return true;
}

bool ValidateContainerArray(const void* data,
                            ValidationContext* validation_context,
                            const ContainerValidateParams* validate_params,
                            ValidateContainerFunc* validate_func) {
  if (!data) {
    return true;
  }
  const ArrayHeader* header = ValidateArrayHeaderAndClaimMemory(
      data, validation_context, validate_params, sizeof(GenericPointer));
  if (!header) {
    return false;
  }
  const uint64_t* elements =
      reinterpret_cast<const uint64_t*>(UNSAFE_TODO(header + 1));
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    // Each element is an encoded 8-byte `Pointer<T>` offset (`0` encodes null).
    if (!validate_params->element_is_nullable && !UNSAFE_TODO(elements[i])) {
      ReportValidationError(
          validation_context, VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    header->num_elements, i));
      return false;
    }
    if (!ValidateContainerImpl(UNSAFE_TODO(&elements[i]), validation_context,
                               validate_params->element_validate_params,
                               validate_func)) {
      return false;
    }
  }
  return true;
}

bool ValidateUnionArray(const void* data,
                        ValidationContext* validation_context,
                        const ContainerValidateParams* validate_params,
                        ValidateUnionFunc* validate_func) {
  if (!data) {
    return true;
  }
  const ArrayHeader* header = ValidateArrayHeaderAndClaimMemory(
      data, validation_context, validate_params, kUnionDataSize);
  if (!header) {
    return false;
  }
  const uint8_t* elements =
      reinterpret_cast<const uint8_t*>(UNSAFE_TODO(header + 1));
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    const void* element = UNSAFE_TODO(elements + i * kUnionDataSize);
    // The leading `uint32_t` of an inlined union is its `size` field (`0`
    // encodes a null union; see `Union_Data::is_null()`).
    if (!validate_params->element_is_nullable &&
        *static_cast<const uint32_t*>(element) == 0) {
      ReportValidationError(
          validation_context, VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    header->num_elements, i));
      return false;
    }
    if (!ValidateInlinedUnionImpl(element, validation_context, validate_func)) {
      return false;
    }
  }
  return true;
}

bool ValidateMapImpl(const void* data,
                     ValidationContext* validation_context,
                     const ContainerValidateParams* validate_params,
                     ValidateContainerFunc* validate_keys_func,
                     ValidateContainerFunc* validate_values_func) {
  if (!data) {
    return true;
  }
  if (!ValidateStructHeaderAndClaimMemory(data, validation_context)) {
    return false;
  }
  const auto* object = static_cast<const Map_Data<char, char>*>(data);
  if (object->header_.num_bytes != sizeof(Map_Data<char, char>) ||
      object->header_.version != 0) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }
  if (!ValidateNonNullContainerImpl(&object->keys.offset, validation_context,
                                    validate_params->key_validate_params, 0,
                                    validate_keys_func) ||
      !ValidateNonNullContainerImpl(&object->values.offset, validation_context,
                                    validate_params->element_validate_params, 1,
                                    validate_values_func)) {
    return false;
  }
  // SAFETY: Validated above.
  if (UNSAFE_BUFFERS(object->keys.Get())->size() !=
      UNSAFE_BUFFERS(object->values.Get())->size()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP);
    return false;
  }
  return true;
}

NOINLINE bool ValidateContainerImpl(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    ValidateContainerFunc* validate_func) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         validate_func(UNSAFE_BUFFERS(DecodePointer(offset)),
                       validation_context, validate_params);
}

NOINLINE bool ValidateNonNullContainerImpl(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateContainerFunc* validate_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateContainerImpl(offset, validation_context, validate_params,
                               validate_func);
}

NOINLINE bool ValidateByteArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidatePrimitiveOrEnumArray(UNSAFE_BUFFERS(DecodePointer(offset)),
                                      validation_context, validate_params, 1);
}

NOINLINE bool ValidateNonNullByteArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateByteArrayContainer(offset, validation_context,
                                    validate_params);
}

NOINLINE bool ValidatePrimitiveOrEnumArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    uint32_t element_size) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidatePrimitiveOrEnumArray(UNSAFE_BUFFERS(DecodePointer(offset)),
                                      validation_context, validate_params,
                                      element_size);
}

NOINLINE bool ValidateNonNullPrimitiveOrEnumArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    uint32_t element_size) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidatePrimitiveOrEnumArrayContainer(offset, validation_context,
                                               validate_params, element_size);
}

bool ValidateBoolArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidateBoolArray(UNSAFE_BUFFERS(DecodePointer(offset)),
                           validation_context, validate_params);
}

bool ValidateNonNullBoolArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateBoolArrayContainer(offset, validation_context,
                                    validate_params);
}

NOINLINE bool ValidateStructArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    ValidateStructFunc* validate_func) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidateStructArray(UNSAFE_BUFFERS(DecodePointer(offset)),
                             validation_context, validate_params,
                             validate_func);
}

NOINLINE bool ValidateNonNullStructArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateStructFunc* validate_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateStructArrayContainer(offset, validation_context,
                                      validate_params, validate_func);
}

bool ValidateUnionArrayContainer(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 const ContainerValidateParams* validate_params,
                                 ValidateUnionFunc* validate_func) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidateUnionArray(UNSAFE_BUFFERS(DecodePointer(offset)),
                            validation_context, validate_params, validate_func);
}

NOINLINE bool ValidateNonNullUnionArrayContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateUnionFunc* validate_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateUnionArrayContainer(offset, validation_context,
                                     validate_params, validate_func);
}

NOINLINE bool ValidateMapContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    ValidateContainerFunc* validate_keys_func,
    ValidateContainerFunc* validate_values_func) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         ValidateMapImpl(UNSAFE_BUFFERS(DecodePointer(offset)),
                         validation_context, validate_params,
                         validate_keys_func, validate_values_func);
}

NOINLINE bool ValidateNonNullMapContainer(
    const uint64_t* offset,
    ValidationContext* validation_context,
    const ContainerValidateParams* validate_params,
    int field_index,
    ValidateContainerFunc* validate_keys_func,
    ValidateContainerFunc* validate_values_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateMapContainer(offset, validation_context, validate_params,
                              validate_keys_func, validate_values_func);
}

NOINLINE bool ValidateStructImpl(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 ValidateStructFunc* validate_func) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         validate_func(UNSAFE_BUFFERS(DecodePointer(offset)),
                       validation_context);
}

NOINLINE bool ValidateNonNullStructImpl(const uint64_t* offset,
                                        ValidationContext* validation_context,
                                        int field_index,
                                        ValidateStructFunc* validate_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateStructImpl(offset, validation_context, validate_func);
}

NOINLINE bool ValidateInlinedUnionImpl(const void* data,
                                       ValidationContext* validation_context,
                                       ValidateUnionFunc* validate_func) {
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return validate_func(data, validation_context, true);
}

NOINLINE bool ValidateNonNullInlinedUnionImpl(
    const void* data,
    ValidationContext* validation_context,
    int field_index,
    ValidateUnionFunc* validate_func) {
  // A serialized union starts with a 4-byte `size` field (`4-byte size +
  // 4-byte tag + 8-byte payload`), where `size == 0` encodes a null union
  // (matching generated `Union_Data::is_null()`).
  if (*static_cast<const uint32_t*>(data) == 0) [[unlikely]] {
    ReportNonNullableValidationError(validation_context,
                                     VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                                     field_index);
    return false;
  }
  return ValidateInlinedUnionImpl(data, validation_context, validate_func);
}

bool ValidateNonInlinedUnionImpl(const uint64_t* offset,
                                 ValidationContext* validation_context,
                                 ValidateUnionFunc* validate_func) {
  // SAFETY: The decoded pointer is bounds-checked before it is dereferenced.
  return ValidateParamsOffset(offset, validation_context) &&
         validate_func(UNSAFE_BUFFERS(DecodePointer(offset)),
                       validation_context, false);
}

NOINLINE bool ValidateNonNullNonInlinedUnionImpl(
    const uint64_t* offset,
    ValidationContext* validation_context,
    int field_index,
    ValidateUnionFunc* validate_func) {
  return CheckPointerNonNullable(offset, field_index, validation_context) &&
         ValidateNonInlinedUnionImpl(offset, validation_context, validate_func);
}

bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }
  if (!validation_context->IsValidRange(data, sizeof(StructHeader))) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  const StructHeader* header = static_cast<const StructHeader*>(data);

  if (header->num_bytes < sizeof(StructHeader)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  if (!validation_context->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

NOINLINE bool ValidateStructHeaderAndVersionSizeAndClaimMemory(
    const void* data,
    base::span<const StructVersionSize> version_sizes,
    ValidationContext* validation_context) {
  if (!ValidateStructHeaderAndClaimMemory(data, validation_context)) {
    return false;
  }

  DCHECK(data);
  DCHECK(!version_sizes.empty());
  const auto& header = *static_cast<const StructHeader*>(data);
  if (header.version <= version_sizes.back().version) {
    // Scan in reverse order to optimize for more recent versions.
    for (const auto& version_size : std::views::reverse(version_sizes)) {
      if (header.version >= version_size.version) {
        if (header.num_bytes == version_size.num_bytes) {
          break;
        }
        ReportValidationError(validation_context,
                              VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
        return false;
      }
    }
  } else if (header.num_bytes < version_sizes.back().num_bytes) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  return true;
}

NOINLINE bool ValidateUnversionedStructHeaderAndSizeAndClaimMemory(
    const void* data,
    size_t v0_size,
    ValidationContext* validation_context) {
  if (!ValidateStructHeaderAndClaimMemory(data, validation_context)) {
    return false;
  }

  DCHECK(data);
  const auto& header = *static_cast<const StructHeader*>(data);
  if ((header.version == 0 && header.num_bytes != v0_size) ||
      header.num_bytes < v0_size) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  return true;
}

NOINLINE bool ValidateNonInlinedUnionHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }

  // The first 4 bytes of a serialized union store its `size` field, which must
  // equal `kUnionDataSize` (16 bytes) for a non-null out-of-line union.
  if (!validation_context->ClaimMemory(data, kUnionDataSize) ||
      *static_cast<const uint32_t*>(data) != kUnionDataSize) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      !message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsResponse(const Message* message,
                               ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagExpectsResponse) ||
      !message->has_flag(Message::kFlagIsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input) {
  return input.handle.is_valid();
}

bool IsHandleOrInterfaceValid(const AssociatedEndpointHandle_Data& input) {
  return input.is_valid();
}

bool IsHandleOrInterfaceValid(const Interface_Data& input) {
  return input.handle.is_valid();
}

bool IsHandleOrInterfaceValid(const Handle_Data& input) {
  return input.is_valid();
}

NOINLINE bool ValidateHandleOrInterface(const AssociatedInterface_Data& input,
                                        ValidationContext* validation_context) {
  if (validation_context->ClaimAssociatedEndpointHandle(input.handle)) {
    return true;
  }

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

NOINLINE bool ValidateHandleOrInterface(
    const AssociatedEndpointHandle_Data& input,
    ValidationContext* validation_context) {
  if (validation_context->ClaimAssociatedEndpointHandle(input)) {
    return true;
  }

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

NOINLINE bool ValidateHandleOrInterface(const Interface_Data& input,
                                        ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input.handle)) {
    return true;
  }

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

NOINLINE bool ValidateHandleOrInterface(const Handle_Data& input,
                                        ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input)) {
    return true;
  }

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

namespace {

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input)) {
    return true;
  }

  ReportNonNullableValidationError(
      validation_context, VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
      field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedEndpointHandle_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input)) {
    return true;
  }

  ReportNonNullableValidationError(
      validation_context, VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
      field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input)) {
    return true;
  }

  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                                   field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input)) {
    return true;
  }

  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                                   field_index);
  return false;
}

template <typename T>
bool ValidateNonNullHandleOrInterfaceImpl(const T& input,
                                          ValidationContext* validation_context,
                                          int field_index) {
  return ValidateHandleOrInterfaceNonNullable(input, field_index,
                                              validation_context) &&
         ValidateHandleOrInterface(input, validation_context);
}

}  // namespace

NOINLINE bool ValidateNonNullHandleOrInterface(
    const AssociatedInterface_Data& input,
    ValidationContext* validation_context,
    int field_index) {
  return ValidateNonNullHandleOrInterfaceImpl(input, validation_context,
                                              field_index);
}

NOINLINE bool ValidateNonNullHandleOrInterface(
    const AssociatedEndpointHandle_Data& input,
    ValidationContext* validation_context,
    int field_index) {
  return ValidateNonNullHandleOrInterfaceImpl(input, validation_context,
                                              field_index);
}

NOINLINE bool ValidateNonNullHandleOrInterface(
    const Interface_Data& input,
    ValidationContext* validation_context,
    int field_index) {
  return ValidateNonNullHandleOrInterfaceImpl(input, validation_context,
                                              field_index);
}

NOINLINE bool ValidateNonNullHandleOrInterface(
    const Handle_Data& input,
    ValidationContext* validation_context,
    int field_index) {
  return ValidateNonNullHandleOrInterfaceImpl(input, validation_context,
                                              field_index);
}

}  // namespace internal
}  // namespace mojo
