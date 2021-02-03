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

AccessibilityLinkInfo::AccessibilityLinkInfo() = default;

AccessibilityLinkInfo::AccessibilityLinkInfo(
    const std::string& url,
    uint32_t index_in_page,
    const gfx::RectF& bounds,
    const AccessibilityTextRunRangeInfo& text_range)
    : url(url),
      index_in_page(index_in_page),
      bounds(bounds),
      text_range(text_range) {}

AccessibilityLinkInfo::AccessibilityLinkInfo(
    const AccessibilityLinkInfo& other) = default;

AccessibilityLinkInfo::~AccessibilityLinkInfo() = default;

AccessibilityImageInfo::AccessibilityImageInfo() = default;

AccessibilityImageInfo::AccessibilityImageInfo(const std::string& alt_text,
                                               uint32_t text_run_index,
                                               const gfx::RectF& bounds)
    : alt_text(alt_text), text_run_index(text_run_index), bounds(bounds) {}

AccessibilityImageInfo::AccessibilityImageInfo(
    const AccessibilityImageInfo& other) = default;

AccessibilityImageInfo::~AccessibilityImageInfo() = default;

}  // namespace chrome_pdf
