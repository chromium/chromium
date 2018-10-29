/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_

#include <hb.h>
#include <memory>
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class SimpleFontData;

// This struct should be TriviallyCopyable so that std::copy() is equivalent to
// memcpy.
struct HarfBuzzRunGlyphData {
  DISALLOW_NEW();

  static constexpr unsigned kCharacterIndexBits = 15;
  static constexpr unsigned kMaxCharacterIndex = (1 << kCharacterIndexBits) - 1;
  static constexpr unsigned kMaxGlyphs = 1 << kCharacterIndexBits;

  uint16_t glyph;
  unsigned character_index : kCharacterIndexBits;
  unsigned safe_to_break_before : 1;
  float advance;
  FloatSize offset;

  void SetGlyphAndPositions(uint16_t glyph_id,
                            uint16_t character_index,
                            float advance,
                            const FloatSize& offset,
                            bool safe_to_break_before);
};

struct ShapeResult::RunInfo {
  USING_FAST_MALLOC(RunInfo);

 public:
  RunInfo(const SimpleFontData* font,
          hb_direction_t dir,
          CanvasRotationInVertical canvas_rotation,
          hb_script_t script,
          unsigned start_index,
          unsigned num_glyphs,
          unsigned num_characters)
      : font_data_(const_cast<SimpleFontData*>(font)),
        direction_(dir),
        canvas_rotation_(canvas_rotation),
        script_(script),
        glyph_data_(
            std::min(num_glyphs, HarfBuzzRunGlyphData::kMaxCharacterIndex + 1)),
        start_index_(start_index),
        num_characters_(num_characters),
        width_(0.0f) {}

  RunInfo(const RunInfo& other)
      : font_data_(other.font_data_),
        direction_(other.direction_),
        canvas_rotation_(other.canvas_rotation_),
        script_(other.script_),
        glyph_data_(other.glyph_data_),
        graphemes_(other.graphemes_),
        start_index_(other.start_index_),
        num_characters_(other.num_characters_),
        width_(other.width_) {}

  unsigned NumGlyphs() const { return glyph_data_.size(); }
  bool Rtl() const { return HB_DIRECTION_IS_BACKWARD(direction_); }
  bool IsHorizontal() const { return HB_DIRECTION_IS_HORIZONTAL(direction_); }
  CanvasRotationInVertical CanvasRotation() const { return canvas_rotation_; }
  unsigned NextSafeToBreakOffset(unsigned) const;
  unsigned PreviousSafeToBreakOffset(unsigned) const;
  float XPositionForVisualOffset(unsigned, AdjustMidCluster) const;
  float XPositionForOffset(unsigned, AdjustMidCluster) const;
  void CharacterIndexForXPosition(float,
                                  BreakGlyphsOption,
                                  GlyphIndexResult*) const;
  unsigned LimitNumGlyphs(unsigned start_glyph,
                          unsigned* num_glyphs_in_out,
                          const bool is_ltr,
                          const hb_glyph_info_t* glyph_infos);
  void SetGlyphAndPositions(unsigned index,
                            uint16_t glyph_id,
                            float advance,
                            float offset_x,
                            float offset_y);

  unsigned GlyphToCharacterIndex(unsigned i) const {
    return start_index_ + glyph_data_[i].character_index;
  }

  unsigned NumGraphemes(unsigned start, unsigned end) const;

  // For memory reporting.
  size_t ByteSize() const {
    return sizeof(this) + glyph_data_.size() * sizeof(HarfBuzzRunGlyphData);
  }

  // Represents a range of HarfBuzzRunGlyphData. |begin| and |end| follow the
  // iterator pattern; i.e., |begin| is lower or equal to |end| in the address
  // space regardless of LTR/RTL. |begin| is inclusive, |end| is exclusive.
  struct GlyphDataRange {
    HarfBuzzRunGlyphData* begin;
    HarfBuzzRunGlyphData* end;
  };

