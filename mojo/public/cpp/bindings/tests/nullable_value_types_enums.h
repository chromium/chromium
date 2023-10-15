// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_ENUMS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_ENUMS_H_

namespace mojo::test::nullable_value_types {

enum class TypemappedEnum : uint64_t {
  // These values are intentionally distinct from the Mojo enum definition.
  kValueOne = uint64_t{1} << 33,
  kValueTwo = uint64_t{1} << 34,
};

}  // namespace mojo::test::nullable_value_types

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_NULLABLE_VALUE_TYPES_ENUMS_H_
