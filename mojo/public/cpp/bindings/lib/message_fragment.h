// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/checked_math.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

// Sentinel value used to denote an invalid index and thus a null fragment. Note
// that we choose a sentinel value over something more explicit like
// std::optional because this is used heavily in generated code, so code size
// is particularly relevant.
constexpr size_t kInvalidFragmentIndex = std::numeric_limits<size_t>::max();

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageFragmentBase {
 public:
  // Constructs a null MessageFragment for `message`.
  explicit MessageFragmentBase(Message& message) : message_(message) {}

  MessageFragmentBase(const MessageFragmentBase&) = delete;
  MessageFragmentBase& operator=(const MessageFragmentBase&) = delete;

  Message& message() { return message_; }

  // Indicates whether space has been claimed by this fragment.
  bool is_null() const { return index_ == kInvalidFragmentIndex; }

  void SetPointer(size_t offset_in_parent,
                  const MessageFragmentBase& child_fragment);

  // Claims the initial parameter struct pre-allocated at the start of
  // `message_`'s payload by `Message(..., StructHeader)`.
  void ClaimInitialPayload() {
    DCHECK(is_null());
    index_ = sizeof(MessageHeaderV3);
  }

  void ClaimOffset(const MessageFragmentBase& parent, size_t offset_in_parent) {
    DCHECK(!parent.is_null());
    index_ = parent.index_ + offset_in_parent;
  }

 protected:
  void* AllocateFragment(size_t num_bytes);
  void AllocateUnversionedStructFragment(uint32_t num_bytes);
  void AllocateVersionedStructFragment(uint32_t num_bytes, uint32_t version);

  // RAW_PTR_EXCLUSION: Binary size increase (~350K on Android).
  RAW_PTR_EXCLUSION Message& message_;
  size_t index_ = kInvalidFragmentIndex;
};

// MessageFragment provides a common interface for serialization code to
// allocate, initialize, and expose convenient access to aligned blocks of data
// within a Message object. Each MessageFragment corresponds to a logical data
// element (e.g. struct, field, array, array element, etc) within a Message.
//
// A MessageFragment is configured at construction time with a partially
// serialized Message. The fragment is initially null and does not reference
// valid memory.
//
// In order to use `data()` or `operator->` the fragment must first claim a
// chunk of memory within the message. This is done by calling either
// `Allocate()` -- which appends `sizeof(T)` bytes to the end of the Message
// payload, initializes its header, and assumes control of those bytes -- or
// `ClaimOffset()`, which takes an existing parent fragment and field offset
// within the message payload and assumes control of the `sizeof(T)` bytes at
// that message offset.
//
// Note that array types use a specialization of this class defined below,
// and must instead call `AllocateArrayData()` to allocate and claim space
// within the Message.
template <typename T>
class MessageFragment : public MessageFragmentBase {
 public:
  using MessageFragmentBase::MessageFragmentBase;

  // Returns the in-Message memory claimed by this MessageFragment.
  T* data() {
    DCHECK(!is_null());
    return message_.payload_buffer()->template Get<T>(index_);
  }

  T* operator->() { return data(); }

  // Allocates and claims `sizeof(T)` bytes on the end of the Message's payload,
  // and initializes the header in place.
  void Allocate() {
    if constexpr (IsUnionDataType<T>::value) {
      AllocateFragment(sizeof(T));
    } else if constexpr (requires { T::kVersion_; }) {
      AllocateVersionedStructFragment(sizeof(T), T::kVersion_);
    } else {
      AllocateUnversionedStructFragment(sizeof(T));
    }
  }
};