  // Find the range of HarfBuzzRunGlyphData for the specified character index
  // range. This function uses binary search twice, hence O(2 log n).
  GlyphDataRange FindGlyphDataRange(unsigned start_character_index,
                                    unsigned end_character_index) {
    const auto comparer = [](const HarfBuzzRunGlyphData& glyph_data,
                             unsigned index) {
      return glyph_data.character_index < index;
    };
    if (!Rtl()) {
      HarfBuzzRunGlyphData* start_glyph =
          std::lower_bound(glyph_data_.begin(), glyph_data_.end(),
                           start_character_index, comparer);
      if (UNLIKELY(start_glyph == glyph_data_.end()))
        return {nullptr, nullptr};
      HarfBuzzRunGlyphData* end_glyph = std::lower_bound(
          start_glyph, glyph_data_.end(), end_character_index, comparer);
      return {start_glyph, end_glyph};
    }

    // RTL needs to use reverse iterators because there maybe multiple glyphs
    // for a character, and we want to find the first one in the logical order.
    auto start_glyph =
        std::lower_bound(glyph_data_.rbegin(), glyph_data_.rend(),
                         start_character_index, comparer);
    if (UNLIKELY(start_glyph == glyph_data_.rend()))
      return {nullptr, nullptr};
    auto end_glyph = std::lower_bound(start_glyph, glyph_data_.rend(),
                                      end_character_index, comparer);
    // Convert reverse iterators to pointers. Then increment to make |begin|
    // inclusive and |end| exclusive.
    return {&*end_glyph + 1, &*start_glyph + 1};
  }

  // Creates a new RunInfo instance representing a subset of the current run.
  std::unique_ptr<RunInfo> CreateSubRun(unsigned start, unsigned end) {
    DCHECK(end > start);
    unsigned number_of_characters = std::min(end - start, num_characters_);
    auto glyphs = FindGlyphDataRange(start, end);
    unsigned number_of_glyphs =
        static_cast<unsigned>(std::distance(glyphs.begin, glyphs.end));

    auto run = std::make_unique<RunInfo>(
        font_data_.get(), direction_, canvas_rotation_, script_,
        start_index_ + start, number_of_glyphs, number_of_characters);

    static_assert(base::is_trivially_copyable<HarfBuzzRunGlyphData>::value,
                  "HarfBuzzRunGlyphData should be trivially copyable");
    std::copy(glyphs.begin, glyphs.end, run->glyph_data_.begin());

    float total_advance = 0;
    for (HarfBuzzRunGlyphData& glyph_data : run->glyph_data_) {
      glyph_data.character_index -= start;
      total_advance += glyph_data.advance;
    }

    run->width_ = total_advance;
    run->num_characters_ = number_of_characters;

    return run;
  }

  void ExpandRangeToIncludePartialGlyphs(int offset, int* from, int* to) const {
    int start = !Rtl() ? offset : (offset + num_characters_);
    int end = offset + num_characters_;

    for (unsigned i = 0; i < glyph_data_.size(); ++i) {
      int index = offset + glyph_data_[i].character_index;
      if (start == index)
        continue;

      if (!Rtl())
        end = index;

      if (end > *from && start < *to) {
        *from = std::min(*from, start);
        *to = std::max(*to, end);
      }

      if (!Rtl())
        end = num_characters_;
      else
        end = start;
      start = index;
    }

    if (end > *from && start < *to) {
      *from = std::min(*from, start);
      *to = std::max(*to, end);
    }
  }

  scoped_refptr<SimpleFontData> font_data_;
  hb_direction_t direction_;
  // For upright-in-vertical we need to tell the ShapeResultBloberizer to rotate
  // the canvas back 90deg for this RunInfo.
  CanvasRotationInVertical canvas_rotation_;
  hb_script_t script_;
  Vector<HarfBuzzRunGlyphData> glyph_data_;

  // graphemes_[i] is the number of graphemes up to (and including) the ith
  // character in the run.
  Vector<unsigned> graphemes_;

  unsigned start_index_;
  unsigned num_characters_;
  float width_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
