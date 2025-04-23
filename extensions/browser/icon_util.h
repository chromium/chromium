// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ICON_UTIL_H_
#define EXTENSIONS_BROWSER_ICON_UTIL_H_

#include "base/values.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {
enum class IconParseResult {
  kSuccess,
  kDecodeFailure,
  kUnpickleFailure,
};

// Tries to parse |*icon| from a dictionary {"19": imageData19, "38":
// imageData38}, and returns the result of the parsing attempt.
IconParseResult ParseIconFromCanvasDictionary(const base::Value::Dict& dict,
                                              gfx::ImageSkia* icon);
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ICON_UTIL_H_
