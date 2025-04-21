// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_

#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data.h"

namespace blink {

// Represents a range of HarfBuzzRunGlyphData. |begin| and |end| follow the
// iterator pattern; i.e., |begin| is lower or equal to |end| in the address
// space regardless of LTR/RTL. |begin| is inclusive, |end| is exclusive.
class PLATFORM_EXPORT GlyphDataRange {
 public:
  GlyphDataRange() = default;
  GlyphDataRange(const HarfBuzzRunGlyphData* begin,
                 const HarfBuzzRunGlyphData* end,
                 const GlyphOffset* offsets)
      : begin_(begin), end_(end), offsets_(offsets) {}

  unsigned size() const { return static_cast<unsigned>(end_ - begin_); }
  bool IsEmpty() const { return begin_ == end_; }

  base::span<const HarfBuzzRunGlyphData> Glyphs() const {
    return base::span{begin_, end_};
  }

  using const_iterator = const HarfBuzzRunGlyphData*;
  const_iterator begin() const { return begin_; }
  const_iterator end() const { return end_; }

  bool HasOffsets() const { return offsets_; }
  base::span<const GlyphOffset> Offsets() const {
    return HasOffsets() ? base::span{offsets_, size()}
                        : base::span<const GlyphOffset>{};
  }
  const GlyphOffset* Offset() const { return offsets_; }

  GlyphDataRange FindGlyphDataRange(bool is_rtl,
                                    unsigned start_character_index,
                                    unsigned end_character_index) const;

 private:
  const HarfBuzzRunGlyphData* begin_ = nullptr;
  const HarfBuzzRunGlyphData* end_ = nullptr;
  const GlyphOffset* offsets_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_GLYPH_DATA_RANGE_H_
