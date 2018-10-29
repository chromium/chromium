// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGLineHeightMetrics::NGLineHeightMetrics(const ComputedStyle& style,
                                         FontBaseline baseline_type) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  Initialize(font_data->GetFontMetrics(), baseline_type);
}

NGLineHeightMetrics::NGLineHeightMetrics(const ComputedStyle& style)
    : NGLineHeightMetrics(style, style.GetFontBaseline()) {}

NGLineHeightMetrics::NGLineHeightMetrics(const FontMetrics& font_metrics,
                                         FontBaseline baseline_type) {
  Initialize(font_metrics, baseline_type);
}

void NGLineHeightMetrics::Initialize(const FontMetrics& font_metrics,
                                     FontBaseline baseline_type) {
  // TODO(kojii): In future, we'd like to use LayoutUnit metrics to support
  // sub-CSS-pixel layout.
  ascent = LayoutUnit(font_metrics.Ascent(baseline_type));
  descent = LayoutUnit(font_metrics.Descent(baseline_type));
}

void NGLineHeightMetrics::AddLeading(LayoutUnit line_height) {
  DCHECK(!IsEmpty());
  LayoutUnit half_leading = (line_height - (ascent + descent)) / 2;
  // TODO(kojii): floor() is to make text dump compatible with legacy test
  // results. Revisit when we paint.
  ascent += half_leading.Floor();
  descent = line_height - ascent;
}

void NGLineHeightMetrics::Move(LayoutUnit delta) {
  DCHECK(!IsEmpty());
  ascent -= delta;
  descent += delta;
}

void NGLineHeightMetrics::Unite(const NGLineHeightMetrics& other) {
  ascent = std::max(ascent, other.ascent);
  descent = std::max(descent, other.descent);
}

void NGLineHeightMetrics::operator+=(const NGLineHeightMetrics& other) {
  DCHECK(ascent != LayoutUnit::Min() && descent != LayoutUnit::Min());
  DCHECK(other.ascent != LayoutUnit::Min() &&
         other.descent != LayoutUnit::Min());
  ascent += other.ascent;
  descent += other.descent;
}

std::ostream& operator<<(std::ostream& stream,
                         const NGLineHeightMetrics& metrics) {
  return stream << "ascent=" << metrics.ascent
                << ", descent=" << metrics.descent;
}

}  // namespace blink
