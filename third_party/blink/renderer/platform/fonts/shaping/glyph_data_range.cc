// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"

#include <algorithm>

#include "base/types/to_address.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

GlyphDataRange::GlyphDataRange(const ShapeResultRun& run)
    : run_(&run), size_(run.glyph_data_.size()) {}

base::span<const HarfBuzzRunGlyphData> GlyphDataRange::NonCompactGlyphs()
    const {
  if (!run_) {
    return {};
  }
  return base::span<const HarfBuzzRunGlyphData>(
             run_->glyph_data_.NonCompactGlyphs())
      .subspan(index_, size_);
}

bool GlyphDataRange::HasOffsets() const {
  return run_ && run_->glyph_data_.HasNonZeroOffsets();
}

bool GlyphDataRange::IsCompactSource() const {
  return run_ && run_->glyph_data_.IsCompact();
}

TextRunLayoutUnit GlyphDataRange::CompactSourceAdvance() const {
  return run_->glyph_data_.CompactAdvance();
}

unsigned GlyphDataRange::CompactStartCharacterIndex() const {
  CHECK(IsCompactSource());
  CHECK_LE(index_, run_->glyph_data_.size());
  CHECK_LE(size_, run_->glyph_data_.size() - index_);
  return run_->glyph_data_.IsCompactRtl()
             ? run_->glyph_data_.size() - index_ - size_
             : index_;
}

base::span<const uint16_t> GlyphDataRange::CompactGlyphs() const {
  return run_->glyph_data_.CompactGlyphs(index_, size_);
}

base::span<const GlyphOffset> GlyphDataRange::Offsets() const {
  if (HasOffsets()) [[unlikely]] {
    return run_->glyph_data_.Offsets().subspan(index_, size_);
  }
  return base::span<const GlyphOffset>{};
}

void GlyphDataRange::ExpandInto(base::span<HarfBuzzRunGlyphData> dest) const {
  CHECK_EQ(dest.size(), size_);
  if (!size_) {
    return;
  }
  run_->glyph_data_.ExpandInto(index_, dest);
}

GlyphDataRange GlyphDataRange::FindCompactGlyphDataRange(
    unsigned start_character_index,
    unsigned end_character_index) const {
  const wtf_size_t logical_begin = CompactStartCharacterIndex();
  const wtf_size_t logical_end = logical_begin + size_;
  const wtf_size_t start =
      std::clamp<wtf_size_t>(start_character_index, logical_begin, logical_end);
  const wtf_size_t end =
      std::clamp<wtf_size_t>(end_character_index, start, logical_end);
  const wtf_size_t physical_start =
      run_->glyph_data_.IsCompactRtl() ? run_->glyph_data_.size() - end : start;
  return GlyphDataRange(run_.Get(), physical_start, end - start);
}

GlyphDataRange GlyphDataRange::FindGlyphDataRange(
    bool is_rtl,
    wtf_size_t start_character_index,
    wtf_size_t end_character_index) const {
  if (!run_) [[unlikely]] {
    return GlyphDataRange();
  }
  if (!size_) [[unlikely]] {
    return GlyphDataRange(run_.Get(), index_, 0);
  }

  if (run_->glyph_data_.IsCompact()) [[unlikely]] {
    CHECK_EQ(is_rtl, run_->glyph_data_.IsCompactRtl());
    return FindCompactGlyphDataRange(start_character_index,
                                     end_character_index);
  }

  const base::span<const HarfBuzzRunGlyphData> glyphs = NonCompactGlyphs();
  // Use pointers to avoid per-step iterator checks in the standard algorithms.
  // The span validates the range, and the predicates cannot invalidate it.
  const auto* range_begin = glyphs.data();
  const auto* range_end = base::to_address(glyphs.end());
  if (!is_rtl) {
    const auto comparer = [](const HarfBuzzRunGlyphData& glyph,
                             wtf_size_t character_index) {
      return glyph.character_index < character_index;
    };
    const auto* start_glyph = std::lower_bound(range_begin, range_end,
                                               start_character_index, comparer);
    const auto* end_glyph =
        std::lower_bound(start_glyph, range_end, end_character_index, comparer);
    return GlyphDataRange(run_.Get(),
                          index_ + CheckedDistance(range_begin, start_glyph),
                          CheckedDistance(start_glyph, end_glyph));
  }

  // RTL clusters descend in physical order. Search their boundaries directly
  // without reverse-iterator bookkeeping.
  const auto* end_glyph = std::partition_point(
      range_begin, range_end,
      [start_character_index](const HarfBuzzRunGlyphData& glyph) {
        return glyph.character_index >= start_character_index;
      });
  const auto* start_glyph = std::partition_point(
      range_begin, end_glyph,
      [end_character_index](const HarfBuzzRunGlyphData& glyph) {
        return glyph.character_index >= end_character_index;
      });
  return GlyphDataRange(run_.Get(),
                        index_ + CheckedDistance(range_begin, start_glyph),
                        CheckedDistance(start_glyph, end_glyph));
}

void GlyphDataRange::Trace(Visitor* visitor) const {
  visitor->Trace(run_);
}

}  // namespace blink
