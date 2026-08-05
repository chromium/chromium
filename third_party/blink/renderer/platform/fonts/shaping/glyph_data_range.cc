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

GlyphDataRange::GlyphDataRange(const GlyphDataRange& range,
                               const_iterator begin_glyph,
                               const_iterator end_glyph)
    : run_(range.run_) {
  DCHECK(run_);
  CHECK_GE(begin_glyph, run_->glyph_data_.begin());
  index_ = CheckedDistance(run_->glyph_data_.begin(), begin_glyph);
  DCHECK_GE(index_, range.index_);
  CHECK_LE(index_, run_->NumGlyphs());
  CHECK_GE(end_glyph, begin_glyph);
  size_ = CheckedDistance(begin_glyph, end_glyph);
  CHECK_LE(size_, run_->NumGlyphs() - index_);
}

base::span<const HarfBuzzRunGlyphData> GlyphDataRange::Glyphs() const {
  return run_ ? base::span{run_->glyph_data_}.subspan(index_, size_)
              : base::span<const HarfBuzzRunGlyphData>{};
}

GlyphDataRange::const_iterator GlyphDataRange::begin() const {
  return Glyphs().data();
}

GlyphDataRange::const_iterator GlyphDataRange::end() const {
  return base::to_address(Glyphs().end());
}

bool GlyphDataRange::HasOffsets() const {
  return run_ && run_->glyph_data_.HasNonZeroOffsets();
}

base::span<const GlyphOffset> GlyphDataRange::Offsets() const {
  if (HasOffsets()) [[unlikely]] {
    return run_->glyph_data_.Offsets().subspan(index_, size_);
  }
  return base::span<const GlyphOffset>{};
}

// Find the range of HarfBuzzRunGlyphData for the specified character index
// range. This function uses binary search twice, hence O(2 log n).
GlyphDataRange GlyphDataRange::FindGlyphDataRange(
    bool is_rtl,
    unsigned start_character_index,
    unsigned end_character_index) const {
  const auto comparer = [](const HarfBuzzRunGlyphData& glyph_data,
                           unsigned index) {
    return glyph_data.character_index < index;
  };
  if (!is_rtl) {
    const HarfBuzzRunGlyphData* start_glyph =
        std::lower_bound(begin(), end(), start_character_index, comparer);
    if (start_glyph == end()) [[unlikely]] {
      // No glyph matches; an empty range that still keeps the run.
      return {*this, start_glyph, start_glyph};
    }
    const HarfBuzzRunGlyphData* end_glyph =
        std::lower_bound(start_glyph, end(), end_character_index, comparer);
    return {*this, start_glyph, end_glyph};
  }

  // RTL needs to use reverse iterators because there maybe multiple glyphs
  // for a character, and we want to find the first one in the logical order.
  const auto rbegin = std::reverse_iterator<const HarfBuzzRunGlyphData*>(end());
  const auto rend = std::reverse_iterator<const HarfBuzzRunGlyphData*>(begin());
  const auto start_glyph_it =
      std::lower_bound(rbegin, rend, start_character_index, comparer);
  if (start_glyph_it == rend) [[unlikely]] {
    // No glyph matches; an empty range that still keeps the run.
    return {*this, begin(), begin()};
  }
  const auto end_glyph_it =
      std::lower_bound(start_glyph_it, rend, end_character_index, comparer);
  // reverse_iterator::base() is one past the referenced element, which gives
  // the inclusive begin and exclusive end in forward order.
  const HarfBuzzRunGlyphData* start_glyph = end_glyph_it.base();
  const HarfBuzzRunGlyphData* end_glyph = start_glyph_it.base();
  return {*this, start_glyph, end_glyph};
}

void GlyphDataRange::Trace(Visitor* visitor) const {
  visitor->Trace(run_);
}

}  // namespace blink
