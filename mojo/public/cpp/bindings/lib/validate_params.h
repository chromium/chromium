// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_

#include <stdint.h>

#include "base/macros.h"

namespace mojo {
namespace internal {

class ValidationContext;

using ValidateEnumFunc = bool (*)(int32_t, ValidationContext*);

class ContainerValidateParams {
 public:
  // Validates a map. A map is validated as a pair of arrays, one for the keys
  // and one for the values. Both arguments must be non-null.
  //
  // ContainerValidateParams takes ownership of |in_key_validate params| and
  // |in_element_validate params|.
  ContainerValidateParams(ContainerValidateParams* in_key_validate_params,
                          ContainerValidateParams* in_element_validate_params)
      : key_validate_params(in_key_validate_params),
        element_validate_params(in_element_validate_params) {
    DCHECK(in_key_validate_params)
        << "Map validate params require key validate params";
    DCHECK(in_element_validate_params)
        << "Map validate params require element validate params";
  }

  // Validates an array.
  //
  // ContainerValidateParams takes ownership of |in_element_validate params|.
  ContainerValidateParams(uint32_t in_expected_num_elements,
                          bool in_element_is_nullable,
                          ContainerValidateParams* in_element_validate_params)
      : expected_num_elements(in_expected_num_elements),
        element_is_nullable(in_element_is_nullable),
        element_validate_params(in_element_validate_params) {}

  // Validates an array of enums.
  ContainerValidateParams(uint32_t in_expected_num_elements,
                          ValidateEnumFunc in_validate_enum_func)
      : expected_num_elements(in_expected_num_elements),
        validate_enum_func(in_validate_enum_func) {}

  ~ContainerValidateParams() {
    if (element_validate_params)
      delete element_validate_params;
    if (key_validate_params)
      delete key_validate_params;
  }

  // If |expected_num_elements| is not 0, the array is expected to have exactly
  // that number of elements.
  uint32_t expected_num_elements = 0;

  // Whether the elements are nullable.
  bool element_is_nullable = false;

  // Validation information for the map key array. May contain other
  // ArrayValidateParams e.g. if the keys are strings.
  ContainerValidateParams* key_validate_params = nullptr;

  // For arrays: validation information for elements. It is either a pointer to
  // another instance of ArrayValidateParams (if elements are arrays or maps),
  // or nullptr.
  //
  // For maps: validation information for the whole value array. May contain
  // other ArrayValidateParams e.g. if the values are arrays or maps.
  ContainerValidateParams* element_validate_params = nullptr;

  // Validation function for enum elements.
  ValidateEnumFunc validate_enum_func = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerValidateParams);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
