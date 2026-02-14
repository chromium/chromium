// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_FIXED_ARRAY_SIZE_UNITTEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_FIXED_ARRAY_SIZE_UNITTEST_MOJOM_TRAITS_H_

#include <vector>

#include "base/check_op.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/fixed_array_size_unittest.test-mojom-shared.h"

namespace mojo {

namespace test::fixed_array_size_unittest {

struct TypemappedStruct3 {
  TypemappedStruct3();
  ~TypemappedStruct3();

  TypemappedStruct3(const TypemappedStruct3&);
  TypemappedStruct3& operator=(const TypemappedStruct3&);
  TypemappedStruct3(TypemappedStruct3&&);
  TypemappedStruct3& operator=(TypemappedStruct3&&);

  std::vector<int> values;
};

}  // namespace test::fixed_array_size_unittest

template <>
struct StructTraits<test::fixed_array_size_unittest::mojom::Struct3DataView,
                    test::fixed_array_size_unittest::TypemappedStruct3> {
  using CppType = test::fixed_array_size_unittest::TypemappedStruct3;
  using MojoViewType = test::fixed_array_size_unittest::mojom::Struct3DataView;

  static base::span<const int> values(const CppType& in) { return in.values; }

  static bool Read(MojoViewType in, CppType* out) {
    ArrayDataView<int> values;
    in.GetValuesDataView(&values);
    CHECK_EQ(3U, values.size());
    out->values.assign(values.begin(), values.end());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_FIXED_ARRAY_SIZE_UNITTEST_MOJOM_TRAITS_H_
