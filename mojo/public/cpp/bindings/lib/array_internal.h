// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <new>

#include "base/check.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"

namespace mojo {
namespace internal {

template <typename K, typename V>
class Map_Data;

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
std::string MakeMessageWithArrayIndex(const char* message,
                                      size_t size,
                                      size_t index);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
std::string MakeMessageWithExpectedArraySize(const char* message,
                                             size_t size,
                                             size_t expected_size);

template <typename T>
struct ArrayDataTraits {
  using StorageType = T;
  using Ref = T&;
  using ConstRef = const T&;

  static const uint32_t kMaxNumElements =
      MessageFragmentArrayTraits<T>::kMaxNumElements;

  static uint32_t GetStorageSize(uint32_t num_elements) {
    return MessageFragmentArrayTraits<T>::GetStorageSize(num_elements);
  }
  static Ref ToRef(StorageType* storage, size_t offset, uint32_t num_elements) {
    return storage[offset];
  }
  static ConstRef ToConstRef(const StorageType* storage,
                             size_t offset,
                             uint32_t num_elements) {
    return storage[offset];
  }
};

// Specialization of Arrays for bools, optimized for space. It has the
// following differences from a generalized Array:
// * Each element takes up a single bit of memory.
// * Accessing a non-const single element uses a helper class |BitRef|, which
// emulates a reference to a bool.
template <>
struct ArrayDataTraits<bool> {
  // Helper class to emulate a reference to a bool, used for direct element
  // access.
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) BitRef {
   public:
    ~BitRef();
    BitRef& operator=(bool value);
    BitRef& operator=(const BitRef& value);
    operator bool() const;

   private:
    friend struct ArrayDataTraits<bool>;
    BitRef(uint8_t* storage, uint8_t mask);
    BitRef();
    raw_ptr<uint8_t> storage_;
    uint8_t mask_;
  };

  // Because each element consumes only 1/8 byte.
  static const uint32_t kMaxNumElements =
      MessageFragmentArrayTraits<bool>::kMaxNumElements;

  using StorageType = uint8_t;
  using Ref = BitRef;
  using ConstRef = bool;

  static uint32_t GetStorageSize(uint32_t num_elements) {
    return MessageFragmentArrayTraits<bool>::GetStorageSize(num_elements);
  }
  static BitRef ToRef(StorageType* storage,
                      size_t offset,
                      uint32_t num_elements) {
    return BitRef(&storage[offset / 8],
                  static_cast<uint8_t>(1 << (offset % 8)));
  }
  static bool ToConstRef(const StorageType* storage,
                         size_t offset,
                         uint32_t num_elements) {
    return (storage[offset / 8] & (1 << (offset % 8))) != 0;
  }
};

// Similar to the bool specialization, except this optional specialization
// returns an |OptionalBitRef|. The |OptionalBitRef| will write to both the
// flag bit and the value bit.
template <>
struct ArrayDataTraits<std::optional<bool>> {
  using StorageType = uint8_t;

  class OptionalBitRef {
   public:
    ~OptionalBitRef() = default;

    OptionalBitRef& operator=(std::optional<bool> value) {
      if (value) {
        *flag_storage_ |= mask_;
        if (*value) {
          *value_storage_ |= mask_;
        } else {
          *value_storage_ &= ~mask_;
        }
      } else {
        *flag_storage_ &= ~mask_;
      }
      return *this;
    }

    std::optional<bool> ToOptional() {
      if (*flag_storage_ & mask_) {
        return *value_storage_ & mask_;
      } else {
        return std::nullopt;
      }
    }

    explicit operator std::optional<bool>() { return ToOptional(); }

   private:
    friend struct ArrayDataTraits<std::optional<bool>>;
    OptionalBitRef(StorageType* value_storage,
                   uint8_t* flag_storage,
                   uint8_t mask)
        : value_storage_(value_storage),
          flag_storage_(flag_storage),
          mask_(mask) {}

    OptionalBitRef() = delete;

