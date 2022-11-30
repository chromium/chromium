// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_DATA_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_DATA_H_

namespace mojo {
namespace internal {

class ValidationContext;

class NativeEnum_Data {
 public:
  static bool const kIsExtensible = true;

  static bool IsKnownValue(int32_t value) { return false; }

  static bool Validate(int32_t value,
                       ValidationContext* validation_context) { return true; }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_ENUM_DATA_H_
