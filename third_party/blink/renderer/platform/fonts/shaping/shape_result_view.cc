// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

#include <iterator>
#include "base/containers/adapters.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_bounds_accumulator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"

namespace blink {

// Note: We allocate |RunInfoPart| in flexible array in |ShapeResultView|.
struct ShapeResultView::RunInfoPart {
 public:
  RunInfoPart(scoped_refptr<const ShapeResult::RunInfo> run,
              ShapeResult::RunInfo::GlyphDataRange range,
              unsigned start_index,
              unsigned offset,
              unsigned num_characters,
              float width)
      : run_(run),
        range_(range),
        start_index_(start_index),
        offset_(offset),
        num_characters_(num_characters),
        width_(width) {}

  using const_iterator = const HarfBuzzRunGlyphData*;
  const_iterator begin() const { return range_.begin; }
  const_iterator end() const { return range_.end; }
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
  const HarfBuzzRunGlyphData& GlyphAt(unsigned index) const {
    return *(range_.begin + index);
  }
  template <bool has_non_zero_glyph_offsets>
  ShapeResult::RunInfo::GlyphOffsetArray::iterator<has_non_zero_glyph_offsets>
  GetGlyphOffsets() const {
    return ShapeResult::RunInfo::GlyphOffsetArray::iterator<
        has_non_zero_glyph_offsets>(range_);
  }
  bool HasGlyphOffsets() const { return range_.offsets; }
  // The end character index of |this| without considering offsets in
  // |ShapeResultView|. This is analogous to:
  //   GlyphAt(Rtl() ? -1 : NumGlyphs()).character_index
  // if such |HarfBuzzRunGlyphData| is available.
  unsigned CharacterIndexOfEndGlyph() const {
    return num_characters_ + offset_;
  }

  bool Rtl() const { return run_->Rtl(); }
  bool IsHorizontal() const { return run_->IsHorizontal(); }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return range_.end - range_.begin; }
  float Width() const { return width_; }

  unsigned PreviousSafeToBreakOffset(unsigned offset) const;

  // Common signatures with RunInfo, to templatize algorithms.
  const ShapeResult::RunInfo* GetRunInfo() const { return run_.get(); }
  const ShapeResult::RunInfo::GlyphDataRange& GetGlyphDataRange() const {
    return range_;
  }
  ShapeResult::RunInfo::GlyphDataRange FindGlyphDataRange(
      unsigned start_character_index,
      unsigned end_character_index) const {
    return GetGlyphDataRange().FindGlyphDataRange(Rtl(), start_character_index,
                                                  end_character_index);
  }
  unsigned OffsetToRunStartIndex() const { return offset_; }

  // The helper function for implementing |CreateViewsForResult()| for
  // handling iterating over |Vector<scoped_refptr<RunInfo>>| and
  // |base::span<RunInfoPart>|.
  const RunInfoPart* get() const { return this; }

  scoped_refptr<const ShapeResult::RunInfo> run_;
  ShapeResult::RunInfo::GlyphDataRange range_;

  // Start index for partial run, adjusted to ensure that runs are continuous.
  unsigned start_index_;

  // Offset relative to start index for the original run.
  unsigned offset_;

  unsigned num_characters_;
  float width_;
};

unsigned ShapeResultView::RunInfoPart::PreviousSafeToBreakOffset(
    unsigned offset) const {
  if (offset >= NumCharacters())
    return NumCharacters();
  if (!Rtl()) {
    for (const auto& glyph : base::Reversed(*this)) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index;
    }
  } else {
    for (const auto& glyph : *this) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index;
    }
  }

  // Next safe break is at the start of the run.
  return 0;
}

// The offset to add to |HarfBuzzRunGlyphData.character_index| to compute the
// character index of the source string.
unsigned ShapeResultView::CharacterIndexOffsetForGlyphData(
    const RunInfoPart& part) const {
  return part.start_index_ + char_index_offset_ - part.offset_;
}

template <class ShapeResultType>
ShapeResultView::ShapeResultView(const ShapeResultType* other)
    : primary_font_(other->primary_font_),
      start_index_(0),
      num_characters_(0),
      num_glyphs_(0),
      direction_(other->direction_),
      has_vertical_offsets_(other->has_vertical_offsets_),
      width_(0) {}

ShapeResultView::~ShapeResultView() {
  for (auto& part : Parts())
    part.~RunInfoPart();
}

