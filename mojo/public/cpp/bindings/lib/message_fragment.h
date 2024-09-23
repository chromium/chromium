// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/component_export.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

// Sentinel value used to denote an invalid index and thus a null fragment. Note
// that we choose a sentinel value over something more explicit like
// std::optional because this is used heavily in generated code, so code size
// is particularly relevant.
constexpr size_t kInvalidFragmentIndex = std::numeric_limits<size_t>::max();

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
// payload and assumes control of those bytes -- or `Claim()` which takes an
// existing pointer within the message payload and assumes control of the first
// `sizeof(T)` bytes at that message offset. In either case, a new `T` is
// constructed over the claimed bytes and can subsequently be read or modified
// using this fragment.
//
// Note that array types use a specialization of this class defined below,
// and must instead call `AllocateArrayData()` to allocate and claim space
// within the Message.
template <typename T>
class MessageFragment {
 public:
  // Constructs a null MessageFragment for `message`.
  explicit MessageFragment(Message& message) : message_(message) {}

  MessageFragment(const MessageFragment&) = delete;
  MessageFragment& operator=(const MessageFragment&) = delete;

  Message& message() { return message_; }

  // Indicates whether space for a T has been by this fragment.
  bool is_null() const { return index_ == kInvalidFragmentIndex; }

  // Returns the in-Message memory claimed by this MessageFragment.
  T* data() {
    DCHECK(!is_null());
    return message_.payload_buffer()->template Get<T>(index_);
  }

  T* operator->() { return data(); }

  // Allocates and claims `sizeof(T)` bytes on the end of the Message's payload,
  // and initializes a new `T` in place.
  void Allocate() {
    index_ = message_.payload_buffer()->Allocate(sizeof(T));
    new (data()) T();
  }

  // Claims `sizeof(T)` bytes of already-allocated memory at `ptr`, which must
  // fall entirely within an existing MessageFragment for the same Message.
  // Initializes a new `T` in place.
  void Claim(void* ptr) {
    const char* start =
        static_cast<const char*>(message_.payload_buffer()->data());
    const char* slot = static_cast<const char*>(ptr);
    DCHECK_GT(slot, start);
    index_ = slot - start;
    new (data()) T();
  }

 private:
  // RAW_PTR_EXCLUSION: Binary size increase (~350K on Android).
  RAW_PTR_EXCLUSION Message& message_;
  size_t index_ = kInvalidFragmentIndex;
};

// Traits to help infer an array sizing function from the element type. Needed
// because of bool arrays -- everything else is trivially the product of element
// size and element count.
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

// Specialization of MessageFragment<T> specific to Array_Data types.
template <typename T>
class MessageFragment<Array_Data<T>> {
 public:
  using Traits = MessageFragmentArrayTraits<T>;

  explicit MessageFragment(Message& message) : message_(message) {}

  MessageFragment(const MessageFragment&) = delete;
  MessageFragment& operator=(const MessageFragment&) = delete;

  Message& message() { return message_; }

  // Indicates whether space for the array has been claimed by this fragment.
  bool is_null() const { return index_ == kInvalidFragmentIndex; }

  // Returns the in-Message memory claimed by this MessageFragment.
  Array_Data<T>* data() {
    DCHECK(!is_null());
    return message_.payload_buffer()->template Get<Array_Data<T>>(index_);
  }

  Array_Data<T>* operator->() { return data(); }

  // Allocates and claims enough memory for `num_elements` elements of type `T`,
  // plus an array header, and initializes a new `Array_Data<T>` in place.
  void AllocateArrayData(size_t num_elements) {
    static_assert(
        std::numeric_limits<uint32_t>::max() > Traits::kMaxNumElements,
        "Max num elements castable to 32bit");
    CHECK_LE(num_elements, Traits::kMaxNumElements);

    const uint32_t num_bytes =
        Traits::GetStorageSize(static_cast<uint32_t>(num_elements));
    index_ = message_.payload_buffer()->Allocate(num_bytes);
    new (data()) Array_Data<T>(num_bytes, static_cast<uint32_t>(num_elements));
  }

 private:
  // RAW_PTR_EXCLUSION: Binary size increase (~350K on Android).
  RAW_PTR_EXCLUSION Message& message_;
  size_t index_ = kInvalidFragmentIndex;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_FRAGMENT_H_
