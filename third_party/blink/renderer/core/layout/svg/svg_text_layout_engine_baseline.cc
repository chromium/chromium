/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_engine_baseline.h"

#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

SVGTextLayoutEngineBaseline::SVGTextLayoutEngineBaseline(const Font& font,
                                                         float effective_zoom)
    : font_(font), effective_zoom_(effective_zoom) {
  DCHECK(effective_zoom_);
}

float SVGTextLayoutEngineBaseline::CalculateBaselineShift(
    const ComputedStyle& style) const {
  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return 0;

  DCHECK(effective_zoom_);
  switch (style.BaselineShiftType()) {
    case EBaselineShiftType::kLength:
      return SVGLengthContext::ValueForLength(
          style.BaselineShift(), style,
          font_.GetFontDescription().ComputedPixelSize() / effective_zoom_);
    case EBaselineShiftType::kSub:
      return -font_data->GetFontMetrics().FloatHeight() / 2 / effective_zoom_;
    case EBaselineShiftType::kSuper:
      return font_data->GetFontMetrics().FloatHeight() / 2 / effective_zoom_;
    default:
      NOTREACHED();
      return 0;
  }
}

EAlignmentBaseline
SVGTextLayoutEngineBaseline::DominantBaselineToAlignmentBaseline(
    bool is_vertical_text,
    LineLayoutItem text_line_layout) const {
  DCHECK(text_line_layout);
  DCHECK(text_line_layout.Style());

  EDominantBaseline baseline = text_line_layout.StyleRef().DominantBaseline();
  if (baseline == EDominantBaseline::kAuto) {
    if (is_vertical_text)
      baseline = EDominantBaseline::kCentral;
    else
      baseline = EDominantBaseline::kAlphabetic;
  }

  switch (baseline) {
    case EDominantBaseline::kUseScript:
      // TODO(fs): The dominant-baseline and the baseline-table components
      // are set by determining the predominant script of the character data
      // content.
      return EAlignmentBaseline::kAlphabetic;
    case EDominantBaseline::kNoChange:
      DCHECK(text_line_layout.Parent());
      return DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                 text_line_layout.Parent());
    case EDominantBaseline::kResetSize:
      DCHECK(text_line_layout.Parent());
      return DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                 text_line_layout.Parent());
    case EDominantBaseline::kIdeographic:
      return EAlignmentBaseline::kIdeographic;
    case EDominantBaseline::kAlphabetic:
      return EAlignmentBaseline::kAlphabetic;
    case EDominantBaseline::kHanging:
      return EAlignmentBaseline::kHanging;
    case EDominantBaseline::kMathematical:
      return EAlignmentBaseline::kMathematical;
    case EDominantBaseline::kCentral:
      return EAlignmentBaseline::kCentral;
    case EDominantBaseline::kMiddle:
      return EAlignmentBaseline::kMiddle;
    case EDominantBaseline::kTextAfterEdge:
      return EAlignmentBaseline::kTextAfterEdge;
    case EDominantBaseline::kTextBeforeEdge:
      return EAlignmentBaseline::kTextBeforeEdge;
    default:
      NOTREACHED();
      return EAlignmentBaseline::kAuto;
  }
}

float SVGTextLayoutEngineBaseline::CalculateAlignmentBaselineShift(
    bool is_vertical_text,
    LineLayoutItem text_line_layout) const {
  DCHECK(text_line_layout);
  DCHECK(text_line_layout.Style());
  DCHECK(text_line_layout.Parent());

  LineLayoutItem text_line_layout_parent = text_line_layout.Parent();
  DCHECK(text_line_layout_parent);

  EAlignmentBaseline baseline = text_line_layout.StyleRef().AlignmentBaseline();
  if (baseline == EAlignmentBaseline::kAuto ||
      baseline == EAlignmentBaseline::kBaseline) {
    baseline = DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                   text_line_layout_parent);
    DCHECK_NE(baseline, EAlignmentBaseline::kAuto);
    DCHECK_NE(baseline, EAlignmentBaseline::kBaseline);
  }

  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return 0;

  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  float ascent = font_metrics.FloatAscent() / effective_zoom_;
  float descent = font_metrics.FloatDescent() / effective_zoom_;
  float xheight = font_metrics.XHeight() / effective_zoom_;

  // Note: http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
  switch (baseline) {
    case EAlignmentBaseline::kBeforeEdge:
    case EAlignmentBaseline::kTextBeforeEdge:
      return ascent;
    case EAlignmentBaseline::kMiddle:
      return xheight / 2;
    case EAlignmentBaseline::kCentral:
      return (ascent - descent) / 2;
    case EAlignmentBaseline::kAfterEdge:
    case EAlignmentBaseline::kTextAfterEdge:
    case EAlignmentBaseline::kIdeographic:
      return -descent;
    case EAlignmentBaseline::kAlphabetic:
      return 0;
    case EAlignmentBaseline::kHanging:
      return ascent * 8 / 10.f;
    case EAlignmentBaseline::kMathematical:
      return ascent / 2;
    case EAlignmentBaseline::kBaseline:
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
