// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_NATIVE_ENUM_H_
#define MOJO_PUBLIC_CPP_BINDINGS_NATIVE_ENUM_H_

#include <functional>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/native_enum_data.h"

namespace mojo {

// Native-only enums correspond to "[Native] enum Foo;" definitions in mojom.
enum class NativeEnum : int32_t {};

}  // namespace mojo

namespace std {

template <>
struct hash<mojo::NativeEnum>
    : public mojo::internal::EnumHashImpl<mojo::NativeEnum> {};

}  // namespace std

#endif  // MOJO_PUBLIC_CPP_BINDINGS_NATIVE_ENUM_H_
