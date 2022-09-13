// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_
#define EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_

#include <tuple>
#include <vector>

class SkBitmap;

namespace base {
class FilePath;
}

namespace extensions {

using DecodedImages = std::vector<std::tuple<SkBitmap, base::FilePath>>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_
