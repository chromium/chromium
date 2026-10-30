// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/shaped_text.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

void ShapedText::Trace(Visitor* visitor) const {
  visitor->Trace(font_);
  visitor->Trace(runs_with_offset_);
}

ShapedText::ShapedText() = default;

ShapedText::ShapedText(const Font* font,
                       const TextDirection& direction,
                       const V8CanvasTextBaseline::Enum baseline,
                       const V8CanvasTextAlign::Enum align,
                       const String& text,
                       PlainTextPainter& text_painter) {
  Update(font, direction, baseline, align, text, text_painter);
}

void ShapedText::Update(const Font* font,
                        const TextDirection& direction,
                        const V8CanvasTextBaseline::Enum baseline,
                        const V8CanvasTextAlign::Enum align,
                        const String& text,
                        PlainTextPainter& text_painter) {
  const SimpleFontData* font_data = font->PrimaryFont();
  if (!font_data) {
    return;
  }

  text_ = text;
  font_ = font;
  direction_ = direction;

  auto [xpos, glyph_bounds] = MeasureRuns(text_painter);
  double real_width = xpos;
  width_ = real_width;
  glyph_bounds_ = glyph_bounds;

  text_align_dx_ = 0.0f;
  if (align == V8CanvasTextAlign::Enum::kCenter) {
    text_align_dx_ = real_width / 2.0f;
    ctx_text_align_ = V8CanvasTextAlign::Enum::kCenter;
  } else if (align == V8CanvasTextAlign::Enum::kRight ||
             (align == V8CanvasTextAlign::Enum::kStart &&
              direction == TextDirection::kRtl) ||
             (align == V8CanvasTextAlign::Enum::kEnd &&
              direction != TextDirection::kRtl)) {
    text_align_dx_ = real_width;
    ctx_text_align_ = V8CanvasTextAlign::Enum::kRight;
  } else {
    ctx_text_align_ = V8CanvasTextAlign::Enum::kLeft;
  }
  ctx_text_baseline_ = baseline;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent(
      FontBaseline::kAlphabetic, FontMetrics::ApplyBaselineTable(true));
  const float descent = font_metrics.FloatDescent(
      FontBaseline::kAlphabetic, FontMetrics::ApplyBaselineTable(true));
  baseline_y_ = TextMetrics::GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y_;
  font_bounding_box_descent_ = descent + baseline_y_;
}

std::pair<float, gfx::RectF> ShapedText::MeasureRuns(
    PlainTextPainter& text_painter) {
  runs_with_offset_.clear();

  const PlainTextNode& node =
      text_painter.SegmentAndShape(TextRun(text_, direction_), *font_);
  gfx::RectF glyph_bounds;
  float xpos = 0;
  runs_with_offset_.reserve(node.ItemList().size());
  for (const auto& item : node.ItemList()) {
    // Save the run for computing additional metrics.
    const ShapeResult* shape_result = item.GetShapeResult();

    runs_with_offset_.push_back(
        RunWithOffset{.shape_result_ = shape_result,
                      .text_ = item.Text(),
                      .direction_ = item.Direction(),
                      .character_offset_ = item.StartOffset(),
                      .num_characters_ = item.Length(),
                      .x_position_ = xpos});

    // At small font sizes, item.InkBounds() uses BoundsForGlyph which may
    // return integer-precision bounds from Skia (crbug.com/479240778).
    // Recompute bounds using PreciseBoundsForGlyph for accurate
    // actualBoundingBox metrics on measureText().
    constexpr float kSmallFontSizeThreshold = 4.0f;
    const SimpleFontData* primary_font_data = font_->PrimaryFont();
    gfx::RectF run_glyph_bounds;
    if (RuntimeEnabledFeatures::CanvasTextMetricsPreciseBoundsEnabled() &&
        primary_font_data &&
        primary_font_data->PlatformData().size() <= kSmallFontSizeThreshold) {
      shape_result->ForEachGlyph(
          xpos, 0, item.Length(), 0,
          [](void* context, unsigned character_index, Glyph glyph,
             gfx::Vector2dF glyph_offset, float total_advance,
             bool is_horizontal, CanvasRotationInVertical rotation,
             const SimpleFontData* font_data) {
            auto* bounds = static_cast<gfx::RectF*>(context);
            gfx::RectF glyph_bounds = font_data->PreciseBoundsForGlyph(glyph);
            glyph_bounds.Offset(total_advance, 0.0);
            glyph_bounds.Offset(glyph_offset);
            bounds->Union(glyph_bounds);
          },
          static_cast<void*>(&run_glyph_bounds));
    } else {
      // Accumulate the position and the glyph bounding box.
      run_glyph_bounds = item.InkBounds();
      run_glyph_bounds.Offset(xpos, 0);
    }
    glyph_bounds.Union(run_glyph_bounds);
    xpos += shape_result->Width();
  }
  return {xpos, glyph_bounds};
}

