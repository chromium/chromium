// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_string.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace extensions {

bool ContainsStringIgnoreCaseASCII(const std::set<std::string>& collection,
                                   const std::string& value) {
  return std::find_if(std::begin(collection), std::end(collection),
                      [&value](const std::string& s) -> bool {
                        return base::EqualsCaseInsensitiveASCII(s, value);
                      }) != std::end(collection);
}

}  // namespace extensions