    raw_ptr<StorageType> value_storage_;
    raw_ptr<uint8_t> flag_storage_;
    const uint8_t mask_;
  };

  using Ref = OptionalBitRef;
  using ConstRef = std::optional<bool>;
  using OptionalBoolTrait = MessageFragmentArrayTraits<std::optional<bool>>;

  static const uint32_t kMaxNumElements = OptionalBoolTrait::kMaxNumElements;

  static uint32_t GetStorageSize(uint32_t num_elements) {
    return OptionalBoolTrait::GetStorageSize(num_elements);
  }

  static OptionalBitRef ToRef(StorageType* storage,
                              size_t offset,
                              uint32_t num_elements) {
    return OptionalBitRef(
        storage + OptionalBoolTrait::GetEngagedBitfieldSize(num_elements) +
            (offset / 8),
        reinterpret_cast<uint8_t*>(storage) + (offset / 8),
        static_cast<uint8_t>(1 << (offset % 8)));
  }

  static ConstRef ToConstRef(const StorageType* storage,
                             size_t offset,
                             uint32_t num_elements) {
    return ToRef(const_cast<StorageType*>(storage), offset, num_elements)
        .ToOptional();
  }
};

// Optional specialization that returns |OptionalRef|s. |OptionalRef| will
// write to both the data address and the flag field bit.
// TODO(ffred): consider merging with the optional<bool> specialization using
// if constexpr.
template <typename T>
  requires(base::is_instantiation<std::optional, T>)
struct ArrayDataTraits<T> {
  using StorageType = typename T::value_type;

  template <typename StorageType>
  class OptionalRef {
   public:
    ~OptionalRef() = default;

    OptionalRef& operator=(std::optional<StorageType> value) {
      if (value) {
        *flag_storage_ |= mask_;
        *value_storage_ = *value;
      } else {
        *flag_storage_ &= ~mask_;
      }
      return *this;
    }

    T ToOptional() {
      if (*flag_storage_ & mask_) {
        return *value_storage_;
      } else {
        return std::nullopt;
      }
    }

    explicit operator T() { return ToOptional(); }

   private:
    friend struct ArrayDataTraits<T>;
    OptionalRef(StorageType* value_storage, uint8_t* flag_storage, uint8_t mask)
        : value_storage_(value_storage),
          flag_storage_(flag_storage),
          mask_(mask) {}

    OptionalRef() = delete;

    raw_ptr<StorageType> value_storage_;
    raw_ptr<uint8_t> flag_storage_;
    const uint8_t mask_;
  };

  using Ref = OptionalRef<StorageType>;
  using ConstRef = T;
  using OptionalTypeTrait = MessageFragmentArrayTraits<T>;

  static const uint32_t kMaxNumElements = OptionalTypeTrait::kMaxNumElements;

  static uint32_t GetStorageSize(uint32_t num_elements) {
    return OptionalTypeTrait::GetStorageSize(num_elements);
  }

  static OptionalRef<StorageType> ToRef(StorageType* storage,
                                        size_t offset,
                                        size_t num_elements) {
    // Check for proper alignment. Header is already aligned.
    DCHECK(OptionalTypeTrait::GetEngagedBitfieldSize(num_elements) %
               sizeof(StorageType) ==
           0)
        << "bitfield size should be multiple of StorageType";

    uint8_t* value_start =
        reinterpret_cast<uint8_t*>(storage) +
        OptionalTypeTrait::GetEngagedBitfieldSize(num_elements);
    return OptionalRef<StorageType>(
        reinterpret_cast<StorageType*>(value_start) + offset,
        reinterpret_cast<uint8_t*>(storage) + (offset / 8),
        static_cast<uint8_t>(1 << (offset % 8)));
  }

