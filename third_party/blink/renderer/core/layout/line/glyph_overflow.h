/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_GLYPH_OVERFLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_GLYPH_OVERFLOW_H_

#include <algorithm>
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct GlyphOverflow {
  GlyphOverflow() : left(0), right(0), top(0), bottom(0) {}

  bool IsApproximatelyZero() const {
    // Overflow can be expensive so we try to avoid it. Small amounts of
    // overflow is imperceptible and is typically masked by pixel snapping.
    static const float kApproximatelyNoOverflow = 0.0625f;
    return std::fabs(left) < kApproximatelyNoOverflow &&
           std::fabs(right) < kApproximatelyNoOverflow &&
           std::fabs(top) < kApproximatelyNoOverflow &&
           std::fabs(bottom) < kApproximatelyNoOverflow;
  }

  // Compute GlyphOverflow from glyph bounding box. The |bounds| should be in
  // logical coordinates, using alphabetic baseline. See ShapeResult::Bounds().
  void SetFromBounds(const FloatRect& bounds,
                     const Font& font,
                     float text_width) {
    const SimpleFontData* font_data = font.PrimaryFont();
    DCHECK(font_data);
    float ascent = 0;
    float descent = 0;
    if (font_data) {
      FontBaseline baseline_type = kAlphabeticBaseline;
      ascent = font_data->GetFontMetrics().FloatAscent(baseline_type);
      descent = font_data->GetFontMetrics().FloatDescent(baseline_type);
    }
    top = std::max(0.0f, -bounds.Y() - ascent);
    bottom = std::max(0.0f, bounds.MaxY() - descent);
    left = std::max(0.0f, -bounds.X());
    right = std::max(0.0f, bounds.MaxX() - text_width);
  }

  // Top and bottom are the amounts of glyph overflows exceeding the font
  // metrics' ascent and descent, respectively. Left and right are the amounts
  // of glyph overflows exceeding the left and right edge of normal layout
  // boundary, respectively.
  float left;
  float right;
  float top;
  float bottom;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_GLYPH_OVERFLOW_H_
