// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace test::default_construct {

// For convenience, the C++ class is simply defined in the traits header. This
// should never be done in non-test code.
class TestStruct {
 public:
  explicit TestStruct(int value) : value_(value) {}

  TestStruct(const TestStruct&) = default;
  TestStruct& operator=(const TestStruct&) = default;

  int value() const { return value_; }

  explicit TestStruct(DefaultConstruct::Tag) {}

 private:
  int value_ = 0;
};

}  // namespace test::default_construct

template <>
struct StructTraits<test::default_construct::mojom::TestStructDataView,
                    test::default_construct::TestStruct> {
  static int value(const test::default_construct::TestStruct& in) {
    return in.value();
  }

  static bool Read(test::default_construct::mojom::TestStructDataView in,
                   test::default_construct::TestStruct* out) {
    *out = test::default_construct::TestStruct(in.value());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_DEFAULT_CONSTRUCT_UNITTEST_MOJOM_TRAITS_H_
