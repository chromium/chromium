// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_IMAGE_UTIL_H_
#define EXTENSIONS_COMMON_IMAGE_UTIL_H_

#include <string>

class SkBitmap;

typedef unsigned int SkColor;

namespace base {
class FilePath;
}

// This file contains various utility functions for extension images and colors.
namespace extensions {
namespace image_util {

// Parses a CSS-style color string from hex (3- or 6-digit) or HSL(A) format.
// Returns true on success.
bool ParseCssColorString(const std::string& color_string, SkColor* result);

// Parses a RGB or RGBA string like #FF9982CC, #FF9982, #EEEE, or #EEE to a
// color. Returns true for success.
bool ParseHexColorString(const std::string& color_string, SkColor* result);

// Parses rgb() or rgba() string to a color. Returns true for success.
bool ParseRgbColorString(const std::string& color_string, SkColor* result);

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

// Renders the icon bitmap onto another bitmap, combining it with the specified
// background color. The output bitmap must be empty.
void RenderIconForVisibilityAnalysis(const SkBitmap& icon,
                                     SkColor background_color,
                                     SkBitmap* rendered_icon);

// Load a PNG image from a file into the destination bitmap.
bool LoadPngFromFile(const base::FilePath& path, SkBitmap* dst);

}  // namespace image_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_IMAGE_UTIL_H_
