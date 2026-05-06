// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_TEXT_H_
#define PDF_PDF_INK_TEXT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_ink_ids.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

using SkColor = uint32_t;

namespace chrome_pdf {

// These values are persisted in PDFs as integers. Do not change the assigned
// integer values to maintain backward compatibility.
enum class TextTypeface {
  kSansSerif = 0,
  kSerif = 1,
  kMonospace = 2,
};

// These values are persisted in PDFs as integers. Do not change the assigned
// integer values to maintain backward compatibility.
enum class TextAlignment {
  kLeft = 0,
  kCenter = 1,
  kRight = 2,
};

struct InkTextBoxAttributes {
  InkTextBoxAttributes(gfx::RectF rect,
                       SkColor color,
                       float css_font_size,
                       TextTypeface typeface,
                       TextAlignment alignment,
                       int orientation,
                       bool is_bold,
                       bool is_italic,
                       const std::string& text);
  ~InkTextBoxAttributes();

  // `rect` is in CSS screen coordinates.
  gfx::RectF rect;
  SkColor color;
  float css_font_size;
  TextTypeface typeface;
  TextAlignment alignment;
  // `orientation` is in 90-degree units clockwise.
  int orientation;
  bool is_bold;
  bool is_italic;
  std::string text;
};

struct InkTextInfo {
  InkTextInfo(FontId font_id,
              std::vector<uint32_t> glyphs,
              std::vector<float> glyph_positions,
              gfx::RectF location,
              bool is_horizontal);
  InkTextInfo(InkTextInfo&&) noexcept;
  InkTextInfo& operator=(InkTextInfo&&) noexcept;
  ~InkTextInfo();

  static std::vector<InkTextInfo> SplitTypefaceRuns(
      const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
      float effective_zoom);

  FontId font_id;
  std::vector<uint32_t> glyphs;
  // Positions relative to the origin of the `location` rect in CSS pixels.
  // if is_horizontal is true, x-axis, if false, y-axis.
  std::vector<float> glyph_positions;
  // In CSS pixels. Based on top left of screen origin.
  gfx::RectF location;
  bool is_horizontal;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_TEXT_H_
