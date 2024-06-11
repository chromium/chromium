// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_

#include <string_view>

#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<std::string_view> {
  static bool IsNull(std::string_view input) {
    // std::string_view is always converted to non-null mojom string. We could
    // have let std::string_view containing a null data pointer map to null
    // mojom string, but StringPiece::empty() returns true in this case. It
    // seems confusing to mix the concept of empty and null strings, especially
    // because they mean different things in mojom.
    return false;
  }

  static void SetToNull(std::string_view* output) {
    // Convert null to an "empty" std::string_view.
    *output = std::string_view();
  }

  static std::string_view GetUTF8(std::string_view input) { return input; }

  static bool Read(StringDataView input, std::string_view* output) {
    *output = std::string_view(input.storage(), input.size());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
