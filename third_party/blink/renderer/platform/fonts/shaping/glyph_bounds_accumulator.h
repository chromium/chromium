/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_BOUNDS_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_BOUNDS_ACCUMULATOR_H_

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// Helper class to accumulate glyph bounding box.
//
// Glyph positions and bounding boxes from HarfBuzz and fonts are in physical
// coordinate, while ShapeResult::glyph_bounding_box_ is in logical coordinate.
// To minimize the number of conversions, this class accumulates the bounding
// boxes in physical coordinate, and convert the accumulated box to logical.
template <bool is_horizontal_run>
struct GlyphBoundsAccumulator {
  // The accumulated glyph bounding box in physical coordinate, until
  // ConvertVerticalRunToLogical().
  //
  // We store this as a set of positions rather than a gfx::RectF,
  // because it is cheaper to do lots of Union operations when stored
  // that way, rather than as the (point, size) storage that RectF uses.
  float min_x = 0;
  float max_x = 0;
  float min_y = 0;
  float max_y = 0;

  // Unite a glyph bounding box to |bounds|.
  void Unite(gfx::RectF bounds_for_glyph,
             float origin,
             GlyphOffset glyph_offset) {
    if (bounds_for_glyph.IsEmpty()) [[unlikely]] {
      return;
    }

    // Glyphs are drawn at |origin + offset|. Move glyph_bounds to that point.
    // All positions in hb_glyph_position_t are relative to the current point.
    // https://behdad.github.io/harfbuzz/harfbuzz-Buffers.html#hb-glyph-position-t-struct
    if constexpr (is_horizontal_run) {
      bounds_for_glyph.set_x(bounds_for_glyph.x() + origin);
    } else {
      bounds_for_glyph.set_y(bounds_for_glyph.y() + origin);
    }
    bounds_for_glyph.Offset(glyph_offset);

    if (min_x == 0 && max_x == 0) [[unlikely]] {
      min_x = bounds_for_glyph.x();
      max_x = bounds_for_glyph.right();
      min_y = bounds_for_glyph.y();
      max_y = bounds_for_glyph.bottom();
    } else {
      min_x = std::min(min_x, bounds_for_glyph.x());
      max_x = std::max(max_x, bounds_for_glyph.right());
      min_y = std::min(min_y, bounds_for_glyph.y());
      max_y = std::max(max_y, bounds_for_glyph.bottom());
    }
  }

  // Convert vertical run glyph bounding box to logical. Horizontal runs do not
  // need conversions because physical and logical are the same.
  void ConvertVerticalRunToLogical(const FontMetrics& font_metrics) {
    // Convert physical glyph_bounding_box to logical.
    std::swap(min_x, min_y);
    std::swap(max_x, max_y);

    // The glyph bounding box of a vertical run uses ideographic central
    // baseline. Adjust the box Y position because the bounding box of a
    // ShapeResult uses alphabetic baseline.
    // See diagrams of base lines at
    // https://drafts.csswg.org/css-writing-modes-3/#intro-baselines
    int baseline_adjust = font_metrics.Ascent(kCentralBaseline) -
                          font_metrics.Ascent(kAlphabeticBaseline);
    min_y += baseline_adjust;
    max_y += baseline_adjust;
  }

  gfx::RectF Bounds() const {
    return gfx::RectF(gfx::PointF(min_x, min_y),
                      gfx::SizeF(max_x - min_x, max_y - min_y));
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_BOUNDS_ACCUMULATOR_H_
