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

#include "third_party/blink/renderer/core/style/svg_computed_style.h"
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
  const SVGComputedStyle& svg_style = style.SvgStyle();
  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return 0;

  DCHECK(effective_zoom_);
  switch (svg_style.BaselineShift()) {
    case BS_LENGTH:
      return SVGLengthContext::ValueForLength(
          svg_style.BaselineShiftValue(), style,
          font_.GetFontDescription().ComputedPixelSize() / effective_zoom_);
    case BS_SUB:
      return -font_data->GetFontMetrics().FloatHeight() / 2 / effective_zoom_;
    case BS_SUPER:
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

  const SVGComputedStyle& style = text_line_layout.StyleRef().SvgStyle();

  EDominantBaseline baseline = style.DominantBaseline();
  if (baseline == DB_AUTO) {
    if (is_vertical_text)
      baseline = DB_CENTRAL;
    else
      baseline = DB_ALPHABETIC;
  }

  switch (baseline) {
    case DB_USE_SCRIPT:
      // TODO(fs): The dominant-baseline and the baseline-table components
      // are set by determining the predominant script of the character data
      // content.
      return AB_ALPHABETIC;
    case DB_NO_CHANGE:
      DCHECK(text_line_layout.Parent());
      return DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                 text_line_layout.Parent());
    case DB_RESET_SIZE:
      DCHECK(text_line_layout.Parent());
      return DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                 text_line_layout.Parent());
    case DB_IDEOGRAPHIC:
      return AB_IDEOGRAPHIC;
    case DB_ALPHABETIC:
      return AB_ALPHABETIC;
    case DB_HANGING:
      return AB_HANGING;
    case DB_MATHEMATICAL:
      return AB_MATHEMATICAL;
    case DB_CENTRAL:
      return AB_CENTRAL;
    case DB_MIDDLE:
      return AB_MIDDLE;
    case DB_TEXT_AFTER_EDGE:
      return AB_TEXT_AFTER_EDGE;
    case DB_TEXT_BEFORE_EDGE:
      return AB_TEXT_BEFORE_EDGE;
    default:
      NOTREACHED();
      return AB_AUTO;
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
  if (baseline == AB_AUTO || baseline == AB_BASELINE) {
    baseline = DominantBaselineToAlignmentBaseline(is_vertical_text,
                                                   text_line_layout_parent);
    DCHECK_NE(baseline, AB_AUTO);
    DCHECK_NE(baseline, AB_BASELINE);
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
    case AB_BEFORE_EDGE:
    case AB_TEXT_BEFORE_EDGE:
      return ascent;
    case AB_MIDDLE:
      return xheight / 2;
    case AB_CENTRAL:
      return (ascent - descent) / 2;
    case AB_AFTER_EDGE:
    case AB_TEXT_AFTER_EDGE:
    case AB_IDEOGRAPHIC:
      return -descent;
    case AB_ALPHABETIC:
      return 0;
    case AB_HANGING:
      return ascent * 8 / 10.f;
    case AB_MATHEMATICAL:
      return ascent / 2;
    case AB_BASELINE:
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