const HeapVector<Member<DOMRectReadOnly>> ShapedText::getSelectionRects(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  HeapVector<Member<DOMRectReadOnly>> selection_rects;
  Vector<TextDirection> direction_list;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start > text_.length() || end > text_.length()) {
    const bool is_start = start > text_.length();
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound(is_start ? "start" : "end",
                                                    is_start ? start : end,
                                                    text_.length()));
    return selection_rects;
  }

  const double height = font_bounding_box_ascent_ + font_bounding_box_descent_;
  const double y = -font_bounding_box_ascent_;

  for (const auto& run_with_offset : runs_with_offset_) {
    const unsigned int run_start_index = run_with_offset.character_offset_;
    const unsigned int run_end_index =
        run_start_index + run_with_offset.num_characters_;

    // Handle start >= end case the same way the DOM does, returning a
    // zero-width rect after the advance of the character right before the end
    // position. If the position is mid-cluster, the whole cluster is added as a
    // rect.
    if (start >= end) {
      if (run_start_index <= end && end <= run_end_index) {
        const unsigned int index =
            base::CheckSub(end, run_start_index).ValueOrDie();
        float from_x =
            run_with_offset.shape_result_->CaretPositionForOffset(
                index, run_with_offset.text_, AdjustMidCluster::kToStart) +
            run_with_offset.x_position_;
        float to_x =
            run_with_offset.shape_result_->CaretPositionForOffset(
                index, run_with_offset.text_, AdjustMidCluster::kToEnd) +
            run_with_offset.x_position_;
        if (from_x < to_x) {
          selection_rects.push_back(DOMRectReadOnly::Create(
              from_x - text_align_dx_, y, to_x - from_x, height));
        } else {
          selection_rects.push_back(DOMRectReadOnly::Create(
              to_x - text_align_dx_, y, from_x - to_x, height));
        }
        direction_list.push_back(run_with_offset.direction_);
      }
      continue;
    }

    // Outside the required interval.
    if (run_end_index <= start || run_start_index >= end) {
      continue;
    }

    // Calculate the required indexes for this specific run.
    const unsigned int starting_index =
        start > run_start_index ? start - run_start_index : 0;
    const unsigned int ending_index = end < run_end_index
                                          ? end - run_start_index
                                          : run_with_offset.num_characters_;

    // Use caret positions to determine the start and end of the selection rect.
    float from_x =
        run_with_offset.shape_result_->CaretPositionForOffset(
            starting_index, run_with_offset.text_, AdjustMidCluster::kToStart) +
        run_with_offset.x_position_;
    float to_x =
        run_with_offset.shape_result_->CaretPositionForOffset(
            ending_index, run_with_offset.text_, AdjustMidCluster::kToEnd) +
        run_with_offset.x_position_;
    if (from_x < to_x) {
      selection_rects.push_back(DOMRectReadOnly::Create(
          from_x - text_align_dx_, y, to_x - from_x, height));
    } else {
      selection_rects.push_back(DOMRectReadOnly::Create(
          to_x - text_align_dx_, y, from_x - to_x, height));
    }
    direction_list.push_back(run_with_offset.direction_);
  }

  // Merges touching rectangles. Rectangles in `selection_rects` are
  // unnecessarily split due to per-word ShapeResults. This is an internal
  // detail and should be hidden from the web API.
  //
  // Test:
  // external/wpt/html/canvas/element/text/2d.text.measure.selection-rects.tentative.html
  if (selection_rects.size() >= 2) {
    DCHECK_EQ(selection_rects.size(), direction_list.size());
    auto approximately_equal = [](double v1, double v2) {
      return std::abs(v1 - v2) <= 0.1;
    };
    for (wtf_size_t i = selection_rects.size() - 1; i > 0; --i) {
      if (direction_list[i] != direction_list[i - 1]) {
        continue;
      }
      const DOMRectReadOnly& rhs = *selection_rects[i];
      const DOMRectReadOnly& lhs = *selection_rects[i - 1];
      if (approximately_equal(rhs.right(), lhs.left())) {
        selection_rects[i - 1] = DOMRectReadOnly::Create(
            rhs.left(), rhs.top(), lhs.right() - rhs.left(), rhs.height());
        selection_rects.EraseAt(i);
        direction_list.EraseAt(i);
      } else if (approximately_equal(rhs.left(), lhs.right())) {
        selection_rects[i - 1] = DOMRectReadOnly::Create(
            lhs.left(), lhs.top(), rhs.right() - lhs.left(), lhs.height());
        selection_rects.EraseAt(i);
        direction_list.EraseAt(i);
      }
    }
  }
  return selection_rects;
}