  static T ToConstRef(const StorageType* storage,
                      size_t offset,
                      uint32_t num_elements) {
    return ToRef(const_cast<StorageType*>(storage), offset, num_elements)
        .ToOptional();
  }
};

// What follows is code to support the serialization/validation of
// Array_Data<T>. There are four interesting cases: arrays of primitives,
// arrays of handles/interfaces, arrays of objects and arrays of unions.
// Arrays of objects are represented as arrays of pointers to objects. Arrays
// of unions are inlined so they are not pointers, but comparing with primitives
// they require more work for serialization/validation.
//
// TODO(yzshen): Validation code should be organzied in a way similar to
// Serializer<>, or merged into it. It should be templatized with the mojo
// data view type instead of the data type, that way we can use MojomTypeTraits
// to determine the categories.

template <typename T, bool is_union, bool is_handle_or_interface>
struct ArraySerializationHelper;

template <typename T>
struct ArraySerializationHelper<T, false, false> {
  using ElementType = typename ArrayDataTraits<T>::StorageType;

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               ValidationContext* validation_context,
                               const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    if (!validate_params->validate_enum_func)
      return true;

    // Enum validation.
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      // Enums are defined by mojo to be 32-bit, but this code is also compiled
      // for arrays of primitives such as uint64_t (but never called), so it's
      // safe to do a static_cast here.
      DCHECK(sizeof(elements[i]) <= sizeof(int32_t))
          << "Enum validation should never take place on a primitive type of "
             "width greater than 32-bit";
      if (!validate_params->validate_enum_func(
              static_cast<int32_t>(elements[i]), validation_context))
        return false;
    }
    return true;
  }
};

template <typename T>
struct ArraySerializationHelper<std::optional<T>, false, false> {
  using ElementType = typename ArrayDataTraits<T>::StorageType;

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               ValidationContext* validation_context,
                               const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    if (!validate_params->validate_enum_func) {
      return true;
    }
    // Enum validation.
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      // Enums are defined by mojo to be 32-bit, but this code is also compiled
      // for arrays of primitives such as uint64_t (but never called), so it's
      // safe to do a static_cast here.
      std::optional<int32_t> element =
          ArrayDataTraits<std::optional<T>>::ToConstRef(elements, i,
                                                        header->num_elements);
      if (element) {
        if (!validate_params->validate_enum_func(*element,
                                                 validation_context)) {
          return false;
        }
      }
    }
    return true;
  }
};

template <typename T>
struct ArraySerializationHelper<T, false, true> {
  using ElementType = typename ArrayDataTraits<T>::StorageType;

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               ValidationContext* validation_context,
                               const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle or interface type should not have array validate params";

    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (!validate_params->element_is_nullable &&
          !IsHandleOrInterfaceValid(elements[i])) {
        static const ValidationError kError =
            std::is_same<T, Interface_Data>::value ||
                    std::is_same<T, Handle_Data>::value
                ? VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE
                : VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID;
        ReportValidationError(
            validation_context, kError,
            MakeMessageWithArrayIndex(
                "invalid handle or interface ID in array expecting valid "
                "handles or interface IDs",
                header->num_elements, i)
                .c_str());
        return false;
      }
      if (!ValidateHandleOrInterface(elements[i], validation_context))
        return false;
    }
    return true;
  }
};

template <typename T>
struct ArraySerializationHelper<Pointer<T>, false, false> {
  using ElementType = typename ArrayDataTraits<Pointer<T>>::StorageType;

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               ValidationContext* validation_context,
                               const ContainerValidateParams* validate_params) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (!validate_params->element_is_nullable && !elements[i].offset) {
        ReportValidationError(
            validation_context,
            VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
            MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                      header->num_elements,
                                      i).c_str());
        return false;
      }
      if (!ValidateCaller<T>::Run(elements[i], validation_context,
                                  validate_params->element_validate_params)) {
        return false;
      }
    }
    return true;
  }

 private:
  template <typename U,
            bool is_array_or_map = IsSpecializationOf<Array_Data, U>::value ||
                                   IsSpecializationOf<Map_Data, U>::value>
  struct ValidateCaller {
    static bool Run(const Pointer<U>& data,
                    ValidationContext* validation_context,
                    const ContainerValidateParams* validate_params) {
      DCHECK(!validate_params)
          << "Struct type should not have array validate params";

      return ValidateStruct(data, validation_context);
    }
  };

  template <typename U>
  struct ValidateCaller<U, true> {
    static bool Run(const Pointer<U>& data,
                    ValidationContext* validation_context,
                    const ContainerValidateParams* validate_params) {
      return ValidateContainer(data, validation_context, validate_params);
    }
  };
};

