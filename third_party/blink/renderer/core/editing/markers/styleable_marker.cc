// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace blink {

StyleableMarker::StyleableMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 Color underline_color,
                                 ImeTextSpanThickness thickness,
                                 ImeTextSpanUnderlineStyle underline_style,
                                 Color text_color,
                                 Color background_color)
    : DocumentMarker(start_offset, end_offset),
      underline_color_(underline_color),
      background_color_(background_color),
      thickness_(thickness),
      underline_style_(underline_style),
      text_color_(text_color) {}

Color StyleableMarker::UnderlineColor() const {
  return underline_color_;
}

bool StyleableMarker::HasThicknessNone() const {
  return thickness_ == ImeTextSpanThickness::kNone;
}

bool StyleableMarker::HasThicknessThin() const {
  return thickness_ == ImeTextSpanThickness::kThin;
}

bool StyleableMarker::HasThicknessThick() const {
  return thickness_ == ImeTextSpanThickness::kThick;
}

ui::mojom::ImeTextSpanUnderlineStyle StyleableMarker::UnderlineStyle() const {
  return underline_style_;
}

Color StyleableMarker::TextColor() const {
  return text_color_;
}

bool StyleableMarker::UseTextColor() const {
  return thickness_ != ImeTextSpanThickness::kNone &&
         underline_color_ == Color::kTransparent;
}

Color StyleableMarker::BackgroundColor() const {
  return background_color_;
}

bool IsStyleableMarker(const DocumentMarker& marker) {
  DocumentMarker::MarkerType type = marker.GetType();
  return type == DocumentMarker::kComposition ||
         type == DocumentMarker::kActiveSuggestion ||
         type == DocumentMarker::kSuggestion;
}

}  // namespace blink
