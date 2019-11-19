// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom string.
//
// An example specialization for CustomString:
//
//   template <T>
//   struct StringTraits<CustomString> {
//     // These two methods are optional. Please see comments in struct_traits.h
//     static bool IsNull(const CustomString& input);
//     static void SetToNull(CustomString* output);
//
//     // This doesn't need to be a base::StringPiece; it simply needs to be a
//     // type that exposes a data() method that returns a pointer to the UTF-8
//     // bytes and a size() method that returns the length of the UTF-8 bytes.
//     static std::span<char> GetUTF8(const CustomString& input);
//
//     // The caller guarantees that |!input.is_null()|.
//     static bool Read(StringDataView input, CustomString* output);
//   };
template <typename T>
struct StringTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::StringTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_H_
