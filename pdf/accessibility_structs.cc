// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility_structs.h"

namespace chrome_pdf {

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo() = default;

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const std::string& font_name,
    int font_weight,
    AccessibilityTextRenderMode render_mode,
    float font_size,
    uint32_t fill_color,
    uint32_t stroke_color,
    bool is_italic,
    bool is_bold)
    : font_name(font_name),
      font_weight(font_weight),
      render_mode(render_mode),
      font_size(font_size),
      fill_color(fill_color),
      stroke_color(stroke_color),
      is_italic(is_italic),
      is_bold(is_bold) {}

AccessibilityTextStyleInfo::AccessibilityTextStyleInfo(
    const AccessibilityTextStyleInfo& other) = default;

AccessibilityTextStyleInfo::~AccessibilityTextStyleInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo() = default;

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    uint32_t len,
    const gfx::RectF& bounds,
    AccessibilityTextDirection direction,
    const AccessibilityTextStyleInfo& style)
    : len(len), bounds(bounds), direction(direction), style(style) {}

AccessibilityTextRunInfo::AccessibilityTextRunInfo(
    const AccessibilityTextRunInfo& other) = default;

AccessibilityTextRunInfo::~AccessibilityTextRunInfo() = default;

}  // namespace chrome_pdf
