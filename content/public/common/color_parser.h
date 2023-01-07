// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_COLOR_PARSER_H_
#define CONTENT_PUBLIC_COMMON_COLOR_PARSER_H_

#include <string>

#include "content/common/content_export.h"

typedef unsigned int SkColor;

namespace content {

// Parses a CSS-style color string from hex (3- or 6-digit), rgb(), rgba(),
// hsl() or hsla() formats. Returns true on success.
CONTENT_EXPORT bool ParseCssColorString(const std::string& color_string,
                                        SkColor* result);

// Parses a RGB or RGBA string like #FF9982CC, #FF9982, #EEEE, or #EEE to a
// color. Returns true for success.
CONTENT_EXPORT bool ParseHexColorString(const std::string& color_string,
                                        SkColor* result);

// Parses rgb() or rgba() string to a color. Returns true for success.
CONTENT_EXPORT bool ParseRgbColorString(const std::string& color_string,
                                        SkColor* result);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_COLOR_PARSER_H_
