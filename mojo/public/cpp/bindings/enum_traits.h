// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ENUM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ENUM_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as a
// mojom enum |MojomType|. Each specialization needs to implement:
//
//   template <>
//   struct EnumTraits<MojomType, T> {
//     static MojomType ToMojom(T input);
//
//     // Returning false results in deserialization failure and causes the
//     // message pipe receiving it to be disconnected.
//     static bool FromMojom(MojomType input, T* output);
//   };
//
template <typename MojomType, typename T>
struct EnumTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::EnumTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

// No special mapping or validation required if the input and output type are
// identical.
template <typename T>
struct EnumTraits<T, T> {
  static T ToMojom(T input) { return input; }
  static bool FromMojom(T input, T* output) {
    *output = input;
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ENUM_TRAITS_H_
