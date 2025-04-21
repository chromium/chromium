// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data_range.h"

namespace blink {

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
    if (HasOffsets()) {
      return {start_glyph, end_glyph, Offset() + (start_glyph - begin())};
    }
    return {start_glyph, end_glyph, nullptr};
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
  const HarfBuzzRunGlyphData* start_glyph = &*end_glyph_it + 1;
  const HarfBuzzRunGlyphData* end_glyph = &*start_glyph_it + 1;
  if (HasOffsets()) {
    return {start_glyph, end_glyph, Offset() + (start_glyph - begin())};
  }
  return {start_glyph, end_glyph, nullptr};
}

}  // namespace blink