// Traits to help infer an array sizing function from the element type. Needed
// because `bool` arrays and nullable primitive/enum (`std::optional<T>`) arrays
// pack values or engagement flags into bitfields -- every other element type is
// laid out at a fixed byte stride (`sizeof(ArrayHeader) +
// sizeof(ElementType) * num_elements`).
template <typename ElementType>
struct MessageFragmentArrayTraits {
  static constexpr uint32_t kMaxNumElements =
      (std::numeric_limits<uint32_t>::max() - sizeof(ArrayHeader)) /
      sizeof(ElementType);
  static uint32_t GetStorageSize(uint32_t num_elements) {
    DCHECK_LE(num_elements, kMaxNumElements);
    return sizeof(ArrayHeader) + sizeof(ElementType) * num_elements;
  }
};

template <typename ElementType>
  requires(std::is_arithmetic_v<ElementType> || std::is_enum_v<ElementType>)
struct MessageFragmentArrayTraits<std::optional<ElementType>> {
  // We must account for the optional flag bitfield preceding the data.
  static constexpr uint32_t kMaxNumElements = base::checked_cast<uint32_t>(
      ((base::CheckedNumeric<uint64_t>(8) *
            (std::numeric_limits<uint32_t>::max() -
             sizeof(ArrayHeader)
             // There may be up to 8 * sizeof(ElementType) - 1 bits of padding
             // between the bool array tracking engagement and the element array
             // tracking actual values.
             - sizeof(ElementType)) +
        1) /
       // Each element requires an extra bit to track whether the value is
       // engaged.
       (8 * sizeof(ElementType) + 1))
          .ValueOrDie());

  // The layout for arrays of nullable primitivess are:
  // |header| |flag bitfield| |values|.
  // The flag bitfield determines whether or not the value in |values| should
  // be considered as set. The bitfield is followed by padding, if needed, to
  // ensure element alignment.
  static uint32_t GetStorageSize(uint32_t num_elements) {
    DCHECK_LE(num_elements, kMaxNumElements);
    return sizeof(ArrayHeader) + GetEngagedBitfieldSize(num_elements) +
           sizeof(ElementType) * num_elements;
  }

  // Computes the number of bytes needed to represent the engaged bits for
  // |num_elements|. Aligned to sizeof(ElementType).
  static uint32_t GetEngagedBitfieldSize(uint32_t num_elements) {
    uint32_t bitfield_size = (num_elements + 7) / 8;
    uint32_t padded_size =
        base::bits::AlignUp<uint32_t>(bitfield_size, sizeof(ElementType));
    return padded_size;
  }
};

// Bool arrays are packed bit for bit, so e.g. an 8-element bool array requires
// only a single byte of storage apart from the header.
template <>
struct MessageFragmentArrayTraits<bool> {
  static constexpr uint32_t kMaxNumElements =
      std::numeric_limits<uint32_t>::max() - 7;
  static uint32_t GetStorageSize(uint32_t num_elements) {
    DCHECK_LE(num_elements, kMaxNumElements);
    return sizeof(ArrayHeader) + ((num_elements + 7) / 8);
  }
};

// Optional bool arrays are two consecutive bitfields.
template <>
struct MessageFragmentArrayTraits<std::optional<bool>> {
  static constexpr uint32_t kMaxNumElements =
      std::numeric_limits<uint32_t>::max() - 7;

  static uint32_t GetStorageSize(uint32_t num_elements) {
    DCHECK_LE(num_elements, kMaxNumElements);
    return sizeof(ArrayHeader) + GetEngagedBitfieldSize(num_elements) +
           ((num_elements + 7) / 8);
  }

  static uint32_t GetEngagedBitfieldSize(uint32_t num_elements) {
    return (num_elements + 7) / 8;
  }
};

template <typename T>
class Array_Data;

// Base for array MessageFragments whose elements have a fixed byte size
// (`sizeof(ArrayHeader) + num_elements * element_size`), covering all element
// types except bit-packed `bool` and nullable primitives/enums
// (`std::optional<T>`).
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) FixedSizeArrayMessageFragment
    : public MessageFragmentBase {
 public:
  using MessageFragmentBase::MessageFragmentBase;

 protected:
  void* AllocateArrayFragment(size_t num_elements, uint32_t element_size);
  void AllocateAndCopyArrayFragment(const void* data,
                                    size_t num_elements,
                                    uint32_t element_size);
  void SetArrayElementPointer(size_t index,
                              const MessageFragmentBase& child_fragment);
};

