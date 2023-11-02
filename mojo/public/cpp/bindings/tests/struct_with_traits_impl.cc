// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"

namespace mojo {
namespace test {

NestedStructWithTraitsImpl::NestedStructWithTraitsImpl() = default;
NestedStructWithTraitsImpl::NestedStructWithTraitsImpl(int32_t in_value)
    : value(in_value) {}

StructWithTraitsImpl::StructWithTraitsImpl() = default;

StructWithTraitsImpl::~StructWithTraitsImpl() = default;

StructWithTraitsImpl::StructWithTraitsImpl(const StructWithTraitsImpl& other) =
    default;

MoveOnlyStructWithTraitsImpl::MoveOnlyStructWithTraitsImpl() = default;

MoveOnlyStructWithTraitsImpl::MoveOnlyStructWithTraitsImpl(
    MoveOnlyStructWithTraitsImpl&& other) = default;

MoveOnlyStructWithTraitsImpl::~MoveOnlyStructWithTraitsImpl() = default;

MoveOnlyStructWithTraitsImpl& MoveOnlyStructWithTraitsImpl::operator=(
    MoveOnlyStructWithTraitsImpl&& other) = default;

UnionWithTraitsInt32::~UnionWithTraitsInt32() = default;

UnionWithTraitsStruct::~UnionWithTraitsStruct() = default;

StructForceSerializeImpl::StructForceSerializeImpl() = default;

StructForceSerializeImpl::~StructForceSerializeImpl() = default;

StructNestedForceSerializeImpl::StructNestedForceSerializeImpl() = default;

StructNestedForceSerializeImpl::~StructNestedForceSerializeImpl() = default;

}  // namespace test
}  // namespace mojo
