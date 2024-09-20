// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

#include <iterator>
#include <numeric>

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_bounds_accumulator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

ShapeResultView::RunInfoPart::RunInfoPart(const ShapeResult::RunInfo* run,
                                          GlyphDataRange range,
                                          unsigned start_index,
                                          unsigned offset,
                                          unsigned num_characters,
                                          float width)
    : run_(run),
      range_(range),
      start_index_(start_index),
      offset_(offset),
      num_characters_(num_characters),
      width_(width) {
  static_assert(std::is_trivially_destructible<RunInfoPart>::value, "");
}

void ShapeResultView::RunInfoPart::Trace(Visitor* visitor) const {
  visitor->Trace(run_);
}

unsigned ShapeResultView::RunInfoPart::PreviousSafeToBreakOffset(
    unsigned offset) const {
  if (offset >= NumCharacters())
    return NumCharacters();
  offset += offset_;
  if (run_->IsLtr()) {
    for (const auto& glyph : base::Reversed(*this)) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index - offset_;
    }
  } else {
    for (const auto& glyph : *this) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index - offset_;
    }
  }

  // Next safe break is at the start of the run.
  return 0;
}

GlyphDataRange ShapeResultView::RunInfoPart::FindGlyphDataRange(
    unsigned start_character_index,
    unsigned end_character_index) const {
  return GetGlyphDataRange().FindGlyphDataRange(
      run_->IsRtl(), start_character_index, end_character_index);
}

// The offset to add to |HarfBuzzRunGlyphData.character_index| to compute the
// character index of the source string.
unsigned ShapeResultView::CharacterIndexOffsetForGlyphData(
    const RunInfoPart& part) const {
  return part.start_index_ + char_index_offset_ - part.offset_;
}

// |InitData| provides values of const member variables of |ShapeResultView|
// for constructor.
struct ShapeResultView::InitData {
  STACK_ALLOCATED();

 public:
  const SimpleFontData* primary_font = nullptr;
  unsigned start_index = 0;
  unsigned char_index_offset = 0;
  TextDirection direction = TextDirection::kLtr;
  bool has_vertical_offsets = false;
  wtf_size_t num_parts = 0;

  // Uses for fast path of constructing |ShapeResultView| from |ShapeResult|.
  void Populate(const ShapeResult& result) {
    PopulateFromShapeResult(result);
    has_vertical_offsets = result.has_vertical_offsets_;
    num_parts = result.RunsOrParts().size();
  }

