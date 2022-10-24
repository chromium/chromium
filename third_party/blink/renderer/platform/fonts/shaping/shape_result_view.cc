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
  //   GlyphAt(IsRtl() ? -1 : NumGlyphs()).character_index
  // if such |HarfBuzzRunGlyphData| is available.
  unsigned CharacterIndexOfEndGlyph() const {
    return num_characters_ + offset_;
  }

  bool IsLtr() const { return run_->IsLtr(); }
  bool IsRtl() const { return run_->IsRtl(); }
  bool IsHorizontal() const { return run_->IsHorizontal(); }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return range_.size(); }
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
    return GetGlyphDataRange().FindGlyphDataRange(
        IsRtl(), start_character_index, end_character_index);
  }
  unsigned OffsetToRunStartIndex() const { return offset_; }

  // The helper function for implementing |PopulateRunInfoParts()| for
  // handling iterating over |Vector<scoped_refptr<RunInfo>>| and
  // |base::span<RunInfoPart>|.
  const RunInfoPart* get() const { return this; }

  void ExpandRangeToIncludePartialGlyphs(unsigned offset,
                                         unsigned* from,
                                         unsigned* to) const {
    DCHECK_GE(offset + start_index_, offset_);
    unsigned part_offset = offset + start_index_ - offset_;
    run_->ExpandRangeToIncludePartialGlyphs(
        part_offset, reinterpret_cast<int*>(from), reinterpret_cast<int*>(to));
  }

  template <typename RunType, typename ShapeResultType>
  static unsigned ComputeStart(const RunType& run,
                               const ShapeResultType& result) {
    const unsigned part_start =
        run.start_index_ + result.StartIndexOffsetForRun();
    if (result.IsLtr())
      return part_start;
    // Under RTL and multiple parts, A RunInfoPart may have an
    // offset_ greater than start_index. In this case, run_start
    // would result in an invalid negative value.
    return std::max(part_start, run.OffsetToRunStartIndex());
  }

  template <typename RunType, typename ShapeResultType>
  static absl::optional<std::pair<unsigned, unsigned>> ComputeStartEnd(
      const RunType& run,
      const ShapeResultType& result,
      const Segment& segment) {
    if (!run.GetRunInfo())
      return absl::nullopt;
    const unsigned part_start = ComputeStart(run, result);
    if (segment.end_index <= part_start)
      return absl::nullopt;
    if (!run.num_characters_)
      return {{part_start, part_start}};
    const unsigned part_end = part_start + run.num_characters_;
    if (segment.start_index >= part_end)
      return absl::nullopt;
    return {{part_start, part_end}};
  }

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
  offset += offset_;
  if (IsLtr()) {
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

// The offset to add to |HarfBuzzRunGlyphData.character_index| to compute the
// character index of the source string.
unsigned ShapeResultView::CharacterIndexOffsetForGlyphData(
    const RunInfoPart& part) const {
  return part.start_index_ + char_index_offset_ - part.offset_;
}

// |InitData| provides values of const member variables of |ShapeResultView|
// for constructor.
struct ShapeResultView::InitData {
  scoped_refptr<const SimpleFontData> primary_font;
  unsigned start_index = 0;
  unsigned char_index_offset = 0;
  TextDirection direction = TextDirection::kLtr;
  bool has_vertical_offsets = false;
  wtf_size_t num_parts = 0;

  // Uses for fast path of constructing |ShapeResultView| from |ShapeResult|.
  void Populate(const ShapeResult& result) {
    PopulateFromShpaeResult(result);
    has_vertical_offsets = result.has_vertical_offsets_;
    num_parts = result.RunsOrParts().size();
  }

  // Uses for constructing |ShapeResultView| from |Segments|.
  void Populate(base::span<const Segment> segments) {
    const Segment& first_segment = segments.front();

    if (first_segment.result) {
      DCHECK(!first_segment.view);
      PopulateFromShpaeResult(*first_segment.result);
    } else if (first_segment.view) {
      DCHECK(!first_segment.result);
      PopulateFromShpaeResult(*first_segment.view);
    } else {
      NOTREACHED();
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
        NOTREACHED();
      }
    }
  }

 private:
  TextDirection Direction() const { return direction; }
  bool IsLtr() const { return blink::IsLtr(Direction()); }
  bool IsRtl() const { return blink::IsRtl(Direction()); }

  template <typename ShapeResultType>
  void PopulateFromShpaeResult(const ShapeResultType& result) {
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
          return !!RunInfoPart::ComputeStartEnd(*run_or_part.get(), result,
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
      char_index_offset_(data.char_index_offset),
      num_parts_(data.num_parts) {}

ShapeResultView::~ShapeResultView() {
  for (auto& part : Parts())
    part.~RunInfoPart();
}

scoped_refptr<ShapeResult> ShapeResultView::CreateShapeResult() const {
  ShapeResult* new_result =
      new ShapeResult(primary_font_, start_index_ + char_index_offset_,
                      num_characters_, Direction());
  new_result->runs_.reserve(num_parts_);
  for (const auto& part : RunsOrParts()) {
    auto new_run = ShapeResult::RunInfo::Create(
        part.run_->font_data_.get(), part.run_->direction_,
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
    new_result->runs_.push_back(std::move(new_run));
  }

  new_result->num_glyphs_ = num_glyphs_;
  new_result->has_vertical_offsets_ = has_vertical_offsets_;
  new_result->width_ = width_;

  return base::AdoptRef(new_result);
}

template <class ShapeResultType>
ShapeResultView::RunInfoPart* ShapeResultView::PopulateRunInfoParts(
    const ShapeResultType& other,
    const Segment& segment,
    RunInfoPart* part) {
  DCHECK_GE(part, Parts().data());

  // Compute the diff of index and the number of characters from the source
  // ShapeResult and given offsets, because computing them from runs/parts can
  // be inaccurate when all characters in a run/part are missing.
  const int index_diff = start_index_ + num_characters_ -
                         std::max(segment.start_index, other.StartIndex());

  // |num_characters_| is accumulated for computing |index_diff|.
  num_characters_ += std::min(segment.end_index, other.EndIndex()) -
                     std::max(segment.start_index, other.StartIndex());

  for (const auto& run_or_part : other.RunsOrParts()) {
    const auto* const run = run_or_part.get();
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
    ShapeResult::RunInfo::GlyphDataRange range;
    float part_width;
    if (part_start >= segment.start_index && part_end <= segment.end_index) {
      range = run->GetGlyphDataRange();
      part_width = run->width_;
    } else {
      range = run->FindGlyphDataRange(range_start, range_end);
      part_width = std::accumulate(
          range.begin, range.end, 0.0f,
          [](float sum, auto& glyph) { return sum + glyph.advance; });
    }

    // Adjust start_index for runs to be continuous.
    const unsigned part_start_index = run_start + range_start + index_diff;
    const unsigned part_offset = range_start;
    SECURITY_DCHECK(part + 1 <= Parts().data() + num_parts_);
    new (part) RunInfoPart(run->GetRunInfo(), range, part_start_index,
                           part_offset, part_characters, part_width);
    ++part;

    num_glyphs_ += range.end - range.begin;
    width_ += part_width;
  }
  return part;
}

ShapeResultView::RunInfoPart* ShapeResultView::PopulateRunInfoParts(
    const Segment& segment,
    RunInfoPart* part) {
  if (segment.result) {
    DCHECK(!segment.view);
    return PopulateRunInfoParts(*segment.result, segment, part);
  }
  if (segment.view) {
    DCHECK(!segment.result);
    return PopulateRunInfoParts(*segment.view, segment, part);
  }
  NOTREACHED();
  return nullptr;
}

base::span<ShapeResultView::RunInfoPart> ShapeResultView::Parts() {
  return {reinterpret_cast<ShapeResultView::RunInfoPart*>(parts_), num_parts_};
}

base::span<const ShapeResultView::RunInfoPart> ShapeResultView::Parts() const {
  return {reinterpret_cast<const ShapeResultView::RunInfoPart*>(parts_),
          num_parts_};
}

// static
constexpr size_t ShapeResultView::ByteSize(wtf_size_t num_parts) {
  static_assert(sizeof(ShapeResultView) % alignof(RunInfoPart) == 0,
                "We have RunInfoPart as flexible array in ShapeResultView");
  return sizeof(ShapeResultView) + sizeof(RunInfoPart) * num_parts;
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    base::span<const Segment> segments) {
  DCHECK(!segments.empty());
  InitData data;
  data.Populate(segments);

  void* const buffer = ::WTF::Partitions::FastMalloc(
      ByteSize(data.num_parts),
      ::WTF::GetStringWithTypeName<ShapeResultView>());
  ShapeResultView* const out = new (buffer) ShapeResultView(data);
  DCHECK_EQ(out->num_characters_, 0u);
  DCHECK_EQ(out->num_glyphs_, 0u);
  DCHECK_EQ(out->width_, 0);

  // Segments are in logical order, runs and parts are in visual order.
  // Iterate over segments back-to-front for RTL.
  RunInfoPart* part = out->Parts().data();
  if (out->IsLtr()) {
    for (auto& segment : segments)
      part = out->PopulateRunInfoParts(segment, part);
  } else {
    for (auto& segment : base::Reversed(segments))
      part = out->PopulateRunInfoParts(segment, part);
  }
  CHECK_EQ(part, out->Parts().data() + out->num_parts_);

  return base::AdoptRef(out);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result,
    unsigned start_index,
    unsigned end_index) {
  const Segment segments[] = {{result, start_index, end_index}};
  return Create(segments);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResultView* result,
    unsigned start_index,
    unsigned end_index) {
  const Segment segments[] = {{result, start_index, end_index}};
  return Create(segments);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result) {
  // This specialization is an optimization to allow the bounding box to be
  // re-used.
  InitData data;
  data.Populate(*result);

  void* const buffer = ::WTF::Partitions::FastMalloc(
      ByteSize(data.num_parts),
      ::WTF::GetStringWithTypeName<ShapeResultView>());
  ShapeResultView* const out = new (buffer) ShapeResultView(data);
  DCHECK_EQ(out->num_characters_, 0u);
  DCHECK_EQ(out->num_glyphs_, 0u);
  DCHECK_EQ(out->width_, 0);

  const Segment segment = {result, 0, std::numeric_limits<unsigned>::max()};
  RunInfoPart* const part =
      out->PopulateRunInfoParts(segment, out->Parts().data());
  CHECK_EQ(part, out->Parts().data() + out->num_parts_);
  return base::AdoptRef(out);
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
    Vector<ShapeResult::RunFontData>* font_data) const {
  for (const auto& part : RunsOrParts()) {
    font_data->push_back(ShapeResult::RunFontData(
        {part.run_->font_data_.get(),
         static_cast<wtf_size_t>(part.end() - part.begin())}));
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
#if !BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_MAC)
    gfx::RectF glyph_bounds =
        current_font_data.BoundsForGlyph(glyph_data.glyph);
#else
    gfx::RectF glyph_bounds = gfx::SkRectToRectF(bounds_list[j]);
#endif
    bounds.Unite<is_horizontal_run>(glyph_bounds, *glyph_offsets);
    bounds.origin += glyph_data.advance;
    ++glyph_offsets;
  }

  if (!is_horizontal_run)
    bounds.ConvertVerticalRunToLogical(current_font_data.GetFontMetrics());
  ink_bounds->Union(bounds.bounds);
}

gfx::RectF ShapeResultView::ComputeInkBounds() const {
  gfx::RectF ink_bounds;

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

void ShapeResultView::ExpandRangeToIncludePartialGlyphs(unsigned* from,
                                                        unsigned* to) const {
  for (const auto& part : Parts())
    part.ExpandRangeToIncludePartialGlyphs(char_index_offset_, from, to);
}

}  // namespace blink
