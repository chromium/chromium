// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/shaped_text.h"

#include <algorithm>
#include <ranges>

#include "base/containers/span.h"
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
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

// The font of each code unit of a run, from a single pass over its glyphs, so
// that looking up the font of each cluster doesn't rescan the run.
class RunFonts {
  STACK_ALLOCATED();

 public:
  explicit RunFonts(const ShapedText::RunWithOffset& run_with_offset)
      : run_with_offset_(run_with_offset) {
    fonts_.resize(run_with_offset.num_characters_);
    run_with_offset.shape_result_->ForEachGlyph(
        0, 0, run_with_offset.num_characters_, 0,
        [](void* context, unsigned character_index, Glyph glyph,
           gfx::Vector2dF glyph_offset, float total_advance, bool is_horizontal,
           CanvasRotationInVertical rotation, const SimpleFontData* font_data) {
          auto& fonts =
              *static_cast<HeapVector<Member<const SimpleFontData>>*>(context);
          // Keep the font of the first glyph visited for each code unit.
          if (!fonts[character_index]) {
            fonts[character_index] = font_data;
          }
        },
        &fonts_);
  }

  // Returns the font of the text-relative [start, end) range of the run: that
  // of its first glyph in visual order, or `default_font` if it has no glyphs.
  const SimpleFontData* ForRange(unsigned start,
                                 unsigned end,
                                 const SimpleFontData* default_font) const {
    // subspan() CHECKs that [start, end) lies within the run.
    const auto range_fonts = base::span(fonts_).subspan(
        start - run_with_offset_.character_offset_, end - start);

    auto first_font = [default_font](auto&& fonts) -> const SimpleFontData* {
      auto it = std::ranges::find_if(
          fonts, [](const auto& font) { return font != nullptr; });
      return it != std::ranges::end(fonts) ? it->Get() : default_font;
    };

    // Glyphs are in visual order: LTR from the first code unit, RTL from the
    // last.
    return run_with_offset_.shape_result_->IsRtl()
               ? first_font(std::views::reverse(range_fonts))
               : first_font(range_fonts);
  }

 private:
  const ShapedText::RunWithOffset& run_with_offset_;
  HeapVector<Member<const SimpleFontData>> fonts_;
};

// Appends the glyph clusters of a run that intersect the text-relative
// [start, end) range to `clusters`, in logical order.
// Returns the number of appended clusters.
wtf_size_t AppendRunClusters(const ShapedText::RunWithOffset& run_with_offset,
                             uint32_t start,
                             uint32_t end,
                             HeapVector<ShapedText::ClusterData>& clusters) {
  const unsigned offset = run_with_offset.character_offset_;
  // Every cluster lies within its run, so a run outside the range has none.
  if (offset + run_with_offset.num_characters_ <= start || end <= offset) {
    return 0;
  }

  const wtf_size_t initial_clusters_index = clusters.size();
  run_with_offset.shape_result_->ForEachGraphemeClusters(
      StringView(run_with_offset.text_), run_with_offset.x_position_, 0,
      run_with_offset.num_characters_, 0,
      [](void* context, unsigned character_index, float total_advance,
         unsigned graphemes_in_cluster, float cluster_advance,
         CanvasRotationInVertical rotation) {
        auto* clusters =
            static_cast<HeapVector<ShapedText::ClusterData>*>(context);
        // The run-local `start` is made text-relative, and `end` is set, below.
        clusters->push_back(ShapedText::ClusterData{.start = character_index,
                                                    .end = character_index,
                                                    .x_position = total_advance,
                                                    .width = cluster_advance});
      },
      &clusters);

  // Sort by start index, to produce logical order.
  // TODO(fmalita): this is only reordering within a run, and because runs
  // are processed in visual order the cumulative result is in visual order
  // across runs, but logical order within each run - so multi-run RTL text is
  // in neither order.
  std::ranges::sort(base::span(clusters).subspan(initial_clusters_index), {},
                    &ShapedText::ClusterData::start);

  // Second pass:
  //   1. compute the end index and make the start index text-relative
  //   2. filter out clusters outside the range.
  // TODO(fmalita): can we fold this into the initial pass?
  wtf_size_t write_index = initial_clusters_index;
  for (wtf_size_t i = initial_clusters_index; i < clusters.size(); i++) {
    // Each cluster ends where the next one starts. The next cluster's
    // `start` is still run-local at this point.
    const unsigned local_end = i + 1 < clusters.size()
                                   ? clusters[i + 1].start
                                   : run_with_offset.num_characters_;
    ShapedText::ClusterData cluster = clusters[i];
    cluster.start += offset;
    cluster.end = local_end + offset;
    if (cluster.end <= start || end <= cluster.start) {
      continue;
    }

    DCHECK_LE(write_index, i);
    clusters[write_index++] = cluster;
  }

  clusters.Shrink(write_index);

  return write_index - initial_clusters_index;
}

