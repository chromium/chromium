// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_NUMERICS_AND_ENUMS_UNITTEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_NUMERICS_AND_ENUMS_UNITTEST_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace test::nullable_numerics_and_enums_unittest {

// For l̶a̶z̶i̶n̶e̶s̶s̶convenience, the C++ type is simply defined in the traits header.
// This should never be done in non-test code.
enum class TypemappedEnum : uint64_t {
  // These values are intentionally distinct from the Mojo enum definition.
  kValueOne = uint64_t{1} << 33,
  kValueTwo = uint64_t{1} << 34,
};

}  // namespace test::nullable_numerics_and_enums_unittest

template <>
struct EnumTraits<
    test::nullable_numerics_and_enums_unittest::mojom::TypemappedEnum,
    test::nullable_numerics_and_enums_unittest::TypemappedEnum> {
  using CppType =
      ::mojo::test::nullable_numerics_and_enums_unittest::TypemappedEnum;
  using MojomType =
      ::mojo::test::nullable_numerics_and_enums_unittest::mojom::TypemappedEnum;
  static MojomType ToMojom(CppType in) {
    switch (in) {
      case CppType::kValueOne:
        return MojomType::kThisOtherValue;
      case CppType::kValueTwo:
        return MojomType::kThatOtherValue;
    }

    NOTREACHED();
    return MojomType::kMinValue;
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

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_NUMERICS_AND_ENUMS_UNITTEST_MOJOM_TRAITS_H_
