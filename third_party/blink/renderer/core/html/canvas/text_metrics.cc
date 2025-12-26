// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_text_cluster_options.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/text_cluster.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(
    const V8CanvasTextBaseline::Enum text_baseline,
    const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case V8CanvasTextBaseline::Enum::kTop:
      return font_data.NormalizedTypoAscent().ToFloat();
    case V8CanvasTextBaseline::Enum::kHanging:
      if (font_metrics.HangingBaseline().has_value()) {
        return font_metrics.HangingBaseline().value();
      }
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent(kAlphabeticBaseline,
                                      FontMetrics::ApplyBaselineTable(true)) *
             kHangingAsPercentOfAscent / 100.0;
    case V8CanvasTextBaseline::Enum::kIdeographic:
      if (font_metrics.IdeographicBaseline().has_value()) {
        return font_metrics.IdeographicBaseline().value();
      }
      return -font_metrics.FloatDescent(kAlphabeticBaseline,
                                        FontMetrics::ApplyBaselineTable(true));
    case V8CanvasTextBaseline::Enum::kBottom:
      return -font_data.NormalizedTypoDescent().ToFloat();
    case V8CanvasTextBaseline::Enum::kMiddle: {
      const FontHeight metrics = font_data.NormalizedTypoAscentAndDescent();
      return (metrics.ascent.ToFloat() - metrics.descent.ToFloat()) / 2.0f;
    }
    case V8CanvasTextBaseline::Enum::kAlphabetic:
      if (font_metrics.AlphabeticBaseline().has_value()) {
        return font_metrics.AlphabeticBaseline().value();
      }
      return 0;
    default:
      // Do nothing.
      return 0;
  }
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
  visitor->Trace(font_);
  visitor->Trace(runs_with_offset_);
  ScriptWrappable::Trace(visitor);
}

TextMetrics::TextMetrics() : baselines_(Baselines::Create()) {}

TextMetrics::TextMetrics(const Font* font,
                         const TextDirection& direction,
                         const V8CanvasTextBaseline::Enum baseline,
                         const V8CanvasTextAlign::Enum align,
                         const String& text,
                         PlainTextPainter& text_painter)
    : TextMetrics() {
  Update(font, direction, baseline, align, text, text_painter);
}

void TextMetrics::Update(const Font* font,
                         const TextDirection& direction,
                         const V8CanvasTextBaseline::Enum baseline,
                         const V8CanvasTextAlign::Enum align,
                         const String& text,
                         PlainTextPainter& text_painter) {
  const SimpleFontData* font_data = font->PrimaryFont();
  if (!font_data)
    return;

  text_ = text;
  font_ = font;
  direction_ = direction;

  auto [xpos, glyph_bounds] = MeasureRuns(text_painter);
  double real_width = xpos;
  width_ = real_width;

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
  actual_bounding_box_left_ = -glyph_bounds.x() + text_align_dx_;
  actual_bounding_box_right_ = glyph_bounds.right() - text_align_dx_;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  const float descent = font_metrics.FloatDescent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -glyph_bounds.y() - baseline_y;
  actual_bounding_box_descent_ = glyph_bounds.bottom() + baseline_y;
  // TODO(kojii): We use normalized sTypoAscent/Descent here, but this should be
  // revisited when the spec evolves.
  const FontHeight normalized_typo_metrics =
      font_data->NormalizedTypoAscentAndDescent();
  em_height_ascent_ = normalized_typo_metrics.ascent - baseline_y;
  em_height_descent_ = normalized_typo_metrics.descent + baseline_y;

  // Setting baselines:
  if (font_metrics.AlphabeticBaseline().has_value()) {
    baselines_->setAlphabetic(font_metrics.AlphabeticBaseline().value() -
                              baseline_y);
  } else {
    baselines_->setAlphabetic(-baseline_y);
  }

  if (font_metrics.HangingBaseline().has_value()) {
    baselines_->setHanging(font_metrics.HangingBaseline().value() - baseline_y);
  } else {
    baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                           baseline_y);
  }

  if (font_metrics.IdeographicBaseline().has_value()) {
    baselines_->setIdeographic(font_metrics.IdeographicBaseline().value() -
                               baseline_y);
  } else {
    baselines_->setIdeographic(-descent - baseline_y);
  }
}

std::pair<float, gfx::RectF> TextMetrics::MeasureRuns(
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

    // Accumulate the position and the glyph bounding box.
    gfx::RectF run_glyph_bounds = item.InkBounds();
    run_glyph_bounds.Offset(xpos, 0);
    glyph_bounds.Union(run_glyph_bounds);
    xpos += shape_result->Width();
  }
  return {xpos, glyph_bounds};
}

const HeapVector<Member<DOMRectReadOnly>> TextMetrics::getSelectionRects(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  HeapVector<Member<DOMRectReadOnly>> selection_rects;
  Vector<TextDirection> direction_list;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start > text_.length() || end > text_.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start > text_.length() ? "start" : "end"));
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

DOMRectReadOnly* TextMetrics::getActualBoundingBox(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  gfx::RectF bounding_box;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start >= text_.length() || end > text_.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start >= text_.length() ? "start" : "end"));
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
          gfx::RectF glyph_bounds = font_data->BoundsForGlyph(glyph);
          glyph_bounds.Offset(total_advance, 0.0);
          glyph_bounds.Offset(glyph_offset);
          bounding_box->Union(glyph_bounds);
        },
        static_cast<void*>(&bounding_box));
  }
  bounding_box.Offset(-text_align_dx_, baseline_y);
  return DOMRectReadOnly::FromRectF(bounding_box);
}

