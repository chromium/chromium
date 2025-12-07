// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ICON_UTIL_H_
#define EXTENSIONS_BROWSER_ICON_UTIL_H_

#include "base/values.h"
#include "extensions/common/constants.h"

class GURL;

namespace gfx {
class Image;
class ImageSkia;
}  // namespace gfx

namespace extensions {
enum class IconParseResult {
  kSuccess,
  kDecodeFailure,
  kUnpickleFailure,
};

// Tries to parse `*icon` from a dictionary {"19": imageData19, "38":
// imageData38}, and returns the result of the parsing attempt.
IconParseResult ParseIconFromCanvasDictionary(const base::Value::Dict& dict,
                                              gfx::ImageSkia* icon);

// Creates a placeholder icon image based on the extension's `name` and returns
// a base64 representation of it for the given `size`.
GURL GetPlaceholderIconUrl(extension_misc::ExtensionIcons icon_size,
                           const std::string& name);

// Returns an icon url from the given image.
GURL GetIconUrlFromImage(const gfx::Image& image);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ICON_UTIL_H_
