// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_

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
  // |validate_params| must have non-null |key_validate_params| and
  // |element_validate_params| members.
  static bool Validate(const void* data,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
    return ValidateMapImpl(data, validation_context, validate_params,
                           &Array_Data<Key>::Validate,
                           &Array_Data<Value>::Validate);
  }

  static bool ValidateContainer(
      const uint64_t* offset,
      ValidationContext* validation_context,
      const ContainerValidateParams* validate_params) {
    return ValidateMapContainer(offset, validation_context, validate_params,
                                &Array_Data<Key>::Validate,
                                &Array_Data<Value>::Validate);
  }

  static bool ValidateNonNullContainer(
      const uint64_t* offset,
      ValidationContext* validation_context,
      const ContainerValidateParams* validate_params,
      int field_index) {
    return ValidateNonNullMapContainer(
        offset, validation_context, validate_params, field_index,
        &Array_Data<Key>::Validate, &Array_Data<Value>::Validate);
  }

  StructHeader header_;

  Pointer<Array_Data<Key>> keys;
  Pointer<Array_Data<Value>> values;

 private:
  friend class MessageFragment<Map_Data>;

  Map_Data() = default;
  ~Map_Data() = delete;
};
static_assert(sizeof(Map_Data<char, char>) == 24, "Bad sizeof(Map_Data)");

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
