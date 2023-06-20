// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SPARSE_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SPARSE_VECTOR_H_

#include <stdint.h>

#include <limits>
#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class stores lazily-initialized FieldType instances, identified by the
// FieldId enum. Since storing pointers to all of these classes would take up
// a lot of memory, we use a Vector and only include the types that have
// actually been requested. In order to determine which index into the vector
// each type has, an additional bitfield is used to indicate which types are
// currently included in the vector.
//
// Based heavily on the ElementRareDataVector class, however the implementation
// is separate because that class requires garbage collection, whereas the
// paint properties this class is used for are ref-counted.
//
// Template type expectations:
//   FieldId: must be a scoped enum (enforced below) and have a kNumFields entry
//            with value equal to the number of entries in the enum.
//   FieldType: the intention is that it can be an arbitrary type, typically
//              either an class or a managed pointer to a class.
//
// For usage, see the associated tests in `sparse_vector_test.cc`.
template <typename FieldId,
          typename FieldType,
          typename = std::enable_if_t<std::is_enum_v<FieldId>, FieldId>>
class CORE_EXPORT SparseVector {
  USING_FAST_MALLOC(SparseVector);

 public:
  SparseVector() = default;
  SparseVector(SparseVector&&) = default;
  SparseVector(const SparseVector&) = delete;
  SparseVector& operator=(SparseVector&&) = default;
  SparseVector& operator=(const SparseVector&) = delete;
  ~SparseVector() = default;

  // Common vector methods for checking state.
  uint32_t capacity() const { return fields_.capacity(); }
  uint32_t size() const { return fields_.size(); }
  bool empty() const { return fields_.empty(); }

  // Field accessors.
  bool HasField(FieldId field_id) const {
    return fields_bitfield_ & (1u << static_cast<unsigned>(field_id));
  }

  const FieldType& GetField(FieldId field_id) const {
    CHECK(HasField(field_id));
    return fields_[GetFieldIndex(field_id)];
  }
  FieldType& GetField(FieldId field_id) {
    CHECK(HasField(field_id));
    return fields_[GetFieldIndex(field_id)];
  }

  void SetField(FieldId field_id, FieldType field) {
    if (HasField(field_id)) {
      fields_[GetFieldIndex(field_id)] = std::move(field);
    } else {
      // We want to be a little more aggressive with saving memory than the
      // WTF::Vector default of allocating four the first time a value is
      // inserted.
      constexpr int kFirstCapacityToReserve = 2;
      if (fields_.empty()) {
        fields_.reserve(kFirstCapacityToReserve);
      }
      fields_bitfield_ =
          fields_bitfield_ | (1u << static_cast<unsigned>(field_id));
      fields_.insert(GetFieldIndex(field_id), std::move(field));
    }
  }

  // Returns `true` if the field was actually erased (instead of not being
  // present).
  bool ClearField(FieldId field_id) {
    if (HasField(field_id)) {
      fields_.EraseAt(GetFieldIndex(field_id));
      fields_bitfield_ =
          fields_bitfield_ & ~(1u << static_cast<unsigned>(field_id));
      return true;
    }
    return false;
  }

 private:
  // GetFieldIndex returns the index in |fields_| that |field_id| is stored in.
  // If |fields_| isn't storing a field for |field_id|, then this returns the
  // index which the data for |field_id| should be inserted into.
  unsigned GetFieldIndex(FieldId field_id) const {
    // First, create a mask that has entries only for field IDs lower than
    // the field ID we are looking for.
    const unsigned mask = ~(~(0u) << static_cast<unsigned>(field_id));

    // Then count the total population of field IDs lower than that one we
    // are looking for. The target field ID should be located at the index of
    // of the total population.
    return __builtin_popcount(fields_bitfield_ & mask);
  }

  Vector<FieldType> fields_;

  using BitfieldType = uint32_t;
  BitfieldType fields_bitfield_ = {};
  static_assert(sizeof(fields_bitfield_) * CHAR_BIT >=
                    static_cast<unsigned>(FieldId::kNumFields),
                "field_bitfield_ must be big enough to have a bit for each "
                "field in FieldId.");
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SPARSE_VECTOR_H_
