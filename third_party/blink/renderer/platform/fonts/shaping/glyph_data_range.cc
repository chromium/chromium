// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"

namespace blink {

GlyphDataRange::GlyphDataRange(const ShapeResultRun& run)
    : run_(&run), size_(run.glyph_data_.size()) {}

GlyphDataRange::GlyphDataRange(const GlyphDataRange& range,
                               const_iterator begin_glyph,
                               const_iterator end_glyph)
    : run_(range.run_) {
  DCHECK(run_);
  CHECK_GE(begin_glyph, run_->glyph_data_.begin());
  index_ = begin_glyph - run_->glyph_data_.begin();
  DCHECK_GE(index_, range.index_);
  CHECK_LE(index_, run_->NumGlyphs());
  CHECK_GE(end_glyph, begin_glyph);
  size_ = end_glyph - begin_glyph;
  CHECK_LE(size_, run_->NumGlyphs() - index_);
}

base::span<const HarfBuzzRunGlyphData> GlyphDataRange::Glyphs() const {
  return run_ ? base::span{run_->glyph_data_}.subspan(index_, size_)
              : base::span<const HarfBuzzRunGlyphData>{};
}

GlyphDataRange::const_iterator GlyphDataRange::begin() const {
  return run_ ? UNSAFE_TODO(run_->glyph_data_.begin() + index_) : nullptr;
}

GlyphDataRange::const_iterator GlyphDataRange::end() const {
  return run_ ? UNSAFE_TODO(run_->glyph_data_.begin() + index_ + size_)
              : nullptr;
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
      return GlyphDataRange();
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
    return GlyphDataRange();
  }
  const auto end_glyph_it =
      std::lower_bound(start_glyph_it, rend, end_character_index, comparer);
  // Convert reverse iterators to pointers. Then increment to make |begin|
  // inclusive and |end| exclusive.
  const HarfBuzzRunGlyphData* start_glyph = UNSAFE_TODO(&*end_glyph_it + 1);
  const HarfBuzzRunGlyphData* end_glyph = UNSAFE_TODO(&*start_glyph_it + 1);
  return {*this, start_glyph, end_glyph};
}

void GlyphDataRange::Trace(Visitor* visitor) const {
  visitor->Trace(run_);
}

}  // namespace blink