scoped_refptr<ShapeResult> ShapeResultView::CreateShapeResult() const {
  ShapeResult* new_result =
      new ShapeResult(primary_font_, start_index_ + char_index_offset_,
                      num_characters_, Direction());
  new_result->runs_.ReserveCapacity(num_parts_);
  for (const auto& part : RunsOrParts()) {
    auto new_run = ShapeResult::RunInfo::Create(
        part.run_->font_data_.get(), part.run_->direction_,
        part.run_->canvas_rotation_, part.run_->script_, part.start_index_,
        part.NumGlyphs(), part.num_characters_);
    new_run->glyph_data_.CopyFromRange(part.range_);
    for (HarfBuzzRunGlyphData& glyph_data : new_run->glyph_data_) {
      glyph_data.character_index -= part.offset_;
    }

    new_run->start_index_ += char_index_offset_;
    new_run->width_ = part.width_;
    new_run->num_characters_ = part.num_characters_;
    new_result->runs_.push_back(std::move(new_run));
  }

  new_result->num_glyphs_ = num_glyphs_;
  new_result->has_vertical_offsets_ = has_vertical_offsets_;
  new_result->width_ = width_;

  return base::AdoptRef(new_result);
}

template <class ShapeResultType>
void ShapeResultView::CreateViewsForResult(const ShapeResultType* other,
                                           unsigned start_index,
                                           unsigned end_index) {
  // Compute the diff of index and the number of characters from the source
  // ShapeResult and given offsets, because computing them from runs/parts can
  // be inaccurate when all characters in a run/part are missing.
  int index_diff = start_index_ + num_characters_ -
                   std::max(start_index, other->StartIndex());
  num_characters_ += std::min(end_index, other->EndIndex()) -
                     std::max(start_index, other->StartIndex());

  RunInfoPart* part = Parts().data() + num_parts_;
  for (const auto& run_or_part : other->RunsOrParts()) {
    auto* const run = run_or_part.get();
    if (!run->GetRunInfo())
      continue;
    // Compute start/end of the run, or of the part if ShapeResultView.
    unsigned part_start = run->start_index_ + other->StartIndexOffsetForRun();
    unsigned run_end = part_start + run->num_characters_;
    if (start_index < run_end && end_index > part_start) {
      ShapeResult::RunInfo::GlyphDataRange range;

      // Adjust start/end to the character index of |RunInfo|. The start index
      // of |RunInfo| could be different from |part_start| for ShapeResultView.
      DCHECK_GE(part_start, run->OffsetToRunStartIndex());
      unsigned run_start = part_start - run->OffsetToRunStartIndex();
      unsigned adjusted_start =
          start_index > run_start ? start_index - run_start : 0;
      unsigned adjusted_end = std::min(end_index, run_end) - run_start;
      DCHECK(adjusted_end > adjusted_start);
      unsigned part_characters = adjusted_end - adjusted_start;
      float part_width;

      // Avoid O(log n) find operation if the entire run is in range.
      if (part_start >= start_index && run_end <= end_index) {
        range = run->GetGlyphDataRange();
        part_width = run->width_;
      } else {
        range = run->FindGlyphDataRange(adjusted_start, adjusted_end);
        part_width = 0;
        for (auto* glyph = range.begin; glyph != range.end; glyph++)
          part_width += glyph->advance;
      }

      // Adjust start_index for runs to be continuous.
      unsigned part_start_index = run_start + adjusted_start + index_diff;
      unsigned part_offset = adjusted_start;
      new (part) RunInfoPart(run->GetRunInfo(), range, part_start_index,
                             part_offset, part_characters, part_width);
      ++part;

      num_glyphs_ += range.end - range.begin;
      width_ += part_width;
    }
  }
  num_parts_ = static_cast<wtf_size_t>(std::distance(Parts().data(), part));
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(const Segment* segments,
                                                       size_t segment_count) {
  DCHECK_GT(segment_count, 0u);
#if DCHECK_IS_ON()
  for (unsigned i = 0; i < segment_count; ++i) {
    DCHECK((segments[i].result || segments[i].view) &&
           (!segments[i].result || !segments[i].view));
  }
#endif
  wtf_size_t num_parts = 0;
  for (auto& segment : base::span<const Segment>(segments, segment_count)) {
    num_parts += segment.result ? segment.result->RunsOrParts().size()
                                : segment.view->RunsOrParts().size();
  }
  static_assert(sizeof(ShapeResultView) % alignof(RunInfoPart) == 0,
                "We have RunInfoPart as flexible array in ShapeResultView");
  const size_t byte_size =
      sizeof(ShapeResultView) + sizeof(RunInfoPart) * num_parts;
  void* buffer = ::WTF::Partitions::FastMalloc(
      byte_size, ::WTF::GetStringWithTypeName<ShapeResultView>());
  ShapeResultView* out = segments[0].result
                             ? new (buffer) ShapeResultView(segments[0].result)
                             : new (buffer) ShapeResultView(segments[0].view);
  out->AddSegments(segments, segment_count);
  return base::AdoptRef(out);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result,
    unsigned start_index,
    unsigned end_index) {
  Segment segment = {result, start_index, end_index};
  return Create(&segment, 1);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResultView* result,
    unsigned start_index,
    unsigned end_index) {
  Segment segment = {result, start_index, end_index};
  return Create(&segment, 1);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result) {
  // This specialization is an optimization to allow the bounding box to be
  // re-used.
  const wtf_size_t num_parts = result->RunsOrParts().size();
  static_assert(sizeof(ShapeResultView) % alignof(RunInfoPart) == 0,
                "We have RunInfoPart as flexible array in ShapeResultView");
  const size_t byte_size =
      sizeof(ShapeResultView) + sizeof(RunInfoPart) * num_parts;
  void* buffer = ::WTF::Partitions::FastMalloc(
      byte_size, ::WTF::GetStringWithTypeName<ShapeResultView>());
  ShapeResultView* out = new (buffer) ShapeResultView(result);
  out->char_index_offset_ = result->StartIndex();
  if (!out->Rtl()) {
    out->start_index_ = 0;
  } else {
    out->start_index_ = out->char_index_offset_;
    out->char_index_offset_ = 0;
  }
  out->CreateViewsForResult(result, 0, std::numeric_limits<unsigned>::max());
  out->has_vertical_offsets_ = result->has_vertical_offsets_;
  return base::AdoptRef(out);
}

void ShapeResultView::AddSegments(const Segment* segments,
                                  size_t segment_count) {
  // This method assumes that no parts have been added yet.
  DCHECK_EQ(num_parts_, 0u);

  // Segments are in logical order, runs and parts are in visual order. Iterate
  // over segments back-to-front for RTL.
  DCHECK_GT(segment_count, 0u);
  unsigned last_segment_index = segment_count - 1;

  // Compute start index offset for the overall run. This is added to the start
  // index of each glyph to ensure consistency with ShapeResult::SubRange
  char_index_offset_ = segments[0].result ? segments[0].result->StartIndex()
                                          : segments[0].view->StartIndex();
  char_index_offset_ = std::max(char_index_offset_, segments[0].start_index);
  if (!Rtl()) {  // Left-to-right
    start_index_ = 0;
  } else {  // Right to left
    start_index_ = char_index_offset_;
    char_index_offset_ = 0;
  }

  for (unsigned i = 0; i < segment_count; i++) {
    const Segment& segment = segments[Rtl() ? last_segment_index - i : i];
    if (segment.result) {
      DCHECK_EQ(segment.result->Direction(), Direction());
      CreateViewsForResult(segment.result, segment.start_index,
                           segment.end_index);
      has_vertical_offsets_ |= segment.result->has_vertical_offsets_;
    } else if (segment.view) {
      DCHECK_EQ(segment.view->Direction(), Direction());
      CreateViewsForResult(segment.view, segment.start_index,
                           segment.end_index);
      has_vertical_offsets_ |= segment.view->has_vertical_offsets_;
    } else {
      NOTREACHED();
    }
  }
}

unsigned ShapeResultView::PreviousSafeToBreakOffset(unsigned index) const {
  for (auto it = RunsOrParts().rbegin(); it != RunsOrParts().rend(); ++it) {
    const auto& part = *it;
    unsigned run_start = part.start_index_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset <= part.num_characters_) {
        return part.PreviousSafeToBreakOffset(offset) + run_start;
      }
      if (!Rtl()) {
        return run_start + part.num_characters_;
      }
    } else if (Rtl()) {
      if (it == RunsOrParts().rbegin())
        return part.start_index_;
      const auto& previous_run = *--it;
      return previous_run.start_index_ + previous_run.num_characters_;
    }
  }

  return StartIndex();
}

