// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_

#include <string>

#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<std::string> {
  static const std::string& GetUTF8(const std::string& input) { return input; }

  static bool Read(StringDataView input, std::string* output) {
    output->assign(input.storage(), input.size());
    return true;
  }

  static bool IsValidUTF8(const std::string& value) {
    return base::IsStringUTF8(value);
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STL_H_