namespace {
float getTextAlignDelta(float width,
                        const V8CanvasTextAlign::Enum text_align,
                        const TextDirection& direction) {
  switch (text_align) {
    case V8CanvasTextAlign::Enum::kRight:
      return width;
    case V8CanvasTextAlign::Enum::kCenter:
      return width / 2.0f;
    case V8CanvasTextAlign::Enum::kLeft:
      return 0;
    case V8CanvasTextAlign::Enum::kStart:
      if (IsLtr(direction)) {
        return 0;
      }
      return width;
    case V8CanvasTextAlign::Enum::kEnd:
      if (IsLtr(direction)) {
        return width;
      }
      return 0;
  }
}

float getTextBaselineDelta(float baseline,
                           const V8CanvasTextBaseline::Enum text_baseline,
                           const SimpleFontData& font_data) {
  float new_baseline = TextMetrics::GetFontBaseline(text_baseline, font_data);
  return baseline - new_baseline;
}

struct TextClusterCallbackContext {
  unsigned start_index_;
  float x_position_;
  float width_;

  void Trace(Visitor* visitor) const {}
};
}  // namespace

HeapVector<Member<TextCluster>> TextMetrics::getTextClusters(
    const TextClusterOptions* options) {
  return getTextClustersImpl(0, text_.length(), options,
                             /*exception_state=*/nullptr);
}

HeapVector<Member<TextCluster>> TextMetrics::getTextClusters(
    uint32_t start,
    uint32_t end,
    const TextClusterOptions* options,
    ExceptionState& exception_state) {
  return getTextClustersImpl(start, end, options, &exception_state);
}

HeapVector<Member<TextCluster>> TextMetrics::getTextClustersImpl(
    uint32_t start,
    uint32_t end,
    const TextClusterOptions* options,
    ExceptionState* exception_state) {
  HeapVector<Member<TextCluster>> minimal_clusters, clusters_for_range;
  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start >= text_.length() || end > text_.length()) {
    CHECK(exception_state != nullptr);
    exception_state->ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start >= text_.length() ? "start" : "end"));
    return clusters_for_range;
  }

  V8CanvasTextAlign::Enum cluster_text_align(ctx_text_align_);
  V8CanvasTextBaseline::Enum cluster_text_baseline(ctx_text_baseline_);
  if (options) {
    if (options->hasAlign()) {
      cluster_text_align = options->align().AsEnum();
    }
    if (options->hasBaseline()) {
      cluster_text_baseline = options->baseline().AsEnum();
    }
  }

  for (const auto& run_with_offset : runs_with_offset_) {
    HeapVector<TextClusterCallbackContext> clusters_for_run;

    run_with_offset.shape_result_->ForEachGraphemeClusters(
        StringView(run_with_offset.text_), run_with_offset.x_position_, 0,
        run_with_offset.num_characters_, 0,
        [](void* context, unsigned character_index, float total_advance,
           unsigned graphemes_in_cluster, float cluster_advance,
           CanvasRotationInVertical rotation) {
          auto* clusters =
              static_cast<HeapVector<TextClusterCallbackContext>*>(context);
          TextClusterCallbackContext cluster = {.start_index_ = character_index,
                                                .x_position_ = total_advance,
                                                .width_ = cluster_advance};
          clusters->push_back(cluster);
        },
        &clusters_for_run);

    std::sort(clusters_for_run.begin(), clusters_for_run.end(),
              [](TextClusterCallbackContext a, TextClusterCallbackContext b) {
                return a.start_index_ < b.start_index_;
              });

    for (wtf_size_t i = 0; i < clusters_for_run.size(); i++) {
      TextCluster* text_cluster;
      if (i + 1 < clusters_for_run.size()) {
        text_cluster = TextCluster::Create(
            text_, clusters_for_run[i].x_position_, 0,
            clusters_for_run[i].start_index_,
            clusters_for_run[i + 1].start_index_, cluster_text_align,
            cluster_text_baseline, *this);
      } else {
        text_cluster = TextCluster::Create(
            text_, clusters_for_run[i].x_position_, 0,
            clusters_for_run[i].start_index_, run_with_offset.num_characters_,
            cluster_text_align, cluster_text_baseline, *this);
      }
      text_cluster->OffsetCharacters(run_with_offset.character_offset_);
      text_cluster->OffsetPosition(
          getTextAlignDelta(clusters_for_run[i].width_, cluster_text_align,
                            direction_),
          getTextBaselineDelta(baseline_y, cluster_text_baseline,
                               *font_->PrimaryFont()));
      text_cluster->OffsetPosition(-text_align_dx_, 0);
      minimal_clusters.push_back(text_cluster);
    }
  }

  for (const auto& cluster : minimal_clusters) {
    if (cluster->end() <= start or end <= cluster->start()) {
      continue;
    }
    clusters_for_range.push_back(cluster);
  }
  return clusters_for_range;
}

unsigned TextMetrics::getIndexFromOffset(double x) {
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

unsigned TextMetrics::CorrectForMixedBidi(
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
