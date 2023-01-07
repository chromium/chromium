// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_string.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

namespace extensions {

bool ContainsStringIgnoreCaseASCII(const std::set<std::string>& collection,
                                   const std::string& value) {
  return base::ranges::any_of(collection, [&value](const std::string& s) {
    return base::EqualsCaseInsensitiveASCII(s, value);
  });
}

}  // namespace extensions
