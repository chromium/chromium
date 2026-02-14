// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/fixed_array_size_unittest_mojom_traits.h"

namespace mojo::test::fixed_array_size_unittest {

TypemappedStruct3::TypemappedStruct3() = default;
TypemappedStruct3::~TypemappedStruct3() = default;
TypemappedStruct3::TypemappedStruct3(const TypemappedStruct3&) = default;
TypemappedStruct3& TypemappedStruct3::operator=(const TypemappedStruct3&) =
    default;
TypemappedStruct3::TypemappedStruct3(TypemappedStruct3&&) = default;
TypemappedStruct3& TypemappedStruct3::operator=(TypemappedStruct3&&) = default;

}  // namespace mojo::test::fixed_array_size_unittest