void ShapeResultView::GetRunFontData(
    Vector<ShapeResult::RunFontData>* font_data) const {
  for (const auto& part : RunsOrParts()) {
    font_data->push_back(ShapeResult::RunFontData(
        {part.run_->font_data_.get(), part.end() - part.begin()}));
  }
}

void ShapeResultView::FallbackFonts(
    HashSet<const SimpleFontData*>* fallback) const {
  DCHECK(fallback);
  DCHECK(primary_font_);
  for (const auto& part : RunsOrParts()) {
    if (part.run_->font_data_ && part.run_->font_data_ != primary_font_) {
      fallback->insert(part.run_->font_data_.get());
    }
  }
}

template <bool has_non_zero_glyph_offsets>
float ShapeResultView::ForEachGlyphImpl(float initial_advance,
                                        GlyphCallback glyph_callback,
                                        void* context,
                                        const RunInfoPart& part) const {
  auto glyph_offsets = part.GetGlyphOffsets<has_non_zero_glyph_offsets>();
  const auto& run = part.run_;
  auto total_advance = initial_advance;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
  const SimpleFontData* font_data = run->font_data_.get();
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
  auto total_advance = initial_advance;
  const auto& run = part.run_;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
  const SimpleFontData* font_data = run->font_data_.get();
  const unsigned character_index_offset_for_glyph_data =
      CharacterIndexOffsetForGlyphData(part);
  if (!run->Rtl()) {  // Left-to-right
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

  for (const auto& part : Parts()) {
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
        advance_so_far += glyph_data.advance;
        rtl ? --cluster_start : ++cluster_start;
        continue;
      }

      cluster_advance += glyph_data.advance;

      if (text.Is8Bit()) {
        callback(context, current_character_index, advance_so_far, 1,
                 glyph_data.advance, run->canvas_rotation_);

        advance_so_far += glyph_data.advance;
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
    FloatRect* ink_bounds) const {
  // Get glyph bounds from Skia. It's a lot faster if we give it list of glyph
  // IDs rather than calling it for each glyph.
  // TODO(kojii): MacOS does not benefit from batching the Skia request due to
  // https://bugs.chromium.org/p/skia/issues/detail?id=5328, and the cost to
  // prepare batching, which is normally much less than the benefit of
  // batching, is not ignorable unfortunately.
  auto glyph_offsets = part.GetGlyphOffsets<has_non_zero_glyph_offsets>();
  const SimpleFontData& current_font_data = *part.run_->font_data_;
  unsigned num_glyphs = part.NumGlyphs();
#if !defined(OS_MACOSX)
  Vector<Glyph, 256> glyphs(num_glyphs);
  unsigned i = 0;
  for (const auto& glyph_data : part)
    glyphs[i++] = glyph_data.glyph;
  Vector<SkRect, 256> bounds_list(num_glyphs);
  current_font_data.BoundsForGlyphs(glyphs, &bounds_list);
#endif

  GlyphBoundsAccumulator bounds(run_advance);
  for (unsigned j = 0; j < num_glyphs; ++j) {
    const HarfBuzzRunGlyphData& glyph_data = part.GlyphAt(j);
#if defined(OS_MACOSX)
    FloatRect glyph_bounds = current_font_data.BoundsForGlyph(glyph_data.glyph);
#else
    FloatRect glyph_bounds(bounds_list[j]);
#endif
    bounds.Unite<is_horizontal_run>(glyph_bounds, *glyph_offsets);
    bounds.origin += glyph_data.advance;
    ++glyph_offsets;
  }

  if (!is_horizontal_run)
    bounds.ConvertVerticalRunToLogical(current_font_data.GetFontMetrics());
  ink_bounds->Unite(bounds.bounds);
}

FloatRect ShapeResultView::ComputeInkBounds() const {
  FloatRect ink_bounds;

  float run_advance = 0.0f;
  for (const auto& part : Parts()) {
    if (part.HasGlyphOffsets()) {
      if (part.IsHorizontal()) {
        ComputePartInkBounds<true, true>(part, run_advance, &ink_bounds);
      } else {
        ComputePartInkBounds<false, true>(part, run_advance, &ink_bounds);
      }
    } else {
      if (part.IsHorizontal()) {
        ComputePartInkBounds<true, false>(part, run_advance, &ink_bounds);
      } else {
        ComputePartInkBounds<false, false>(part, run_advance, &ink_bounds);
      }
    }
    run_advance += part.Width();
  }

  return ink_bounds;
}

}  // namespace blink
