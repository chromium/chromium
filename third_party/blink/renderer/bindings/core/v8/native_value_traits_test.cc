// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"

#include <type_traits>

#include "third_party/blink/renderer/bindings/core/v8/idl_types_base.h"

// No gtest tests; only static_assert checks.

namespace blink {

template <>
struct NativeValueTraits<bool> : public NativeValueTraitsBase<bool> {};

static_assert(std::is_same<NativeValueTraits<bool>::ImplType, bool>::value,
              "NativeValueTraitsBase works with non IDLBase-derived types");

struct MyIDLType final : public IDLBaseHelper<char> {};
template <>
struct NativeValueTraits<MyIDLType> : public NativeValueTraitsBase<MyIDLType> {
};

static_assert(std::is_same<NativeValueTraits<MyIDLType>::ImplType, char>::value,
              "NativeValueTraitsBase works with IDLBase-derived types");

}  // namespace blink