DOMRectReadOnly* ShapedText::getActualBoundingBox(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  gfx::RectF bounding_box;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start >= text_.length() || end > text_.length()) {
    const bool is_start = start >= text_.length();
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound(is_start ? "start" : "end",
                                                    is_start ? start : end,
                                                    text_.length()));
    return DOMRectReadOnly::FromRectF(bounding_box);
  }

  for (const auto& run_with_offset : runs_with_offset_) {
    const unsigned int run_start_index = run_with_offset.character_offset_;
    const unsigned int run_end_index =
        run_start_index + run_with_offset.num_characters_;

    // Outside the required interval.
    if (run_end_index <= start || run_start_index >= end) {
      continue;
    }

    // Position of the left border for this run.
    const double left_border = run_with_offset.x_position_;

    // Calculate the required indexes for this specific run.
    const unsigned int starting_index =
        start > run_start_index ? start - run_start_index : 0;
    const unsigned int ending_index = end < run_end_index
                                          ? end - run_start_index
                                          : run_with_offset.num_characters_;

    const ShapeResultView* view = ShapeResultView::Create(
        run_with_offset.shape_result_, 0, run_with_offset.num_characters_);
    view->ForEachGlyph(
        left_border, starting_index, ending_index, 0,
        [](void* context, unsigned character_index, Glyph glyph,
           gfx::Vector2dF glyph_offset, float total_advance, bool is_horizontal,
           CanvasRotationInVertical rotation, const SimpleFontData* font_data) {
          auto* bounding_box = static_cast<gfx::RectF*>(context);
          // At small font sizes (<=4px), Skia stores glyph bounds as
          // integers, causing relative errors (crbug.com/479240778).
          // Use path-based float-precision bounds for small sizes.
          constexpr float kSmallFontSizeThreshold = 4.0f;
          gfx::RectF glyph_bounds =
              RuntimeEnabledFeatures::CanvasTextMetricsPreciseBoundsEnabled() &&
                      font_data->PlatformData().size() <=
                          kSmallFontSizeThreshold
                  ? font_data->PreciseBoundsForGlyph(glyph)
                  : font_data->BoundsForGlyph(glyph);
          glyph_bounds.Offset(total_advance, 0.0);
          glyph_bounds.Offset(glyph_offset);
          bounding_box->Union(glyph_bounds);
        },
        static_cast<void*>(&bounding_box));
  }
  bounding_box.Offset(-text_align_dx_, baseline_y_);
  return DOMRectReadOnly::FromRectF(bounding_box);
}

