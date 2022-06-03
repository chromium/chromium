// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_BASE_H_

namespace blink {

// This is the base type for all Web IDL types, such as the ones defined in
// idl_types.h. It is defined in a separate location to avoid circular header
// inclusions when one only needs to check if a type inherits from IDLBase.
struct IDLBase {
  using ImplType = void;
};

// If a child class returns a simple type known at the time it is declared, it
// can inherit from IDLBaseHelper to avoid having to set ImplType on its own.
//
// Example:
// struct IDLDouble final : public IDLBaseHelper<double> {};
template <typename T>
struct IDLBaseHelper : public IDLBase {
  using ImplType = T;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_BASE_H_
