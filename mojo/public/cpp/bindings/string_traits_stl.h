// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_

#include <string>

#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<std::string> {
  static bool IsNull(const std::string& input) {
    // std::string is always converted to non-null mojom string.
    return false;
  }

  static void SetToNull(std::string* output) {
    // std::string doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static const std::string& GetUTF8(const std::string& input) { return input; }

  static bool Read(StringDataView input, std::string* output) {
    output->assign(input.storage(), input.size());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_
