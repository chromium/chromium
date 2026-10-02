// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"

#include "base/types/to_address.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

GlyphDataRange::GlyphDataRange(const ShapeResultRun& run)
    : run_(&run), size_(run.glyph_data_.size()) {}

GlyphDataRange::NonCompactGlyphPointerRange
GlyphDataRange::NonCompactGlyphPointers() const {
  if (!run_) {
    return {};
  }
  const base::span<const HarfBuzzRunGlyphData> glyphs =
      base::span<const HarfBuzzRunGlyphData>(
          run_->glyph_data_.NonCompactGlyphs())
          .subspan(index_, size_);
  return {glyphs.data(), base::to_address(glyphs.end())};
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
  // Identity character indices map the character range directly to glyphs.
  const unsigned base = index_;
  const wtf_size_t start =
      start_character_index > base
          ? std::min<wtf_size_t>(start_character_index - base, size_)
          : 0;
  const wtf_size_t end =
      end_character_index > base
          ? std::min<wtf_size_t>(end_character_index - base, size_)
          : start;
  return GlyphDataRange(run_.Get(), index_ + start,
                        std::max(start, end) - start);
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
    CHECK(!is_rtl);
    return FindCompactGlyphDataRange(start_character_index,
                                     end_character_index);
  }

  const auto comparer = [](const HarfBuzzRunGlyphData& glyph_data,
                           wtf_size_t index) {
    return glyph_data.character_index < index;
  };
  const auto [range_begin, range_end] = NonCompactGlyphPointers();
  if (!is_rtl) {
    const auto start_glyph = std::lower_bound(range_begin, range_end,
                                              start_character_index, comparer);
    if (start_glyph == range_end) [[unlikely]] {
      // No glyph matches; an empty range that still keeps the run.
      return GlyphDataRange(run_.Get(), index_ + size_, 0);
    }
    const auto end_glyph =
        std::lower_bound(start_glyph, range_end, end_character_index, comparer);
    const wtf_size_t start_index = CheckedDistance(range_begin, start_glyph);
    return GlyphDataRange(run_.Get(), index_ + start_index,
                          CheckedDistance(start_glyph, end_glyph));
  }

  // RTL needs to use reverse iterators because there maybe multiple glyphs
  // for a character, and we want to find the first one in the logical order.
  const auto rbegin = std::make_reverse_iterator(range_end);
  const auto rend = std::make_reverse_iterator(range_begin);
  const auto start_glyph_it =
      std::lower_bound(rbegin, rend, start_character_index, comparer);
  if (start_glyph_it == rend) [[unlikely]] {
    // No glyph matches; an empty range that still keeps the run.
    return GlyphDataRange(run_.Get(), index_, 0);
  }
  const auto end_glyph_it =
      std::lower_bound(start_glyph_it, rend, end_character_index, comparer);
  // reverse_iterator::base() is one past the referenced element, which gives
  // the inclusive begin and exclusive end in forward order.
  const auto start_glyph = end_glyph_it.base();
  const auto end_glyph = start_glyph_it.base();
  const wtf_size_t start_index = CheckedDistance(range_begin, start_glyph);
  return GlyphDataRange(run_.Get(), index_ + start_index,
                        CheckedDistance(start_glyph, end_glyph));
}

void GlyphDataRange::Trace(Visitor* visitor) const {
  visitor->Trace(run_);
}

}  // namespace blink