// Returns true if [start, end) is within `text`. Otherwise, throws an
// IndexSizeError and returns false. `exception_state` may be null only when the
// range is known to be valid. Indexes less than 0 are rejected by
// [EnforceRange] in the IDL bindings.
// TODO(crbug.com/580852205): This doesn't match selectionRects(), which
// accepts start == text.length() and collapses start > end. Define consistent
// range semantics for all the range queries.
bool CheckRange(const String& text,
                uint32_t start,
                uint32_t end,
                ExceptionState* exception_state) {
  if (start < text.length() && end <= text.length()) {
    return true;
  }
  CHECK(exception_state);
  const bool is_start = start >= text.length();
  exception_state->ThrowDOMException(
      DOMExceptionCode::kIndexSizeError,
      ExceptionMessages::IndexExceedsMaximumBound(
          is_start ? "start" : "end", is_start ? start : end, text.length()));
  return false;
}

// A visually contiguous part of the text with a single font and direction, used
// to build glyphRuns().
struct RunSegment {
  DISALLOW_NEW();

  void Trace(Visitor* visitor) const { visitor->Trace(font_data); }

  unsigned start = 0;
  unsigned end = 0;
  float x_position = 0.0f;
  float width = 0.0f;
  TextDirection direction = TextDirection::kLtr;
  Member<const SimpleFontData> font_data;
};

}  // namespace

template <>
struct VectorTraits<RunSegment> : VectorTraitsBase<RunSegment> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

