// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

void ShapeResultBuffer::ComputeRangeIn(const ShapeResult& result,
                                       const gfx::RectF& ink_bounds,
                                       CharacterRangeContext& context) {
  result.EnsureGraphemes(StringView(context.text, context.total_num_characters,
                                    result.NumCharacters()));
  if (context.is_rtl) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    if (!context.from_x && context.from >= 0 &&
        static_cast<unsigned>(context.from) < result.NumCharacters()) {
      context.from = result.NumCharacters() - context.from - 1;
    }
    if (!context.to_x && context.to >= 0 &&
        static_cast<unsigned>(context.to) < result.NumCharacters()) {
      context.to = result.NumCharacters() - context.to - 1;
    }
    context.current_x -= result.Width();
  }
  for (unsigned i = 0; i < result.runs_.size(); i++) {
    if (!result.runs_[i]) {
      continue;
    }
    DCHECK_EQ(context.is_rtl, result.runs_[i]->IsRtl());
    int num_characters = result.runs_[i]->num_characters_;
    if (!context.from_x && context.from >= 0 && context.from < num_characters) {
      context.from_x = result.runs_[i]->XPositionForVisualOffset(
                           context.from, AdjustMidCluster::kToStart) +
                       context.current_x;
    } else {
      context.from -= num_characters;
    }

    if (!context.to_x && context.to >= 0 && context.to < num_characters) {
      context.to_x = result.runs_[i]->XPositionForVisualOffset(
                         context.to, AdjustMidCluster::kToEnd) +
                     context.current_x;
    } else {
      context.to -= num_characters;
    }

    if (context.from_x || context.to_x) {
      context.min_y = std::min(context.min_y, ink_bounds.y());
      context.max_y = std::max(context.max_y, ink_bounds.bottom());
    }

    if (context.from_x && context.to_x) {
      break;
    }
    context.current_x += result.runs_[i]->width_;
  }
  if (context.is_rtl) {
    context.current_x -= result.Width();
  }
  context.total_num_characters += result.NumCharacters();
}

}  // namespace blink
