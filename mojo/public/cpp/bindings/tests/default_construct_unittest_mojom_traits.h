// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/default_construct_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace test::default_construct {

// For convenience, the C++ struct is simply defined in the traits header. This
// should never be done in non-test code.
struct TestStruct {
  explicit TestStruct(int value) : value(value) {}

  TestStruct(const TestStruct&) = default;
  TestStruct& operator=(const TestStruct&) = default;

 public:
  friend mojo::DefaultConstructTraits;

  TestStruct() = default;

  int value = 0;
};

}  // namespace test::default_construct

template <>
struct StructTraits<test::default_construct::mojom::TestStructDataView,
                    test::default_construct::TestStruct> {
  static int value(const test::default_construct::TestStruct& in) {
    return in.value;
  }

  static bool Read(test::default_construct::mojom::TestStructDataView in,
                   test::default_construct::TestStruct* out) {
    out->value = in.value();
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_
