// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_
#define EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_

#include <tuple>
#include <vector>

#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

class SkBitmap;

namespace base {
class FilePath;
}

namespace extensions {

using DecodedImages = std::vector<std::tuple<SkBitmap, base::FilePath>>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_UTILITY_TYPES_H_
