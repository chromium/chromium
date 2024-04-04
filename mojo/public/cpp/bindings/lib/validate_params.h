// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_

#include <stdint.h>

#include "base/memory/raw_ptr_exclusion.h"

namespace mojo {
namespace internal {

class ValidationContext;

using ValidateEnumFunc = bool(int32_t, ValidationContext*);

// !!! Do not construct directly. Use the Get*Validator() helpers below !!!
class ContainerValidateParams {
 public:
  // If |expected_num_elements| is not 0, the array is expected to have exactly
  // that number of elements.
  uint32_t expected_num_elements = 0;

  // Whether the elements are nullable.
  bool element_is_nullable = false;

  // Validation information for the map key array. May contain other
  // ArrayValidateParams e.g. if the keys are strings.
  //
  // `key_validate_params` is not a raw_ptr<...> for performance reasons:
  // when non-null, the pointer always refers to a statically-allocated
  // constexpr ContainerValidateParams that is never freed.
  RAW_PTR_EXCLUSION const ContainerValidateParams* key_validate_params =
      nullptr;

  // For arrays: validation information for elements. It is either a pointer to
  // another instance of ArrayValidateParams (if elements are arrays or maps),
  // or nullptr.
  //
  // For maps: validation information for the whole value array. May contain
  // other ArrayValidateParams e.g. if the values are arrays or maps.
  //
  // `element_validate_params` is not a raw_ptr<...> for performance reasons:
  // when non-null, the pointer always refers to a statically-allocated
  // constexpr ContainerValidateParams that is never freed.
  RAW_PTR_EXCLUSION const ContainerValidateParams* element_validate_params =
      nullptr;

  // Validation function for enum elements.
  ValidateEnumFunc* validate_enum_func = nullptr;
};

// !!! Do not use directly. Use the Get*Validator() helpers below !!!
//
// These templates define storage for various ContainerValidateParam instances.
// The actual storage is defined as an inline variable; this forces the linker
// to merge all instances instantiated with a given set of parameters, which
// helps greatly with binary size.
template <uint32_t expected_num_elements,
          bool element_is_nullable,
          const ContainerValidateParams* element_validate_params>
struct ArrayValidateParamsHolder {
  static inline constexpr ContainerValidateParams kInstance = {
      .expected_num_elements = expected_num_elements,
      .element_is_nullable = element_is_nullable,
      .key_validate_params = nullptr,
      .element_validate_params = element_validate_params,
      .validate_enum_func = nullptr,
  };
};

template <uint32_t expected_num_elements,
          bool element_is_nullable,
          ValidateEnumFunc* validate_enum_func>
struct ArrayOfEnumsValidateParamsHolder {
  static_assert(validate_enum_func);
  static inline constexpr ContainerValidateParams kInstance = {
      .expected_num_elements = expected_num_elements,
      .element_is_nullable = element_is_nullable,
      .key_validate_params = nullptr,
      .element_validate_params = nullptr,
      .validate_enum_func = validate_enum_func,
  };
};

template <const ContainerValidateParams& key_validate_params,
          const ContainerValidateParams& element_validate_params>
struct MapValidateParamsHolder {
  static inline constexpr ContainerValidateParams kInstance = {
      .expected_num_elements = 0,
      .element_is_nullable = false,
      .key_validate_params = &key_validate_params,
      .element_validate_params = &element_validate_params,
      .validate_enum_func = nullptr,
  };
};

// Gets a validator for an array. If `expected_num_elements` is 0, size
// validation is skipped. `element_validate_params` may optionally be null, e.g.
// for an array of uint8_t, no nested validation of uint8_t is required.
template <uint32_t expected_num_elements,
          bool element_is_nullable,
          const ContainerValidateParams* element_validate_params>
constexpr const ContainerValidateParams& GetArrayValidator() {
  return ArrayValidateParamsHolder<expected_num_elements, element_is_nullable,
                                   element_validate_params>::kInstance;
}

// Gets a validator for an array of enums. If `expected_num_elements` is 0, size
// validation is skipped. `validate_enum_func` must not be null.',
template <uint32_t expected_num_elements,
          bool element_is_nullable,
          ValidateEnumFunc* validate_enum_func>
constexpr const ContainerValidateParams& GetArrayOfEnumsValidator() {
  static_assert(validate_enum_func);
  return ArrayOfEnumsValidateParamsHolder<expected_num_elements,
                                          element_is_nullable,
                                          validate_enum_func>::kInstance;
}

// Gets a validator for a map. Internally, a map is represented as an array of
// keys and an array of values.
template <const ContainerValidateParams& key_validate_params,
          const ContainerValidateParams& element_validate_params>
constexpr const ContainerValidateParams& GetMapValidator() {
  return MapValidateParamsHolder<key_validate_params,
                                 element_validate_params>::kInstance;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