unsigned ShapedText::getIndexFromOffset(double x) {
  if (runs_with_offset_.empty()) {
    return 0;
  }

  // x is visual direction from the alignment point, regardless of the text
  // direction. Note x can be negative, to enable positions to the left of the
  // alignment point.
  float target_x = text_align_dx_ + x;

  // If to the left (or right), clamp to the left (or right) point
  if (target_x <= 0) {
    target_x = 0;
  }
  if (target_x >= width_) {
    target_x = width_;
  }

  for (HeapVector<RunWithOffset>::reverse_iterator riter =
           runs_with_offset_.rbegin();
       riter != runs_with_offset_.rend(); riter++) {
    if (riter->x_position_ <= target_x) {
      float run_x = target_x - riter->x_position_;
      unsigned run_offset = riter->shape_result_->CaretOffsetForHitTest(
          run_x, StringView(riter->text_));
      if (direction_ != riter->direction_) {
        return CorrectForMixedBidi(riter, run_offset);
      }
      return run_offset + riter->character_offset_;
    }
  }
  return 0;
}

unsigned ShapedText::CorrectForMixedBidi(
    HeapVector<RunWithOffset>::reverse_iterator& riter,
    unsigned run_offset) {
  DCHECK(direction_ != riter->direction_);
  // Do our best to handle mixed direction strings. The decisions to adjust
  // are based on trying to get reasonable selection behavior when there
  // are LTR runs embedded in an RTL string or vice versa.
  if (IsRtl(direction_)) {
    if (run_offset == 0) {
      // Position is at the left edge of a LTR run within an RTL string.
      // Move it to the start of the next RTL run on its left.
      auto next_run = riter + 1;
      if (next_run != runs_with_offset_.rend()) {
        if (IsRtl(next_run->direction_)) {
          return next_run->character_offset_;
        }
      }
    } else if (run_offset == riter->num_characters_) {
      // Position is at the right end of an LTR run embedded in RTL. Move
      // it to the last position of the RTL run to the right, which is the first
      // position of the LTR run, unless there is no run to the right.
      if (riter != runs_with_offset_.rbegin()) {
        auto right_run = riter - 1;
        if (IsRtl(right_run->direction_)) {
          //   rtl_run_1, ltr_run_1, ltr_run_2(*riter), rtl_run_2(right_run)
          //                                          ^run_offset
          // In this case, what we'd like to return is
          //   - The first position of ltr_run_1, or
          //   - The last position of rtl_run_2.
          // It's easy to apply the latter.
          return right_run->character_offset_ + right_run->num_characters_;
        }
      }
    }
  } else {
    if (run_offset == 0) {
      // Position is at the right edge of a RTL run within an LTR string.
      // Move it to the start of the next LTR run on its right.
      if (riter != runs_with_offset_.rbegin()) {
        auto previous_run = riter - 1;
        if (IsLtr(previous_run->direction_)) {
          return previous_run->character_offset_;
        }
      }
    } else if (run_offset == riter->num_characters_) {
      // Position is at the left end of an RTL run embedded in LTR. Move
      // it to the last position of the left side LTR run, unless there is
      // no run to the left.
      auto next_run = riter + 1;
      if (next_run != runs_with_offset_.rend()) {
        if (IsLtr(next_run->direction_)) {
          return next_run->character_offset_ + next_run->num_characters_;
        }
      }
    }
  }
  return run_offset + riter->character_offset_;
}

}  // namespace blink
