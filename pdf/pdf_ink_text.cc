// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_text.h"

namespace chrome_pdf {

InkTextBoxAttributes::InkTextBoxAttributes(gfx::RectF rect,
                                           SkColor color,
                                           float css_font_size,
                                           TextTypeface typeface,
                                           TextAlignment alignment,
                                           int orientation,
                                           bool is_bold,
                                           bool is_italic)
    : rect(rect),
      color(color),
      css_font_size(css_font_size),
      typeface(typeface),
      alignment(alignment),
      orientation(orientation),
      is_bold(is_bold),
      is_italic(is_italic) {}
InkTextBoxAttributes::~InkTextBoxAttributes() = default;

InkTextInfo::InkTextInfo(FontId font_id,
                         std::vector<uint32_t> glyphs,
                         std::vector<float> glyph_positions,
                         gfx::RectF location,
                         bool is_horizontal)
    : font_id(font_id),
      glyphs(glyphs),
      glyph_positions(glyph_positions),
      location(location),
      is_horizontal(is_horizontal) {}
InkTextInfo::InkTextInfo(InkTextInfo&&) noexcept = default;
InkTextInfo& InkTextInfo::operator=(InkTextInfo&&) noexcept = default;
InkTextInfo::~InkTextInfo() = default;

std::vector<InkTextInfo> InkTextInfo::SplitTypefaceRuns(
    const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
    float effective_zoom) {
  std::vector<InkTextInfo> results;
  for (const pdf::mojom::InkTextRunPtr& text_run : text_runs) {
    float left_edge = text_run->location.x();
    float prev_right_edge_advance = 0;
    for (size_t i = 0; i < text_run->typeface_runs.size(); ++i) {
      const pdf::mojom::InkTypefaceRunPtr& typeface_run =
          text_run->typeface_runs[i];
      // TODO(crbug.com/510015130): handle vertical text.
      CHECK(typeface_run->is_horizontal);

      const float right_edge_advance =
          i + 1 < text_run->typeface_runs.size() &&
                  !text_run->typeface_runs[i + 1]->glyphs.empty()
              ? text_run->typeface_runs[i + 1]->glyphs.front()->total_advance
              : text_run->location.width();
      const float right_edge = right_edge_advance + text_run->location.x();
      gfx::RectF run_location(left_edge, text_run->location.y(),
                              right_edge - left_edge,
                              text_run->location.height());
      run_location.Scale(1.0 / effective_zoom);
      left_edge = right_edge;

      std::vector<uint32_t> glyphs;
      std::vector<float> glyph_positions;
      glyphs.reserve(typeface_run->glyphs.size());
      glyph_positions.reserve(typeface_run->glyphs.size());
      for (const pdf::mojom::InkGlyphInfoPtr& glyph_info :
           typeface_run->glyphs) {
        gfx::Vector2dF position(glyph_info->offset.x() +
                                    glyph_info->total_advance -
                                    prev_right_edge_advance,
                                glyph_info->offset.y());
        position.Scale(1.0 / effective_zoom);
        glyph_positions.push_back(position.x());
        glyphs.push_back(glyph_info->glyph);
      }
      prev_right_edge_advance = right_edge_advance;

      CHECK_EQ(glyphs.size(), glyph_positions.size());
      if (glyphs.empty()) {
        continue;
      }
      results.emplace_back(FontId(typeface_run->typeface_id), std::move(glyphs),
                           std::move(glyph_positions), run_location,
                           typeface_run->is_horizontal);
    }
  }
  return results;
}

}  // namespace chrome_pdf