// Base for array MessageFragments that pack values or nullability flags into
// bitfields (`bool`, `std::optional<bool>`, and nullable primitives/enums
// `std::optional<T>`).
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) BitPackedArrayMessageFragment
    : public MessageFragmentBase {
 public:
  using MessageFragmentBase::MessageFragmentBase;

 protected:
  void AllocateArrayFragment(uint32_t num_bytes, uint32_t num_elements);
};

template <typename T, typename Base>
class ArrayMessageFragment : public Base {
 public:
  using Base::Base;

  // Returns the in-Message memory claimed by this MessageFragment.
  Array_Data<T>* data() {
    DCHECK(!this->is_null());
    return this->message_.payload_buffer()->template Get<Array_Data<T>>(
        this->index_);
  }

  Array_Data<T>* operator->() { return data(); }
};

template <typename T>
inline constexpr bool kIsBitPackedArrayElement = false;
template <>
inline constexpr bool kIsBitPackedArrayElement<bool> = true;
template <typename T>
  requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
inline constexpr bool kIsBitPackedArrayElement<std::optional<T>> = true;

// Specialization of MessageFragment<T> for fixed-element-size Array_Data types.
template <typename T>
class MessageFragment<Array_Data<T>>
    : public ArrayMessageFragment<T, FixedSizeArrayMessageFragment> {
 public:
  using Base = ArrayMessageFragment<T, FixedSizeArrayMessageFragment>;
  using Base::Base;

  // Allocates and claims enough memory for `num_elements` elements of type `T`,
  // plus an array header, and initializes the `ArrayHeader` in place.
  void AllocateArrayData(size_t num_elements) {
    this->AllocateArrayFragment(num_elements, sizeof(T));
  }

  void AllocateAndCopyArrayData(const T* data, size_t num_elements) {
    this->AllocateAndCopyArrayFragment(data, num_elements, sizeof(T));
  }

  template <typename U>
    requires std::same_as<T, Pointer<U>>
  void SetElementPointer(size_t index,
                         const MessageFragment<U>& child_fragment) {
    this->SetArrayElementPointer(index, child_fragment);
  }

  void ClaimElement(size_t index, MessageFragment<T>& element_fragment) {
    DCHECK(!this->is_null());
    DCHECK_LT(index, this->message_.payload_buffer()
                         ->template Get<ArrayHeader>(this->index_)
                         ->num_elements);
    element_fragment.ClaimOffset(*this,
                                 sizeof(ArrayHeader) + index * sizeof(T));
  }
};

// Specialization of MessageFragment<T> for bit-packed Array_Data types (`bool`
// and nullable primitives/enums `std::optional<T>`).
template <typename T>
  requires kIsBitPackedArrayElement<T>
class MessageFragment<Array_Data<T>>
    : public ArrayMessageFragment<T, BitPackedArrayMessageFragment> {
 public:
  using Base = ArrayMessageFragment<T, BitPackedArrayMessageFragment>;
  using Traits = MessageFragmentArrayTraits<T>;
  using Base::Base;

  // Allocates and claims enough memory for `num_elements` bit-packed elements
  // of type `T`, plus an array header, and initializes the `ArrayHeader` in
  // place.
  void AllocateArrayData(size_t num_elements) {
    static_assert(
        std::numeric_limits<uint32_t>::max() > Traits::kMaxNumElements,
        "Max num elements castable to 32bit");
    CHECK_LE(num_elements, Traits::kMaxNumElements);
    const uint32_t num_bytes =
        Traits::GetStorageSize(static_cast<uint32_t>(num_elements));
    this->AllocateArrayFragment(num_bytes, static_cast<uint32_t>(num_elements));
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_