void ShapedText::Trace(Visitor* visitor) const {
  visitor->Trace(font_);
  visitor->Trace(runs_with_offset_);
  ScriptWrappable::Trace(visitor);
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

const HeapVector<Member<DOMRectReadOnly>> ShapedText::selectionRects(
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

gfx::RectF ShapedText::ComputeGlyphBoundsForRange(uint32_t start,
                                                  uint32_t end) const {
  gfx::RectF bounding_box;
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
  return bounding_box;
}

DOMRectReadOnly* ShapedText::actualBoundingBox(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  if (!CheckRange(text_, start, end, &exception_state)) {
    return DOMRectReadOnly::FromRectF(gfx::RectF());
  }

  gfx::RectF bounding_box = ComputeGlyphBoundsForRange(start, end);
  bounding_box.Offset(-text_align_dx_, baseline_y_);
  return DOMRectReadOnly::FromRectF(bounding_box);
}

HeapVector<ShapedText::ClusterData> ShapedText::CollectClustersForRange(
    uint32_t start,
    uint32_t end,
    ExceptionState* exception_state) const {
  HeapVector<ClusterData> clusters;
  if (!CheckRange(text_, start, end, exception_state)) {
    return clusters;
  }

  const SimpleFontData* primary_font = font_ ? font_->PrimaryFont() : nullptr;

  for (const auto& run_with_offset : runs_with_offset_) {
    const wtf_size_t first = clusters.size();
    if (!AppendRunClusters(run_with_offset, start, end, clusters)) {
      continue;
    }
    if (!run_with_offset.shape_result_->HasFallbackFonts(primary_font)) {
      for (wtf_size_t i = first; i < clusters.size(); i++) {
        clusters[i].font_data = primary_font;
      }
      continue;
    }
    const RunFonts run_fonts(run_with_offset);
    for (wtf_size_t i = first; i < clusters.size(); i++) {
      ClusterData& cluster = clusters[i];
      cluster.font_data =
          run_fonts.ForRange(cluster.start, cluster.end, primary_font);
    }
  }
  return clusters;
}

HeapVector<Member<GlyphRun>> ShapedText::glyphRuns() {
  HeapVector<Member<GlyphRun>> result;
  if (!font_) {
    return result;
  }
  const SimpleFontData* primary_font = font_->PrimaryFont();
  HeapVector<RunSegment> segments;

  // Merges `seg` into the last segment when they are visually adjacent and
  // have the same font and direction.
  auto append_or_merge = [&segments](const RunSegment& seg) {
    if (!segments.empty()) {
      RunSegment& prev = segments.back();
      if (prev.font_data == seg.font_data && prev.direction == seg.direction) {
        if (IsLtr(seg.direction) && prev.end == seg.start) {
          prev.end = seg.end;
          prev.width += seg.width;
          return;
        }
        if (IsRtl(seg.direction) && seg.end == prev.start) {
          prev.start = seg.start;
          prev.width += seg.width;
          return;
        }
      }
    }
    segments.push_back(seg);
  };

  for (const auto& run_with_offset : runs_with_offset_) {
    // A run without fallback fonts is a single segment. Otherwise, split it
    // into clusters, each with its resolved font.
    if (!run_with_offset.shape_result_->HasFallbackFonts(primary_font)) {
      append_or_merge(
          RunSegment{.start = run_with_offset.character_offset_,
                     .end = run_with_offset.character_offset_ +
                            run_with_offset.num_characters_,
                     .x_position = run_with_offset.x_position_,
                     .width = run_with_offset.shape_result_->Width(),
                     .direction = run_with_offset.direction_,
                     .font_data = primary_font});
      continue;
    }

    HeapVector<ClusterData> clusters;
    AppendRunClusters(run_with_offset, 0, text_.length(), clusters);
    const RunFonts run_fonts(run_with_offset);
    HeapVector<RunSegment> run_segments;
    run_segments.reserve(clusters.size());
    for (const ClusterData& cluster : clusters) {
      run_segments.push_back(RunSegment{
          .start = cluster.start,
          .end = cluster.end,
          .x_position = cluster.x_position,
          .width = cluster.width,
          .direction = run_with_offset.direction_,
          .font_data =
              run_fonts.ForRange(cluster.start, cluster.end, primary_font)});
    }
    // Clusters are in logical order. Reverse RTL runs into visual order.
    if (IsRtl(run_with_offset.direction_)) {
      std::reverse(run_segments.begin(), run_segments.end());
    }
    for (const auto& seg : run_segments) {
      append_or_merge(seg);
    }
  }

  result.reserve(segments.size());
  for (const auto& seg : segments) {
    result.push_back(MakeGarbageCollected<GlyphRun>(
        seg.start, seg.end, seg.x_position - text_align_dx_, 0.0, seg.width,
        *this, seg.font_data.Get()));
  }
  return result;
}

HeapVector<Member<GlyphRun>> ShapedText::glyphClusters() {
  // The whole text: there is no valid range in an empty text.
  if (text_.empty()) {
    return {};
  }
  return GetGlyphClustersImpl(0, text_.length(), /*exception_state=*/nullptr);
}

HeapVector<Member<GlyphRun>> ShapedText::glyphClusters(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  return GetGlyphClustersImpl(start, end, &exception_state);
}

HeapVector<Member<GlyphRun>> ShapedText::GetGlyphClustersImpl(
    uint32_t start,
    uint32_t end,
    ExceptionState* exception_state) {
  HeapVector<ClusterData> clusters =
      CollectClustersForRange(start, end, exception_state);
  HeapVector<Member<GlyphRun>> result;
  result.reserve(clusters.size());
  for (const auto& cluster : clusters) {
    result.push_back(MakeGarbageCollected<GlyphRun>(
        cluster.start, cluster.end, cluster.x_position - text_align_dx_, 0.0,
        cluster.width, *this, cluster.font_data.Get()));
  }
  return result;
}

unsigned ShapedText::advanceToPosition(double x) {
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

double ShapedText::positionToAdvance(uint32_t position,
                                     ExceptionState& exception_state) {
  if (position > text_.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("position", position,
                                                    text_.length()));
    return 0.0;
  }

  // `position` is in the run that starts at or contains it (see the bidi
  // limitation in the header). The end of the text is at the end of the
  // (logically) last run.
  const bool at_text_end = position == text_.length();
  for (const auto& run_with_offset : runs_with_offset_) {
    const unsigned int run_start_index = run_with_offset.character_offset_;
    const unsigned int run_end_index =
        run_start_index + run_with_offset.num_characters_;
    if (position < run_start_index || position > run_end_index ||
        (position == run_end_index && !at_text_end)) {
      continue;
    }
    float x = run_with_offset.shape_result_->CaretPositionForOffset(
                  position - run_start_index, run_with_offset.text_,
                  position < run_end_index ? AdjustMidCluster::kToStart
                                           : AdjustMidCluster::kToEnd) +
              run_with_offset.x_position_;
    return x - text_align_dx_;
  }

  return -text_align_dx_;
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