template <typename U>
struct ArraySerializationHelper<U, true, false> {
  using ElementType = typename ArrayDataTraits<U>::StorageType;

  static bool ValidateElements(const ArrayHeader* header,
                               const ElementType* elements,
                               ValidationContext* validation_context,
                               const ContainerValidateParams* validate_params) {
    for (uint32_t i = 0; i < header->num_elements; ++i) {
      if (!validate_params->element_is_nullable && elements[i].is_null()) {
        ReportValidationError(
            validation_context,
            VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
            MakeMessageWithArrayIndex("null in array expecting valid unions",
                                      header->num_elements, i)
                .c_str());
        return false;
      }
      if (!ValidateInlinedUnion(elements[i], validation_context))
        return false;
    }
    return true;
  }
};

template <typename T>
class Array_Data {
 public:
  using Traits = ArrayDataTraits<T>;
  using StorageType = typename Traits::StorageType;
  using Ref = typename Traits::Ref;
  using ConstRef = typename Traits::ConstRef;
  using Helper = ArraySerializationHelper<
      T,
      IsUnionDataType<T>::value,
      std::is_same<T, AssociatedInterface_Data>::value ||
          std::is_same<T, AssociatedEndpointHandle_Data>::value ||
          std::is_same<T, Interface_Data>::value ||
          std::is_same<T, Handle_Data>::value>;
  using Element = T;

  static bool Validate(const void* data,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
    if (!data)
      return true;
    if (!IsAligned(data)) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_MISALIGNED_OBJECT);
      return false;
    }
    if (!validation_context->IsValidRange(data, sizeof(ArrayHeader))) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
      return false;
    }
    const ArrayHeader* header = static_cast<const ArrayHeader*>(data);
    if (header->num_elements > Traits::kMaxNumElements ||
        header->num_bytes < Traits::GetStorageSize(header->num_elements)) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);
      return false;
    }
    if (validate_params->expected_num_elements != 0 &&
        header->num_elements != validate_params->expected_num_elements) {
      ReportValidationError(
          validation_context,
          VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
          MakeMessageWithExpectedArraySize(
              "fixed-size array has wrong number of elements",
              header->num_elements,
              validate_params->expected_num_elements).c_str());
      return false;
    }
    if (!validation_context->ClaimMemory(data, header->num_bytes)) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
      return false;
    }

    const Array_Data<T>* object = static_cast<const Array_Data<T>*>(data);
    return Helper::ValidateElements(&object->header_, object->storage(),
                                    validation_context, validate_params);
  }

  size_t size() const { return header_.num_elements; }

  Ref at(size_t offset) {
    DCHECK(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToRef(storage(), offset, header_.num_elements);
  }

  ConstRef at(size_t offset) const {
    DCHECK(offset < static_cast<size_t>(header_.num_elements));
    return Traits::ToConstRef(storage(), offset, header_.num_elements);
  }

  StorageType* storage() {
    return reinterpret_cast<StorageType*>(reinterpret_cast<char*>(this) +
                                          sizeof(*this));
  }

  const StorageType* storage() const {
    return reinterpret_cast<const StorageType*>(this + 1);
  }

 private:
  friend class MessageFragment<Array_Data>;

  Array_Data(uint32_t num_bytes, uint32_t num_elements) {
    header_.num_bytes = num_bytes;
    header_.num_elements = num_elements;
  }
  ~Array_Data() = delete;

  internal::ArrayHeader header_;

  // Elements of type internal::ArrayDataTraits<T>::StorageType follow.
};
static_assert(sizeof(Array_Data<char>) == 8, "Bad sizeof(Array_Data)");

// UTF-8 encoded
using String_Data = Array_Data<char>;

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_INTERNAL_H_
