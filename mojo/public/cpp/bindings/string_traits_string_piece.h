// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<base::StringPiece> {
  static bool IsNull(base::StringPiece input) {
    // base::StringPiece is always converted to non-null mojom string. We could
    // have let StringPiece containing a null data pointer map to null mojom
    // string, but StringPiece::empty() returns true in this case. It seems
    // confusing to mix the concept of empty and null strings, especially
    // because they mean different things in mojom.
    return false;
  }

  static void SetToNull(base::StringPiece* output) {
    // Convert null to an "empty" base::StringPiece.
    output->set(nullptr, 0);
  }

  static base::StringPiece GetUTF8(base::StringPiece input) { return input; }

  static bool Read(StringDataView input, base::StringPiece* output) {
    output->set(input.storage(), input.size());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
