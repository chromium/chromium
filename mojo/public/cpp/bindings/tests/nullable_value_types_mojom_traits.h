// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/nullable_value_types_enums.h"
#include "mojo/public/interfaces/bindings/tests/nullable_value_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<test::nullable_value_types::mojom::TypemappedEnum,
                  test::nullable_value_types::TypemappedEnum> {
  using CppType = ::mojo::test::nullable_value_types::TypemappedEnum;
  using MojomType = ::mojo::test::nullable_value_types::mojom::TypemappedEnum;
  static MojomType ToMojom(CppType in) {
    switch (in) {
      case CppType::kValueOne:
        return MojomType::kThisOtherValue;
      case CppType::kValueTwo:
        return MojomType::kThatOtherValue;
    }

    NOTREACHED();
  }

  static bool FromMojom(MojomType in, CppType* out) {
    switch (in) {
      case MojomType::kThisOtherValue:
        *out = CppType::kValueOne;
        return true;
      case MojomType::kThatOtherValue:
        *out = CppType::kValueTwo;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_MOJOM_TRAITS_H_
