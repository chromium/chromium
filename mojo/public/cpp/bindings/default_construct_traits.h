// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TRAITS_H_

#include <type_traits>

namespace mojo {

// Mojo deserialization normally requires that types be default constructible.
// For types where default construction does not make sense, the default
// constructor can be restricted to Mojo by marking it protected/private and
// friending `mojo::DefaultConstructTraits`:
//
//   #include "mojo/public/cpp/bindings/default_construct_traits.h"
//
//   class RestrictedDefaultCtor {
//    private:
//     friend mojo::DefaultConstructTraits;
//     RestrictedDefaultCtor() = default;
//   };
//
// TODO(https://crbug.com/1269986): Note that this will not help with array or
// map deserialization, as none of their deserialization traits currently use
// this helper type.
struct DefaultConstructTraits {
  template <typename T>
  static constexpr T CreateInstance() {
    return T();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DEFAULT_CONSTRUCT_TRAITS_H_
