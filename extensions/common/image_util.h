// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_IMAGE_UTIL_H_
#define EXTENSIONS_COMMON_IMAGE_UTIL_H_

class SkBitmap;

using SkColor = unsigned int;

namespace base {
class FilePath;
}

// This file contains various utility functions for extension images and colors.
namespace extensions::image_util {

// Returns whether an icon image is considered to be visible in its display
// context.
bool IsIconSufficientlyVisible(const SkBitmap& bitmap);

// Returns whether an icon image is considered to be visible in its display
// context.
bool IsIconAtPathSufficientlyVisible(const base::FilePath& path);

// This is the color of the toolbar in the default scheme. There is a unit test
// to catch any changes to this value.
extern const SkColor kDefaultToolbarColor;

// Renders the icon bitmap onto another bitmap, combining it with the specified
// background color, then determines whether the rendered icon is sufficiently
// visible against the background.
bool IsRenderedIconSufficientlyVisible(const SkBitmap& bitmap,
                                       SkColor background_color);

// Returns whether an icon image is considered to be visible in its display
// context, according to the previous function.
bool IsRenderedIconAtPathSufficientlyVisible(const base::FilePath& path,
                                             SkColor background_color);

// Icons should be a reasonable size for analysis. There have been crash
// reports due to memory allocation issues with calls to
// SkBitmap::allocN32Pixels. See crbug.com/1155746.
inline constexpr int kMaxAllowedPixels = 2048 * 2048;

// Renders the icon bitmap onto another bitmap, combining it with the specified
// background color. The output bitmap must be empty.
[[nodiscard]] bool RenderIconForVisibilityAnalysis(const SkBitmap& icon,
                                                   SkColor background_color,
                                                   SkBitmap* rendered_icon);

// Load a PNG image from a file into the destination bitmap.
bool LoadPngFromFile(const base::FilePath& path, SkBitmap* dst);

}  // namespace extensions::image_util

#endif  // EXTENSIONS_COMMON_IMAGE_UTIL_H_
