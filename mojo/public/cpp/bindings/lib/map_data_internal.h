// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"

namespace mojo {
namespace internal {

// Map serializes into a struct which has two arrays as struct fields, the keys
// and the values.
template <typename Key, typename Value>
class Map_Data {
 public:
  class BufferWriter {
   public:
    BufferWriter() = default;

    void Allocate(Buffer* buffer) {
      buffer_ = buffer;
      index_ = buffer_->Allocate(sizeof(Map_Data));
      new (data()) Map_Data();
    }

    bool is_null() const { return !buffer_; }
    Map_Data* data() {
      DCHECK(!is_null());
      return buffer_->Get<Map_Data>(index_);
    }
    Map_Data* operator->() { return data(); }

   private:
    Buffer* buffer_ = nullptr;
    size_t index_ = 0;

    DISALLOW_COPY_AND_ASSIGN(BufferWriter);
  };

  // |validate_params| must have non-null |key_validate_params| and
  // |element_validate_params| members.
  static bool Validate(const void* data,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
    if (!data)
      return true;

    if (!ValidateStructHeaderAndClaimMemory(data, validation_context))
      return false;

    const Map_Data* object = static_cast<const Map_Data*>(data);
    if (object->header_.num_bytes != sizeof(Map_Data) ||
        object->header_.version != 0) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
      return false;
    }

    if (!ValidatePointerNonNullable(object->keys, 0, validation_context) ||
        !ValidateContainer(object->keys, validation_context,
                           validate_params->key_validate_params)) {
      return false;
    }

    if (!ValidatePointerNonNullable(object->values, 1, validation_context) ||
        !ValidateContainer(object->values, validation_context,
                           validate_params->element_validate_params)) {
      return false;
    }

    if (object->keys.Get()->size() != object->values.Get()->size()) {
      ReportValidationError(validation_context,
                            VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP);
      return false;
    }

    return true;
  }

  StructHeader header_;

  Pointer<Array_Data<Key>> keys;
  Pointer<Array_Data<Value>> values;

 private:
  Map_Data() {
    header_.num_bytes = sizeof(*this);
    header_.version = 0;
  }
  ~Map_Data() = delete;
};
static_assert(sizeof(Map_Data<char, char>) == 24, "Bad sizeof(Map_Data)");

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
