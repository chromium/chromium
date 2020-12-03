// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_STRUCTS_H_
#define PDF_ACCESSIBILITY_STRUCTS_H_

#include <stdint.h>

#include <string>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

struct AccessibilityPageInfo {
  uint32_t page_index = 0;
  gfx::Rect bounds;
  uint32_t text_run_count = 0;
  uint32_t char_count = 0;
};

// TODO(crbug.com/1144444): Remove next line comment after PDF migrates away
// from Pepper.
// Explicitly set all enum values to match enum values in PP_TextRenderingMode.
// See PDF Reference 1.7, page 402, table 5.3.
enum class AccessibilityTextRenderMode {
  kUnknown = -1,
  kFill = 0,
  kStroke = 1,
  kFillStroke = 2,
  kInvisible = 3,
  kFillClip = 4,
  kStrokeClip = 5,
  kFillStrokeClip = 6,
  kClip = 7,
  kMaxValue = kClip,
};

struct AccessibilityTextStyleInfo {
  AccessibilityTextStyleInfo();
  AccessibilityTextStyleInfo(const std::string& font_name,
                             int font_weight,
                             AccessibilityTextRenderMode render_mode,
                             float font_size,
                             uint32_t fill_color,
                             uint32_t stroke_color,
                             bool is_italic,
                             bool is_bold);
  AccessibilityTextStyleInfo(const AccessibilityTextStyleInfo& other);
  ~AccessibilityTextStyleInfo();

  std::string font_name;
  int font_weight = 0;
  AccessibilityTextRenderMode render_mode =
      AccessibilityTextRenderMode::kUnknown;
  float font_size = 0.0f;
  // Colors are ARGB.
  uint32_t fill_color = 0;
  uint32_t stroke_color = 0;
  bool is_italic = false;
  bool is_bold = false;
};

// TODO(crbug.com/1144444): Remove next line comment after PDF migrates away
// from Pepper.
// Explicitly set all enum values to match enum values in PP_PrivateDirection.
enum class AccessibilityTextDirection {
  kNone = 0,
  kLeftToRight = 1,
  kRightToLeft = 2,
  kTopToBottom = 3,
  kBottomToTop = 4,
  kMaxValue = kBottomToTop,
};

struct AccessibilityTextRunInfo {
  AccessibilityTextRunInfo();
  AccessibilityTextRunInfo(uint32_t len,
                           const gfx::RectF& bounds,
                           AccessibilityTextDirection direction,
                           const AccessibilityTextStyleInfo& style);
  AccessibilityTextRunInfo(const AccessibilityTextRunInfo& other);
  ~AccessibilityTextRunInfo();

  uint32_t len = 0;
  gfx::RectF bounds;
  AccessibilityTextDirection direction = AccessibilityTextDirection::kNone;
  struct AccessibilityTextStyleInfo style;
};

struct AccessibilityCharInfo {
  uint32_t unicode_character = 0;
  double char_width = 0.0;
};

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_STRUCTS_H_