  // Uses for constructing |ShapeResultView| from |Segments|.
  void Populate(base::span<const Segment> segments) {
    const Segment& first_segment = segments.front();

    if (first_segment.result) {
      DCHECK(!first_segment.view);
      PopulateFromShapeResult(*first_segment.result);
    } else if (first_segment.view) {
      DCHECK(!first_segment.result);
      PopulateFromShapeResult(*first_segment.view);
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    // Compute start index offset for the overall run. This is added to the
    // start index of each glyph to ensure consistency with
    // |ShapeResult::SubRange|.
    if (IsLtr()) {
      DCHECK_EQ(start_index, 0u);
      char_index_offset =
          std::max(char_index_offset, first_segment.start_index);
    } else {
      DCHECK(IsRtl());
      start_index = std::max(start_index, first_segment.start_index);
      DCHECK_EQ(char_index_offset, 0u);
    }

    // Accumulates |num_parts| and |has_vertical_offsets|.
    DCHECK_EQ(num_parts, 0u);
    // Iterate |segment| in logical order, because of |ProcessShapeResult()|
    // doesn't case logical/visual order. See |ShapeResult::Create()|.
    for (auto& segment : segments) {
      if (segment.result) {
        DCHECK(!segment.view);
        ProcessShapeResult(*segment.result, segment);
      } else if (segment.view) {
        DCHECK(!segment.result);
        ProcessShapeResult(*segment.view, segment);
      } else {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

 private:
  TextDirection Direction() const { return direction; }
  bool IsLtr() const { return blink::IsLtr(Direction()); }
  bool IsRtl() const { return blink::IsRtl(Direction()); }

  template <typename ShapeResultType>
  void PopulateFromShapeResult(const ShapeResultType& result) {
    primary_font = result.primary_font_;
    direction = result.Direction();
    if (IsLtr()) {
      DCHECK_EQ(start_index, 0u);
      char_index_offset = result.StartIndex();
    } else {
      DCHECK(IsRtl());
      start_index = result.StartIndex();
      DCHECK_EQ(char_index_offset, 0u);
    }
  }

  template <typename ShapeResultType>
  void ProcessShapeResult(const ShapeResultType& result,
                          const Segment& segment) {
    DCHECK_EQ(result.Direction(), Direction());
    has_vertical_offsets |= result.has_vertical_offsets_;
    num_parts += CountRunInfoParts(result, segment);
  }

  template <typename ShapeResultType>
  static unsigned CountRunInfoParts(const ShapeResultType& result,
                                    const Segment& segment) {
    return static_cast<unsigned>(base::ranges::count_if(
        result.RunsOrParts(), [&result, &segment](const auto& run_or_part) {
          return !!RunInfoPart::ComputeStartEnd(*run_or_part.Get(), result,
                                                segment);
        }));
  }
};

ShapeResultView::ShapeResultView(const InitData& data)
    : primary_font_(data.primary_font),
      start_index_(data.start_index),
      num_glyphs_(0),
      direction_(static_cast<unsigned>(data.direction)),
      has_vertical_offsets_(data.has_vertical_offsets),
      char_index_offset_(data.char_index_offset) {}

ShapeResult* ShapeResultView::CreateShapeResult() const {
  ShapeResult* new_result = MakeGarbageCollected<ShapeResult>(
      primary_font_, start_index_ + char_index_offset_, num_characters_,
      Direction());
  new_result->runs_.ReserveInitialCapacity(parts_.size());
  for (const auto& part : RunsOrParts()) {
    auto* new_run = MakeGarbageCollected<ShapeResult::RunInfo>(
        part.run_->font_data_.Get(), part.run_->direction_,
        part.run_->canvas_rotation_, part.run_->script_, part.start_index_,
        part.NumGlyphs(), part.num_characters_);
    new_run->glyph_data_.CopyFromRange(part.range_);
    for (HarfBuzzRunGlyphData& glyph_data : new_run->glyph_data_) {
      DCHECK_GE(glyph_data.character_index, part.offset_);
      glyph_data.character_index -= part.offset_;
      DCHECK_LT(glyph_data.character_index, part.num_characters_);
    }

    new_run->start_index_ += char_index_offset_;
    new_run->width_ = part.width_;
    new_run->num_characters_ = part.num_characters_;
    new_run->CheckConsistency();
    new_result->runs_.push_back(new_run);
  }

  new_result->num_glyphs_ = num_glyphs_;
  new_result->has_vertical_offsets_ = has_vertical_offsets_;
  new_result->width_ = width_;

  return new_result;
}

template <class ShapeResultType>
void ShapeResultView::PopulateRunInfoParts(const ShapeResultType& other,
                                           const Segment& segment) {
  // Compute the diff of index and the number of characters from the source
  // ShapeResult and given offsets, because computing them from runs/parts can
  // be inaccurate when all characters in a run/part are missing.
  const int index_diff = start_index_ + num_characters_ -
                         std::max(segment.start_index, other.StartIndex());

  // |num_characters_| is accumulated for computing |index_diff|.
  num_characters_ += std::min(segment.end_index, other.EndIndex()) -
                     std::max(segment.start_index, other.StartIndex());

  for (const auto& run_or_part : other.RunsOrParts()) {
    const auto* const run = run_or_part.Get();
    const auto part_start_end =
        RunInfoPart::ComputeStartEnd(*run, other, segment);
    if (!part_start_end)
      continue;

    // Adjust start/end to the character index of |RunInfo|. The start index
    // of |RunInfo| could be different from |part_start| for
    // ShapeResultView.
    const unsigned part_start = part_start_end.value().first;
    const unsigned part_end = part_start_end.value().second;
    DCHECK_GE(part_start, run->OffsetToRunStartIndex());
    const unsigned run_start = part_start - run->OffsetToRunStartIndex();
    const unsigned range_start =
        segment.start_index > run_start
            ? std::max(segment.start_index, part_start) - run_start
            : 0;
    const unsigned range_end =
        std::min(segment.end_index, part_end) - run_start;
    DCHECK_GT(range_end, range_start);
    const unsigned part_characters = range_end - range_start;

    // Avoid O(log n) find operation if the entire run is in range.
    GlyphDataRange range;
    float part_width;
    if (part_start >= segment.start_index && part_end <= segment.end_index) {
      range = run->GetGlyphDataRange();
      part_width = run->width_;
    } else {
      range = run->FindGlyphDataRange(range_start, range_end);
      part_width = std::accumulate(
          range.begin, range.end, InlineLayoutUnit(),
          [](InlineLayoutUnit sum, const auto& glyph) {
            return sum + glyph.advance.template To<InlineLayoutUnit>();
          });
    }

    // Adjust start_index for runs to be continuous.
    const unsigned part_start_index = run_start + range_start + index_diff;
    const unsigned part_offset = range_start;
    parts_.emplace_back(run->GetRunInfo(), range, part_start_index, part_offset,
                        part_characters, part_width);

    num_glyphs_ += range.end - range.begin;
    width_ += part_width;
  }
}

void ShapeResultView::PopulateRunInfoParts(const Segment& segment) {
  if (segment.result) {
    DCHECK(!segment.view);
    PopulateRunInfoParts(*segment.result, segment);
  } else if (segment.view) {
    DCHECK(!segment.result);
    PopulateRunInfoParts(*segment.view, segment);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

ShapeResultView* ShapeResultView::Create(base::span<const Segment> segments) {
  DCHECK(!segments.empty());
  InitData data;
  data.Populate(segments);

  ShapeResultView* out = MakeGarbageCollected<ShapeResultView>(data);
  DCHECK_EQ(out->num_characters_, 0u);
  DCHECK_EQ(out->num_glyphs_, 0u);
  DCHECK_EQ(out->width_, 0);
  out->parts_.ReserveInitialCapacity(data.num_parts);

  // Segments are in logical order, runs and parts are in visual order.
  // Iterate over segments back-to-front for RTL.
  if (out->IsLtr()) {
    for (auto& segment : segments)
      out->PopulateRunInfoParts(segment);
  } else {
    for (auto& segment : base::Reversed(segments))
      out->PopulateRunInfoParts(segment);
  }
  DCHECK_EQ(data.num_parts, out->parts_.size());
  return out;
}

ShapeResultView* ShapeResultView::Create(const ShapeResult* result,
                                         unsigned start_index,
                                         unsigned end_index) {
  const Segment segments[] = {{result, start_index, end_index}};
  return Create(segments);
}

ShapeResultView* ShapeResultView::Create(const ShapeResultView* result,
                                         unsigned start_index,
                                         unsigned end_index) {
  const Segment segments[] = {{result, start_index, end_index}};
  return Create(segments);
}

ShapeResultView* ShapeResultView::Create(const ShapeResult* result) {
  // This specialization is an optimization to allow the bounding box to be
  // re-used.
  InitData data;
  data.Populate(*result);

  ShapeResultView* out = MakeGarbageCollected<ShapeResultView>(data);
  DCHECK_EQ(out->num_characters_, 0u);
  DCHECK_EQ(out->num_glyphs_, 0u);
  DCHECK_EQ(out->width_, 0);
  out->parts_.ReserveInitialCapacity(data.num_parts);

  const Segment segment = {result, 0, std::numeric_limits<unsigned>::max()};
  out->PopulateRunInfoParts(segment);
  DCHECK_EQ(data.num_parts, out->parts_.size());
  return out;
}

unsigned ShapeResultView::PreviousSafeToBreakOffset(unsigned index) const {
  for (auto it = RunsOrParts().rbegin(); it != RunsOrParts().rend(); ++it) {
    const auto& part = *it;
    unsigned run_start = part.start_index_ + char_index_offset_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset <= part.num_characters_) {
        return part.PreviousSafeToBreakOffset(offset) + run_start;
      }
      if (IsLtr()) {
        return run_start + part.num_characters_;
      }
    } else if (IsRtl()) {
      if (it == RunsOrParts().rbegin())
        return part.start_index_;
      const auto& previous_run = *--it;
      return previous_run.start_index_ + previous_run.num_characters_;
    }
  }

  return StartIndex();
}

void ShapeResultView::GetRunFontData(
    HeapVector<ShapeResult::RunFontData>* font_data) const {
  for (const auto& part : RunsOrParts()) {
    font_data->push_back(ShapeResult::RunFontData(
        {part.run_->font_data_.Get(),
         static_cast<wtf_size_t>(part.end() - part.begin())}));
  }
}

HeapHashSet<Member<const SimpleFontData>> ShapeResultView::UsedFonts() const {
  HeapHashSet<Member<const SimpleFontData>> used_fonts;
  for (const auto& part : RunsOrParts()) {
    if (part.run_->font_data_) {
      used_fonts.insert(part.run_->font_data_.Get());
    }
  }
  return used_fonts;
}

template <bool has_non_zero_glyph_offsets>
float ShapeResultView::ForEachGlyphImpl(float initial_advance,
                                        GlyphCallback glyph_callback,
                                        void* context,
                                        const RunInfoPart& part) const {
  auto glyph_offsets = part.GetGlyphOffsets<has_non_zero_glyph_offsets>();
  const auto& run = part.run_;
  auto total_advance = InlineLayoutUnit::FromFloatRound(initial_advance);
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
  const SimpleFontData* font_data = run->font_data_.Get();
  const unsigned character_index_offset_for_glyph_data =
      CharacterIndexOffsetForGlyphData(part);
  for (const auto& glyph_data : part) {
    unsigned character_index =
        glyph_data.character_index + character_index_offset_for_glyph_data;
    glyph_callback(context, character_index, glyph_data.glyph, *glyph_offsets,
                   total_advance, is_horizontal, run->canvas_rotation_,
                   font_data);
    total_advance += glyph_data.advance;
    ++glyph_offsets;
  }
  return total_advance;
}

float ShapeResultView::ForEachGlyph(float initial_advance,
                                    GlyphCallback glyph_callback,
                                    void* context) const {
  auto total_advance = initial_advance;
  for (const auto& part : RunsOrParts()) {
    if (part.HasGlyphOffsets()) {
      total_advance =
          ForEachGlyphImpl<true>(total_advance, glyph_callback, context, part);
    } else {
      total_advance =
          ForEachGlyphImpl<false>(total_advance, glyph_callback, context, part);
    }
  }
  return total_advance;
}

template <bool has_non_zero_glyph_offsets>
float ShapeResultView::ForEachGlyphImpl(float initial_advance,
                                        unsigned from,
                                        unsigned to,
                                        unsigned index_offset,
                                        GlyphCallback glyph_callback,
                                        void* context,
                                        const RunInfoPart& part) const {
  auto glyph_offsets = part.GetGlyphOffsets<has_non_zero_glyph_offsets>();
  auto total_advance = InlineLayoutUnit::FromFloatRound(initial_advance);
  const auto& run = part.run_;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
  const SimpleFontData* font_data = run->font_data_.Get();
  const unsigned character_index_offset_for_glyph_data =
      CharacterIndexOffsetForGlyphData(part);
  if (run->IsLtr()) {  // Left-to-right
    for (const auto& glyph_data : part) {
      unsigned character_index =
          glyph_data.character_index + character_index_offset_for_glyph_data;
      if (character_index >= to)
        break;
      if (character_index >= from) {
        glyph_callback(context, character_index, glyph_data.glyph,
                       *glyph_offsets, total_advance, is_horizontal,
                       run->canvas_rotation_, font_data);
      }
      total_advance += glyph_data.advance;
      ++glyph_offsets;
    }

  } else {  // Right-to-left
    for (const auto& glyph_data : part) {
      unsigned character_index =
          glyph_data.character_index + character_index_offset_for_glyph_data;
      if (character_index < from)
        break;
      if (character_index < to) {
        glyph_callback(context, character_index, glyph_data.glyph,
                       *glyph_offsets, total_advance, is_horizontal,
                       run->canvas_rotation_, font_data);
      }
      total_advance += glyph_data.advance;
      ++glyph_offsets;
    }
  }
  return total_advance;
}

float ShapeResultView::ForEachGlyph(float initial_advance,
                                    unsigned from,
                                    unsigned to,
                                    unsigned index_offset,
                                    GlyphCallback glyph_callback,
                                    void* context) const {
  auto total_advance = initial_advance;

  for (const auto& part : parts_) {
    if (part.HasGlyphOffsets()) {
      total_advance = ForEachGlyphImpl<true>(
          total_advance, from, to, index_offset, glyph_callback, context, part);
    } else {
      total_advance = ForEachGlyphImpl<false>(
          total_advance, from, to, index_offset, glyph_callback, context, part);
    }
  }
  return total_advance;
}

float ShapeResultView::ForEachGraphemeClusters(const StringView& text,
                                               float initial_advance,
                                               unsigned from,
                                               unsigned to,
                                               unsigned index_offset,
                                               GraphemeClusterCallback callback,
                                               void* context) const {
  unsigned run_offset = index_offset;
  float advance_so_far = initial_advance;

  for (const auto& part : RunsOrParts()) {
    if (!part.NumGlyphs())
      continue;

    const auto& run = part.run_;
    unsigned graphemes_in_cluster = 1;
    float cluster_advance = 0;
    bool rtl = Direction() == TextDirection::kRtl;

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be
    // broken down further from a text shaping point of view.  A cluster can
    // contain multiple glyphs and grapheme clusters, with mutually overlapping
    // boundaries.
    const unsigned character_index_offset_for_glyph_data =
        CharacterIndexOffsetForGlyphData(part) + run_offset;
    uint16_t cluster_start =
        static_cast<uint16_t>(rtl ? part.CharacterIndexOfEndGlyph() +
                                        character_index_offset_for_glyph_data
                                  : part.GlyphAt(0).character_index +
                                        character_index_offset_for_glyph_data);

    const unsigned num_glyphs = part.NumGlyphs();
    for (unsigned i = 0; i < num_glyphs; ++i) {
      const HarfBuzzRunGlyphData& glyph_data = part.GlyphAt(i);
      uint16_t current_character_index =
          glyph_data.character_index + character_index_offset_for_glyph_data;

      bool is_run_end = (i + 1 == num_glyphs);
      bool is_cluster_end =
          is_run_end || (part.GlyphAt(i + 1).character_index +
                             character_index_offset_for_glyph_data !=
                         current_character_index);

      if ((rtl && current_character_index >= to) ||
          (!rtl && current_character_index < from)) {
        advance_so_far += glyph_data.advance.ToFloat();
        rtl ? --cluster_start : ++cluster_start;
        continue;
      }

      cluster_advance += glyph_data.advance.ToFloat();

      if (text.Is8Bit()) {
        callback(context, current_character_index, advance_so_far, 1,
                 glyph_data.advance, run->canvas_rotation_);

        advance_so_far += glyph_data.advance.ToFloat();
      } else if (is_cluster_end) {
        uint16_t cluster_end;
        if (rtl) {
          cluster_end = current_character_index;
        } else {
          cluster_end = static_cast<uint16_t>(
              is_run_end ? part.CharacterIndexOfEndGlyph() +
                               character_index_offset_for_glyph_data
                         : part.GlyphAt(i + 1).character_index +
                               character_index_offset_for_glyph_data);
        }
        graphemes_in_cluster = ShapeResult::CountGraphemesInCluster(
            text.Span16(), cluster_start, cluster_end);
        if (!graphemes_in_cluster || !cluster_advance)
          continue;

        callback(context, current_character_index, advance_so_far,
                 graphemes_in_cluster, cluster_advance, run->canvas_rotation_);
        advance_so_far += cluster_advance;

        cluster_start = cluster_end;
        cluster_advance = 0;
      }
    }
  }
  return advance_so_far;
}

template <bool is_horizontal_run, bool has_non_zero_glyph_offsets>
void ShapeResultView::ComputePartInkBounds(
    const ShapeResultView::RunInfoPart& part,
    float run_advance,
    gfx::RectF* ink_bounds) const {
  // Get glyph bounds from Skia. It's a lot faster if we give it list of glyph
  // IDs rather than calling it for each glyph.
  // TODO(kojii): MacOS does not benefit from batching the Skia request due to
  // https://bugs.chromium.org/p/skia/issues/detail?id=5328, and the cost to
  // prepare batching, which is normally much less than the benefit of
  // batching, is not ignorable unfortunately.
  auto glyph_offsets = part.GetGlyphOffsets<has_non_zero_glyph_offsets>();
  const SimpleFontData& current_font_data = *part.run_->font_data_;
  unsigned num_glyphs = part.NumGlyphs();
#if !BUILDFLAG(IS_APPLE)
  Vector<Glyph, 256> glyphs(num_glyphs);
  unsigned i = 0;
  for (const auto& glyph_data : part)
    glyphs[i++] = glyph_data.glyph;
  Vector<SkRect, 256> bounds_list(num_glyphs);
  current_font_data.BoundsForGlyphs(glyphs, &bounds_list);
#endif

  GlyphBoundsAccumulator<is_horizontal_run> bounds;
  InlineLayoutUnit origin = InlineLayoutUnit::FromFloatCeil(run_advance);
  for (unsigned j = 0; j < num_glyphs; ++j) {
    const HarfBuzzRunGlyphData& glyph_data = part.GlyphAt(j);
#if BUILDFLAG(IS_APPLE)
    gfx::RectF glyph_bounds =
        current_font_data.BoundsForGlyph(glyph_data.glyph);
#else
    gfx::RectF glyph_bounds = gfx::SkRectToRectF(bounds_list[j]);
#endif
    bounds.Unite(glyph_bounds, origin, *glyph_offsets);
    origin += glyph_data.advance;
    ++glyph_offsets;
  }

  if (!is_horizontal_run)
    bounds.ConvertVerticalRunToLogical(current_font_data.GetFontMetrics());
  ink_bounds->Union(bounds.bounds);
}

gfx::RectF ShapeResultView::ComputeInkBounds() const {
  gfx::RectF ink_bounds;

  float run_advance = 0.0f;
  for (const auto& part : parts_) {
    if (part.HasGlyphOffsets()) {
      if (part.run_->IsHorizontal()) {
        ComputePartInkBounds<true, true>(part, run_advance, &ink_bounds);
      } else {
        ComputePartInkBounds<false, true>(part, run_advance, &ink_bounds);
      }
    } else {
      if (part.run_->IsHorizontal()) {
        ComputePartInkBounds<true, false>(part, run_advance, &ink_bounds);
      } else {
        ComputePartInkBounds<false, false>(part, run_advance, &ink_bounds);
      }
    }
    run_advance += part.Width();
  }

  return ink_bounds;
}

void ShapeResultView::ExpandRangeToIncludePartialGlyphs(unsigned* from,
                                                        unsigned* to) const {
  for (const auto& part : parts_) {
    unsigned part_offset =
        char_index_offset_ + part.start_index_ - part.offset_;
    part.run_->ExpandRangeToIncludePartialGlyphs(
        part_offset, reinterpret_cast<int*>(from), reinterpret_cast<int*>(to));
  }
}

}  // namespace blink
