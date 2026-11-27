// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

namespace blink {

AppliedTextDecoration::AppliedTextDecoration(
    TextDecorationLine line,
    ETextDecorationStyle style,
    Color color,
    TextDecorationThickness thickness,
    Length underline_offset,
    TextDecorationInset decoration_inset,
    EBoxDecorationBreak box_decoration_break)
    : AppliedTextDecoration(line,
                            style,
                            color,
                            color,
                            /*is_visited=*/false,
                            thickness,
                            underline_offset,
                            decoration_inset,
                            box_decoration_break) {}

AppliedTextDecoration::AppliedTextDecoration(
    TextDecorationLine line,
    ETextDecorationStyle style,
    Color unvisited_color,
    Color visited_color,
    bool is_visited,
    TextDecorationThickness thickness,
    Length underline_offset,
    TextDecorationInset decoration_inset,
    EBoxDecorationBreak box_decoration_break)
    : lines_(static_cast<unsigned>(line)),
      style_(static_cast<unsigned>(style)),
      box_decoration_break_(static_cast<unsigned>(box_decoration_break)),
      is_visited_(is_visited),
      color_(unvisited_color),
      visited_color_(visited_color),
      thickness_(thickness),
      underline_offset_(underline_offset),
      decoration_inset_(decoration_inset) {}

bool AppliedTextDecoration::operator==(
    const AppliedTextDecoration& other) const {
  // Intentionally ignore `is_visited_` so equality does not depend on :visited
  // link state.
  return lines_ == other.lines_ && style_ == other.style_ &&
         box_decoration_break_ == other.box_decoration_break_ &&
         color_ == other.color_ && visited_color_ == other.visited_color_ &&
         thickness_ == other.thickness_ &&
         underline_offset_ == other.underline_offset_ &&
         decoration_inset_ == other.decoration_inset_ &&
         precomputed_hover_state_ == other.precomputed_hover_state_;
}

}  // namespace blink
