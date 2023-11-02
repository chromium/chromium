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

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"

#include <hb.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_bounds_accumulator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

constexpr unsigned HarfBuzzRunGlyphData::kMaxCharacterIndex;
constexpr unsigned HarfBuzzRunGlyphData::kMaxGlyphs;

struct SameSizeAsHarfBuzzRunGlyphData {
  unsigned glyph : 16;
  unsigned char_index_and_bit_field : 16;
  float advance;
};

ASSERT_SIZE(HarfBuzzRunGlyphData, SameSizeAsHarfBuzzRunGlyphData);

struct SameSizeAsRunInfo : public RefCounted<SameSizeAsRunInfo> {
  struct GlyphDataCollection {
    void* pointers[2];
    unsigned integer;
  } glyph_data;
  void* pointer;
  Vector<int> vector;
  int integers[6];
};

ASSERT_SIZE(ShapeResult::RunInfo, SameSizeAsRunInfo);

struct SameSizeAsShapeResult : public RefCounted<SameSizeAsShapeResult> {
  float floats[5];
  Vector<int> vector;
  void* pointers[2];
  unsigned integers[2];
  unsigned bitfields : 32;
};

ASSERT_SIZE(ShapeResult, SameSizeAsShapeResult);

unsigned ShapeResult::RunInfo::NextSafeToBreakOffset(unsigned offset) const {
  DCHECK_LE(offset, num_characters_);
  if (IsLtr()) {
    for (const auto& glyph_data : glyph_data_) {
      if (glyph_data.safe_to_break_before &&
          glyph_data.character_index >= offset)
        return glyph_data.character_index;
    }
  } else {
    for (const auto& glyph_data : base::Reversed(glyph_data_)) {
      if (glyph_data.safe_to_break_before &&
          glyph_data.character_index >= offset)
        return glyph_data.character_index;
    }
  }

  // Next safe break is at the end of the run.
  return num_characters_;
}

unsigned ShapeResult::RunInfo::PreviousSafeToBreakOffset(
    unsigned offset) const {
  if (offset >= num_characters_)
    return num_characters_;
  if (IsLtr()) {
    for (const auto& glyph_data : base::Reversed(glyph_data_)) {
      if (glyph_data.safe_to_break_before &&
          glyph_data.character_index <= offset)
        return glyph_data.character_index;
    }
  } else {
    for (const auto& glyph_data : glyph_data_) {
      if (glyph_data.safe_to_break_before &&
          glyph_data.character_index <= offset)
        return glyph_data.character_index;
    }
  }

  // Next safe break is at the start of the run.
  return 0;
}

float ShapeResult::RunInfo::XPositionForVisualOffset(
    unsigned offset,
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_LT(offset, num_characters_);
  if (IsRtl())
    offset = num_characters_ - offset - 1;
  return XPositionForOffset(offset, adjust_mid_cluster);
}

unsigned ShapeResult::RunInfo::NumGraphemes(unsigned start,
                                            unsigned end) const {
  if (graphemes_.size() == 0 || start >= num_characters_)
    return 0;
  CHECK_LT(start, end);
  CHECK_LE(end, num_characters_);
  CHECK_EQ(num_characters_, graphemes_.size());
  return graphemes_[end - 1] - graphemes_[start] + 1;
}

void ShapeResult::EnsureGraphemes(const StringView& text) const {
  CHECK_EQ(NumCharacters(), text.length());

  // Hit-testing, canvas, etc. may still call this function for 0-length text,
  // or glyphs may be missing at all.
  if (runs_.empty())
    return;

  bool is_computed = !runs_.front()->graphemes_.empty();
#if DCHECK_IS_ON()
  for (const auto& run : runs_)
    DCHECK_EQ(is_computed, !run->graphemes_.empty());
#endif
  if (is_computed)
    return;

  unsigned result_start_index = StartIndex();
  for (const scoped_refptr<RunInfo>& run : runs_) {
    if (!run)
      continue;
    DCHECK_GE(run->start_index_, result_start_index);
    GraphemesClusterList(
        StringView(text, run->start_index_ - result_start_index,
                   run->num_characters_),
        &run->graphemes_);
  }
}

// XPositionForOffset returns the X position (in layout space) from the
// beginning of the run to the beginning of the cluster of glyphs for X
// character.
// For RTL, beginning means the right most side of the cluster.
// Characters may spawn multiple glyphs.
// In the case that multiple characters form a Unicode grapheme cluster, we
// distribute the width of the grapheme cluster among the number of cursor
// positions returned by cursor-based TextBreakIterator.
float ShapeResult::RunInfo::XPositionForOffset(
    unsigned offset,
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_LE(offset, num_characters_);
  const unsigned num_glyphs = glyph_data_.size();

  // In this context, a glyph sequence is a sequence of glyphs that shares the
  // same character_index and therefore represent the same interval of source
  // characters. glyph_sequence_start marks the character index at the beginning
  // of the interval of characters for which this glyph sequence was formed as
  // the result of shaping; glyph_sequence_end marks the end of the interval of
  // characters for which this glyph sequence was formed. [glyph_sequence_start,
  // glyph_sequence_end) is inclusive on the start for the range of characters
  // of the current sequence we are visiting.
  unsigned glyph_sequence_start = 0;
  unsigned glyph_sequence_end = num_characters_;
  // the advance of the current glyph sequence.
  float glyph_sequence_advance = 0.0;
  // the accumulated advance up to the current glyph sequence.
  float accumulated_position = 0;

  if (IsLtr()) {
    for (unsigned i = 0; i < num_glyphs; ++i) {
      unsigned current_glyph_char_index = glyph_data_[i].character_index;
      // If this glyph is still part of the same glyph sequence for the grapheme
      // cluster at character index glyph_sequence_start, add its advance to the
      // glyph_sequence's advance.
      if (glyph_sequence_start == current_glyph_char_index) {
        glyph_sequence_advance += glyph_data_[i].advance;
        continue;
      }

      // We are about to move out of a glyph sequence that contains offset, so
      // the current glyph sequence is the one we are looking for.
      if (glyph_sequence_start <= offset && offset < current_glyph_char_index) {
        glyph_sequence_end = current_glyph_char_index;
        break;
      }

      glyph_sequence_start = current_glyph_char_index;
      // Since we always update glyph_sequence_end when we break, set this to
      // last_character in case this is the final iteration of the loop.
      glyph_sequence_end = num_characters_;
      accumulated_position += glyph_sequence_advance;
      glyph_sequence_advance = glyph_data_[i].advance;
    }

  } else {
    glyph_sequence_start = glyph_sequence_end = num_characters_;

    for (unsigned i = 0; i < num_glyphs; ++i) {
      unsigned current_glyph_char_index = glyph_data_[i].character_index;
      // If this glyph is still part of the same glyph sequence for the grapheme
      // cluster at character index glyph_sequence_start, add its advance to the
      // glyph_sequence's advance.
      if (glyph_sequence_start == current_glyph_char_index) {
        glyph_sequence_advance += glyph_data_[i].advance;
        continue;
      }

      // We are about to move out of a glyph sequence that contains offset, so
      // the current glyph sequence is the one we are looking for.
      if (glyph_sequence_start <= offset && offset < glyph_sequence_end) {
        break;
      }

      glyph_sequence_end = glyph_sequence_start;
      glyph_sequence_start = current_glyph_char_index;
      accumulated_position += glyph_sequence_advance;
      glyph_sequence_advance = glyph_data_[i].advance;
    }
  }

  // This is the character position inside the glyph sequence.
  unsigned pos = offset - glyph_sequence_start;

  // We calculate the number of Unicode grapheme clusters (actually cursor
  // position stops) on the subset of characters. We use this to divide
  // glyph_sequence_advance by the number of unicode grapheme clusters this
  // glyph sequence was shaped for, and thus linearly interpolate the cursor
  // position based on accumulated position and a fraction of
  // glyph_sequence_advance.
  unsigned graphemes = NumGraphemes(glyph_sequence_start, glyph_sequence_end);
  if (graphemes > 1) {
    DCHECK_GE(glyph_sequence_end, glyph_sequence_start);
    unsigned size = glyph_sequence_end - glyph_sequence_start;
    unsigned place = graphemes * pos / size;
    pos -= place;
    glyph_sequence_advance = glyph_sequence_advance / graphemes;
    if (IsRtl()) {
      accumulated_position += glyph_sequence_advance * (graphemes - place - 1);
    } else {
      accumulated_position += glyph_sequence_advance * place;
    }
  }

  // Re-adapt based on adjust_mid_cluster. On LTR, if we want AdjustToEnd and
  // offset is not at the beginning, we need to jump to the right side of the
  // grapheme. On RTL, if we want AdjustToStart and offset is not at the end, we
  // need to jump to the left side of the grapheme.
  if (IsLtr() && adjust_mid_cluster == AdjustMidCluster::kToEnd && pos != 0) {
    accumulated_position += glyph_sequence_advance;
  } else if (IsRtl() && adjust_mid_cluster == AdjustMidCluster::kToEnd &&
             pos != 0) {
    accumulated_position -= glyph_sequence_advance;
  }

  if (IsRtl()) {
    // For RTL, we return the right side.
    accumulated_position += glyph_sequence_advance;
  }

  return accumulated_position;
}

// In some ways, CharacterIndexForXPosition is the reverse of
// XPositionForOffset. Given a target pixel distance on screen space, returns a
// character index for the end of the interval that would be included within
// that space. @break_glyphs controls whether we use grapheme information
// to break glyphs into grapheme clusters and return character that are a part
// of a glyph.
void ShapeResult::RunInfo::CharacterIndexForXPosition(
    float target_x,
    BreakGlyphsOption break_glyphs,
    GlyphIndexResult* result) const {
  DCHECK(target_x >= 0 && target_x <= width_);

  result->origin_x = 0;
  unsigned glyph_sequence_start = 0;
  unsigned glyph_sequence_end = num_characters_;
  result->advance = 0.0;

  // on RTL, we start on the last index.
  if (IsRtl()) {
    glyph_sequence_start = glyph_sequence_end = num_characters_;
  }

  for (const HarfBuzzRunGlyphData& glyph_data : glyph_data_) {
    unsigned current_glyph_char_index = glyph_data.character_index;
    // If the glyph is part of the same sequence, we just accumulate the
    // advance.
    if (glyph_sequence_start == current_glyph_char_index) {
      result->advance += glyph_data.advance;
      continue;
    }

    // Since we are about to move to the next sequence of glyphs, check if
    // the target falls inside it, if it does, we found our sequence.
    if (result->origin_x + result->advance > target_x) {
      if (IsLtr()) {
        glyph_sequence_end = current_glyph_char_index;
      }
      break;
    }

    // Move to the next sequence, update accumulated_x.
    if (IsRtl()) {
      // Notice that on RTL, as we move to our next sequence, we already know
      // both bounds. Nonetheless, we still need to move forward so we can
      // capture all glyphs of this sequence.
      glyph_sequence_end = glyph_sequence_start;
    }
    glyph_sequence_start = current_glyph_char_index;
    result->origin_x += result->advance;
    result->advance = glyph_data.advance;
  }

  // At this point, we have [glyph_sequence_start, glyph_sequence_end)
  // representing a sequence of glyphs, of size glyph_sequence_advance. We
  // linearly interpolate how much space each character takes, and reduce the
  // sequence to only match the character size.
  if (break_glyphs && glyph_sequence_end > glyph_sequence_start) {
    int graphemes = NumGraphemes(glyph_sequence_start, glyph_sequence_end);
    if (graphemes > 1) {
      float unit_size = result->advance / graphemes;
      unsigned step = floor((target_x - result->origin_x) / unit_size);
      unsigned glyph_length = glyph_sequence_end - glyph_sequence_start;
      unsigned final_size = floor(glyph_length / graphemes);
      result->origin_x += unit_size * step;
      if (IsLtr()) {
        glyph_sequence_start += step;
        glyph_sequence_end = glyph_sequence_start + final_size;
      } else {
        glyph_sequence_end -= step;
        glyph_sequence_start = glyph_sequence_end - final_size;
      }
      result->advance = unit_size;
    }
  }

  if (IsLtr()) {
    result->left_character_index = glyph_sequence_start;
    result->right_character_index = glyph_sequence_end;
  } else {
    result->left_character_index = glyph_sequence_end;
    result->right_character_index = glyph_sequence_start;
  }
}

ShapeResult::ShapeResult(scoped_refptr<const SimpleFontData> font_data,
                         unsigned start_index,
                         unsigned num_characters,
                         TextDirection direction)
    : width_(0),
      primary_font_(font_data),
      start_index_(start_index),
      num_characters_(num_characters),
      num_glyphs_(0),
      direction_(static_cast<unsigned>(direction)),
      has_vertical_offsets_(false),
      is_applied_spacing_(false) {}

ShapeResult::ShapeResult(const Font* font,
                         unsigned start_index,
                         unsigned num_characters,
                         TextDirection direction)
    : ShapeResult(font->PrimaryFont(), start_index, num_characters, direction) {
}

ShapeResult::ShapeResult(const ShapeResult& other)
    : width_(other.width_),
      primary_font_(other.primary_font_),
      start_index_(other.start_index_),
      num_characters_(other.num_characters_),
      num_glyphs_(other.num_glyphs_),
      direction_(other.direction_),
      has_vertical_offsets_(other.has_vertical_offsets_),
      is_applied_spacing_(other.is_applied_spacing_) {
  runs_.reserve(other.runs_.size());
  for (const auto& run : other.runs_)
    runs_.push_back(run->Create(*run.get()));
}

ShapeResult::~ShapeResult() = default;

size_t ShapeResult::ByteSize() const {
  size_t self_byte_size = sizeof(*this);
  for (unsigned i = 0; i < runs_.size(); ++i) {
    self_byte_size += runs_[i]->ByteSize();
  }
  return self_byte_size;
}

bool ShapeResult::IsStartSafeToBreak() const {
  // Empty is likely a |SubRange| at the middle of a cluster or a ligature.
  if (UNLIKELY(runs_.empty()))
    return false;
  const RunInfo* run = nullptr;
  const HarfBuzzRunGlyphData* glyph_data = nullptr;
  if (IsLtr()) {
    run = runs_.front().get();
    glyph_data = &run->glyph_data_.front();
  } else {
    run = runs_.back().get();
    glyph_data = &run->glyph_data_.back();
  }
  return glyph_data->safe_to_break_before &&
         // If the glyph for the first character is missing, consider not safe.
         StartIndex() == run->start_index_ + glyph_data->character_index;
}

unsigned ShapeResult::NextSafeToBreakOffset(unsigned index) const {
  for (auto* it = runs_.begin(); it != runs_.end(); ++it) {
    const auto& run = *it;
    if (!run)
      continue;

    unsigned run_start = run->start_index_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset <= run->num_characters_) {
        return run->NextSafeToBreakOffset(offset) + run_start;
      }
      if (IsRtl()) {
        if (it == runs_.begin())
          return run_start + run->num_characters_;
        const auto& previous_run = *--it;
        return previous_run->start_index_;
      }
    } else if (IsLtr()) {
      return run_start;
    }
  }

  return EndIndex();
}

unsigned ShapeResult::PreviousSafeToBreakOffset(unsigned index) const {
  for (auto it = runs_.rbegin(); it != runs_.rend(); ++it) {
    const auto& run = *it;
    if (!run)
      continue;

    unsigned run_start = run->start_index_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset <= run->num_characters_) {
        return run->PreviousSafeToBreakOffset(offset) + run_start;
      }
      if (IsLtr()) {
        return run_start + run->num_characters_;
      }
    } else if (IsRtl()) {
      if (it == runs_.rbegin())
        return run->start_index_;
      const auto& previous_run = *--it;
      return previous_run->start_index_ + previous_run->num_characters_;
    }
  }

  return StartIndex();
}

// If the position is outside of the result, returns the start or the end offset
// depends on the position.
void ShapeResult::OffsetForPosition(float target_x,
                                    BreakGlyphsOption break_glyphs,
                                    GlyphIndexResult* result) const {
  if (target_x <= 0) {
    if (IsRtl()) {
      result->left_character_index = result->right_character_index =
          NumCharacters();
    }
    return;
  }

  unsigned characters_so_far = IsRtl() ? NumCharacters() : 0;
  float current_x = 0;

  for (const scoped_refptr<RunInfo>& run_ptr : runs_) {
    const RunInfo* run = run_ptr.get();
    if (!run)
      continue;
    if (IsRtl())
      characters_so_far -= run->num_characters_;
    float next_x = current_x + run->width_;
    float offset_for_run = target_x - current_x;
    if (offset_for_run >= 0 && offset_for_run < run->width_) {
      // The x value in question is within this script run.
      run->CharacterIndexForXPosition(offset_for_run, break_glyphs, result);
      result->characters_on_left_runs = characters_so_far;
      if (IsRtl()) {
        result->left_character_index =
            characters_so_far + result->left_character_index;
        result->right_character_index =
            characters_so_far + result->right_character_index;
        DCHECK_LE(result->left_character_index, NumCharacters() + 1);
        DCHECK_LE(result->right_character_index, NumCharacters());
      } else {
        result->left_character_index += characters_so_far;
        result->right_character_index += characters_so_far;
        DCHECK_LE(result->left_character_index, NumCharacters());
        DCHECK_LE(result->right_character_index, NumCharacters() + 1);
      }
      result->origin_x += current_x;
      return;
    }
    if (IsLtr())
      characters_so_far += run->num_characters_;
    current_x = next_x;
  }

  if (IsRtl()) {
    result->left_character_index = 0;
    result->right_character_index = 0;
  } else {
    result->left_character_index += characters_so_far;
    result->right_character_index += characters_so_far;
  }

  result->characters_on_left_runs = characters_so_far;

  DCHECK_LE(result->left_character_index, NumCharacters());
  DCHECK_LE(result->right_character_index, NumCharacters() + 1);
}

unsigned ShapeResult::OffsetForPosition(float x,
                                        BreakGlyphsOption break_glyphs) const {
  GlyphIndexResult result;
  OffsetForPosition(x, break_glyphs, &result);

  // For LTR, the offset is always the left one.
  if (IsLtr())
    return result.left_character_index;

  // For RTL the offset is the right one, except that the interval is open
  // on other side. So in case we are exactly at the boundary, we return the
  // left index.
  if (x == result.origin_x)
    return result.left_character_index;
  return result.right_character_index;
}

unsigned ShapeResult::CaretOffsetForHitTest(
    float x,
    const StringView& text,
    BreakGlyphsOption break_glyphs_option) const {
  if (break_glyphs_option)
    EnsureGraphemes(text);

  GlyphIndexResult result;
  OffsetForPosition(x, break_glyphs_option, &result);

  if (x - result.origin_x <= result.advance / 2)
    return result.left_character_index;
  return result.right_character_index;
}

unsigned ShapeResult::OffsetToFit(float x, TextDirection line_direction) const {
  GlyphIndexResult result;
  OffsetForPosition(x, BreakGlyphsOption(false), &result);

  if (blink::IsLtr(line_direction))
    return result.left_character_index;

  if (x == result.origin_x)
    return result.left_character_index;
  return result.right_character_index;
}

float ShapeResult::PositionForOffset(
    unsigned absolute_offset,
    AdjustMidCluster adjust_mid_cluster) const {
  float x = 0;
  float offset_x = 0;

  // The absolute_offset argument represents the offset for the entire
  // ShapeResult while offset is continuously updated to be relative to the
  // current run.
  unsigned offset = absolute_offset;

  if (IsRtl()) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    x = width_;
    if (offset < NumCharacters())
      offset = NumCharacters() - offset - 1;
    x -= Width();
  }

  for (unsigned i = 0; i < runs_.size(); i++) {
    if (!runs_[i])
      continue;
    DCHECK_EQ(IsRtl(), runs_[i]->IsRtl());
    unsigned num_characters = runs_[i]->num_characters_;

    if (!offset_x && offset < num_characters) {
      offset_x =
          runs_[i]->XPositionForVisualOffset(offset, adjust_mid_cluster) + x;
      break;
    }

    offset -= num_characters;
    x += runs_[i]->width_;
  }

  // The position in question might be just after the text.
  if (!offset_x && absolute_offset == NumCharacters())
    return IsRtl() ? 0 : width_;

  return offset_x;
}

float ShapeResult::CaretPositionForOffset(
    unsigned offset,
    const StringView& text,
    AdjustMidCluster adjust_mid_cluster) const {
  EnsureGraphemes(text);
  return PositionForOffset(offset, adjust_mid_cluster);
}

void ShapeResult::FallbackFonts(
    HashSet<const SimpleFontData*>* fallback) const {
  DCHECK(fallback);
  DCHECK(primary_font_);
  for (unsigned i = 0; i < runs_.size(); ++i) {
    if (runs_[i] && runs_[i]->font_data_ &&
        runs_[i]->font_data_ != primary_font_) {
      fallback->insert(runs_[i]->font_data_.get());
    }
  }
}

void ShapeResult::GetRunFontData(Vector<RunFontData>* font_data) const {
  for (const auto& run : runs_) {
    font_data->push_back(
        RunFontData({run->font_data_.get(), run->glyph_data_.size()}));
  }
}

template <bool has_non_zero_glyph_offsets>
float ShapeResult::ForEachGlyphImpl(float initial_advance,
                                    GlyphCallback glyph_callback,
                                    void* context,
                                    const RunInfo& run) const {
  auto glyph_offsets = run.glyph_data_.GetOffsets<has_non_zero_glyph_offsets>();
  auto total_advance = initial_advance;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run.direction_);
  for (const auto& glyph_data : run.glyph_data_) {
    glyph_callback(context, run.start_index_ + glyph_data.character_index,
                   glyph_data.glyph, *glyph_offsets, total_advance,
                   is_horizontal, run.canvas_rotation_, run.font_data_.get());
    total_advance += glyph_data.advance;
    ++glyph_offsets;
  }
  return total_advance;
}

float ShapeResult::ForEachGlyph(float initial_advance,
                                GlyphCallback glyph_callback,
                                void* context) const {
  auto total_advance = initial_advance;
  for (const auto& run : runs_) {
    if (run->glyph_data_.HasNonZeroOffsets()) {
      total_advance =
          ForEachGlyphImpl<true>(total_advance, glyph_callback, context, *run);
    } else {
      total_advance =
          ForEachGlyphImpl<false>(total_advance, glyph_callback, context, *run);
    }
  }
  return total_advance;
}

template <bool has_non_zero_glyph_offsets>
float ShapeResult::ForEachGlyphImpl(float initial_advance,
                                    unsigned from,
                                    unsigned to,
                                    unsigned index_offset,
                                    GlyphCallback glyph_callback,
                                    void* context,
                                    const RunInfo& run) const {
  auto glyph_offsets = run.glyph_data_.GetOffsets<has_non_zero_glyph_offsets>();
  auto total_advance = initial_advance;
  unsigned run_start = run.start_index_ + index_offset;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run.direction_);
  const SimpleFontData* font_data = run.font_data_.get();

  if (run.IsLtr()) {  // Left-to-right
    for (const auto& glyph_data : run.glyph_data_) {
      const unsigned character_index = run_start + glyph_data.character_index;
      if (character_index >= to)
        break;
      if (character_index >= from) {
        glyph_callback(context, character_index, glyph_data.glyph,
                       *glyph_offsets, total_advance, is_horizontal,
                       run.canvas_rotation_, font_data);
      }
      total_advance += glyph_data.advance;
      ++glyph_offsets;
    }
  } else {  // Right-to-left
    for (const auto& glyph_data : run.glyph_data_) {
      const unsigned character_index = run_start + glyph_data.character_index;
      if (character_index < from)
        break;
      if (character_index < to) {
        glyph_callback(context, character_index, glyph_data.glyph,
                       *glyph_offsets, total_advance, is_horizontal,
                       run.canvas_rotation_, font_data);
      }
      total_advance += glyph_data.advance;
      ++glyph_offsets;
    }
  }
  return total_advance;
}

float ShapeResult::ForEachGlyph(float initial_advance,
                                unsigned from,
                                unsigned to,
                                unsigned index_offset,
                                GlyphCallback glyph_callback,
                                void* context) const {
  auto total_advance = initial_advance;
  for (const auto& run : runs_) {
    if (run->glyph_data_.HasNonZeroOffsets()) {
      total_advance = ForEachGlyphImpl<true>(
          total_advance, from, to, index_offset, glyph_callback, context, *run);
    } else {
      total_advance = ForEachGlyphImpl<false>(
          total_advance, from, to, index_offset, glyph_callback, context, *run);
    }
  }
  return total_advance;
}

unsigned ShapeResult::CountGraphemesInCluster(base::span<const UChar> str,
                                              uint16_t start_index,
                                              uint16_t end_index) {
  if (start_index > end_index)
    std::swap(start_index, end_index);
  uint16_t length = end_index - start_index;
  TextBreakIterator* cursor_pos_iterator =
      CursorMovementIterator(str.subspan(start_index, length));
  if (!cursor_pos_iterator)
    return 0;

  int cursor_pos = cursor_pos_iterator->current();
  int num_graphemes = -1;
  while (0 <= cursor_pos) {
    cursor_pos = cursor_pos_iterator->next();
    num_graphemes++;
  }
  return std::max(0, num_graphemes);
}

float ShapeResult::ForEachGraphemeClusters(const StringView& text,
                                           float initial_advance,
                                           unsigned from,
                                           unsigned to,
                                           unsigned index_offset,
                                           GraphemeClusterCallback callback,
                                           void* context) const {
  unsigned run_offset = index_offset;
  float advance_so_far = initial_advance;
  for (const auto& run : runs_) {
    unsigned graphemes_in_cluster = 1;
    float cluster_advance = 0;

    // FIXME: should this be run->direction_?
    bool rtl = Direction() == TextDirection::kRtl;

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be
    // broken down further from a text shaping point of view.  A cluster can
    // contain multiple glyphs and grapheme clusters, with mutually overlapping
    // boundaries.
    uint16_t cluster_start = static_cast<uint16_t>(
        rtl ? run->start_index_ + run->num_characters_ + run_offset
            : run->GlyphToCharacterIndex(0) + run_offset);

    const unsigned num_glyphs = run->glyph_data_.size();
    for (unsigned i = 0; i < num_glyphs; ++i) {
      const HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];
      uint16_t current_character_index =
          run->start_index_ + glyph_data.character_index + run_offset;
      bool is_run_end = (i + 1 == num_glyphs);
      bool is_cluster_end =
          is_run_end || (run->GlyphToCharacterIndex(i + 1) + run_offset !=
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
              is_run_end ? run->start_index_ + run->num_characters_ + run_offset
                         : run->GlyphToCharacterIndex(i + 1) + run_offset);
        }
        graphemes_in_cluster =
            CountGraphemesInCluster(text.Span16(), cluster_start, cluster_end);
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

// TODO(kojii): VC2015 fails to explicit instantiation of a member function.
// Typed functions + this private function are to instantiate instances.
template <typename TextContainerType>
void ShapeResult::ApplySpacingImpl(
    ShapeResultSpacing<TextContainerType>& spacing,
    int text_start_offset) {
  float offset = 0;
  float total_space = 0;
  float space = 0;
  for (auto& run : runs_) {
    if (!run)
      continue;
    unsigned run_start_index = run->start_index_ + text_start_offset;
    float total_space_for_run = 0;
    for (wtf_size_t i = 0; i < run->glyph_data_.size(); i++) {
      HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];

      // Skip if it's not a grapheme cluster boundary.
      if (i + 1 < run->glyph_data_.size() &&
          glyph_data.character_index ==
              run->glyph_data_[i + 1].character_index) {
        continue;
      }

      typename ShapeResultSpacing<TextContainerType>::ComputeSpacingParameters
          parameters{.index = run_start_index + glyph_data.character_index,
                     .original_advance = glyph_data.advance};
      space = spacing.ComputeSpacing(parameters, offset);
      glyph_data.advance += space;
      total_space_for_run += space;

      // |offset| is non-zero only when justifying CJK characters that follow
      // non-CJK characters.
      if (UNLIKELY(offset)) {
        if (run->IsHorizontal()) {
          run->glyph_data_.AddOffsetWidthAt(i, offset);
        } else {
          run->glyph_data_.AddOffsetHeightAt(i, offset);
          has_vertical_offsets_ = true;
        }
        offset = 0;
      }
    }
    run->width_ += total_space_for_run;
    total_space += total_space_for_run;
  }
  width_ += total_space;

  // The spacing on the right of the last glyph does not affect the glyph
  // bounding box. Thus, the glyph bounding box becomes smaller than the advance
  // if the letter spacing is positve, or larger if negative.
  if (space) {
    total_space -= space;

    // TODO(kojii): crbug.com/768284: There are cases where
    // InlineTextBox::LogicalWidth() is round down of ShapeResult::Width() in
    // LayoutUnit. Ceiling the width did not help. Add 1px to avoid cut-off.
    if (space < 0)
      total_space += 1;
  }
}

void ShapeResult::ApplySpacing(ShapeResultSpacing<String>& spacing,
                               int text_start_offset) {
  // For simplicity, we apply spacing once only. If you want to do multiple
  // time, please get rid of below |DCHECK()|.
  DCHECK(!is_applied_spacing_) << this;
  is_applied_spacing_ = true;
  ApplySpacingImpl(spacing, text_start_offset);
}

scoped_refptr<ShapeResult> ShapeResult::ApplySpacingToCopy(
    ShapeResultSpacing<TextRun>& spacing,
    const TextRun& run) const {
  unsigned index_of_sub_run = spacing.Text().IndexOfSubRun(run);
  DCHECK_NE(std::numeric_limits<unsigned>::max(), index_of_sub_run);
  scoped_refptr<ShapeResult> result = ShapeResult::Create(*this);
  if (index_of_sub_run != std::numeric_limits<unsigned>::max())
    result->ApplySpacingImpl(spacing, index_of_sub_run);
  return result;
}

namespace {

float HarfBuzzPositionToFloat(hb_position_t value) {
  return static_cast<float>(value) / (1 << 16);
}

// Checks whether it's safe to break without reshaping before the given glyph.
bool IsSafeToBreakBefore(const hb_glyph_info_t* glyph_infos,
                         unsigned i,
                         unsigned num_glyph,
                         TextDirection direction) {
  if (direction == TextDirection::kLtr) {
    // Before the first glyph is safe to break.
    if (!i)
      return true;

    // Not at a cluster boundary.
    if (glyph_infos[i].cluster == glyph_infos[i - 1].cluster)
      return false;
  } else {
    DCHECK_EQ(direction, TextDirection::kRtl);
    // Before the first glyph is safe to break.
    if (i == num_glyph - 1)
      return true;

    // Not at a cluster boundary.
    if (glyph_infos[i].cluster == glyph_infos[i + 1].cluster)
      return false;
  }

  // The HB_GLYPH_FLAG_UNSAFE_TO_BREAK flag is set for all glyphs in a
  // given cluster so we only need to check the last one.
  hb_glyph_flags_t flags = hb_glyph_info_get_glyph_flags(glyph_infos + i);
  return (flags & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;
}

}  // anonymous namespace

// This function computes the number of glyphs and characters that can fit into
// this RunInfo.
//
// HarfBuzzRunGlyphData has a limit kMaxCharacterIndex for the character index
// in order to packsave memory. Also, RunInfo has kMaxGlyphs to make the number
// of glyphs predictable and to minimize the buffer reallocations.
void ShapeResult::RunInfo::LimitNumGlyphs(unsigned start_glyph,
                                          unsigned* num_glyphs_in_out,
                                          unsigned* num_glyphs_removed_out,
                                          const bool is_ltr,
                                          const hb_glyph_info_t* glyph_infos) {
  unsigned num_glyphs = *num_glyphs_in_out;
  CHECK_GT(num_glyphs, 0u);

  // If there were larger character indexes than kMaxCharacterIndex, reduce
  // num_glyphs so that all character indexes can fit to kMaxCharacterIndex.
  // Because code points and glyphs are not always 1:1, we need to check the
  // first and the last cluster.
  const hb_glyph_info_t* left_glyph_info = &glyph_infos[start_glyph];
  const hb_glyph_info_t* right_glyph_info = &left_glyph_info[num_glyphs - 1];
  unsigned start_cluster;
  if (is_ltr) {
    start_cluster = left_glyph_info->cluster;
    unsigned last_cluster = right_glyph_info->cluster;
    unsigned max_cluster =
        start_cluster + HarfBuzzRunGlyphData::kMaxCharacterIndex;
    if (UNLIKELY(last_cluster > max_cluster)) {
      // Limit at |max_cluster| in LTR. If |max_cluster| is 100:
      //   0 1 2 ... 98 99 99 101 101 103 ...
      //                     ^ limit here.
      // Find |glyph_info| where |cluster| <= |max_cluster|.
      const hb_glyph_info_t* limit_glyph_info = std::upper_bound(
          left_glyph_info, right_glyph_info + 1, max_cluster,
          [](unsigned cluster, const hb_glyph_info_t& glyph_info) {
            return cluster < glyph_info.cluster;
          });
      --limit_glyph_info;
      CHECK_GT(limit_glyph_info, left_glyph_info);
      CHECK_LT(limit_glyph_info, right_glyph_info);
      DCHECK_LE(limit_glyph_info->cluster, max_cluster);
      // Adjust |right_glyph_info| and recompute dependent variables.
      right_glyph_info = limit_glyph_info;
      num_glyphs =
          base::checked_cast<unsigned>(right_glyph_info - left_glyph_info + 1);
      num_characters_ = right_glyph_info[1].cluster - start_cluster;
    }
  } else {
    start_cluster = right_glyph_info->cluster;
    unsigned last_cluster = left_glyph_info->cluster;
    unsigned max_cluster =
        start_cluster + HarfBuzzRunGlyphData::kMaxCharacterIndex;
    if (UNLIKELY(last_cluster > max_cluster)) {
      // Limit the right edge, which is in the reverse order in RTL.
      // If |min_cluster| is 3:
      //   103 102 ... 4 4 2 2 ...
      //                  ^ limit here.
      // Find |glyph_info| where |cluster| >= |min_cluster|.
      unsigned min_cluster =
          last_cluster - HarfBuzzRunGlyphData::kMaxCharacterIndex;
      DCHECK_LT(start_cluster, min_cluster);
      const hb_glyph_info_t* limit_glyph_info = std::upper_bound(
          left_glyph_info, right_glyph_info + 1, min_cluster,
          [](unsigned cluster, const hb_glyph_info_t& glyph_info) {
            return cluster > glyph_info.cluster;
          });
      --limit_glyph_info;
      CHECK_GT(limit_glyph_info, left_glyph_info);
      CHECK_LT(limit_glyph_info, right_glyph_info);
      DCHECK_GE(limit_glyph_info->cluster, min_cluster);
      // Adjust |right_glyph_info| and recompute dependent variables.
      right_glyph_info = limit_glyph_info;
      start_cluster = right_glyph_info->cluster;
      num_glyphs =
          base::checked_cast<unsigned>(right_glyph_info - left_glyph_info + 1);
      start_index_ = start_cluster;
      num_characters_ = last_cluster - right_glyph_info[1].cluster;
    }
  }

  // num_glyphs maybe still larger than kMaxGlyphs after it was reduced to fit
  // to kMaxCharacterIndex. Reduce to kMaxGlyphs if so.
  *num_glyphs_removed_out = 0;
  if (UNLIKELY(num_glyphs > HarfBuzzRunGlyphData::kMaxGlyphs)) {
    const unsigned old_num_glyphs = num_glyphs;
    num_glyphs = HarfBuzzRunGlyphData::kMaxGlyphs;

    // If kMaxGlyphs is not a cluster boundary, reduce further until the last
    // boundary.
    const unsigned end_cluster = glyph_infos[start_glyph + num_glyphs].cluster;
    for (; num_glyphs; num_glyphs--) {
      if (glyph_infos[start_glyph + num_glyphs - 1].cluster != end_cluster)
        break;
    }

    if (!num_glyphs) {
      // Extreme edge case when kMaxGlyphs is one grapheme cluster. We don't
      // have much choices, just cut at kMaxGlyphs.
      num_glyphs = HarfBuzzRunGlyphData::kMaxGlyphs;
      *num_glyphs_removed_out = old_num_glyphs - num_glyphs;
    } else if (is_ltr) {
      num_characters_ = end_cluster - start_cluster;
      DCHECK(num_characters_);
    } else {
      num_characters_ = glyph_infos[start_glyph].cluster - end_cluster;
      // Cutting the right end glyphs means cutting the start characters.
      start_index_ = glyph_infos[start_glyph + num_glyphs - 1].cluster;
      DCHECK(num_characters_);
    }
  }
  DCHECK_LE(num_glyphs, HarfBuzzRunGlyphData::kMaxGlyphs);

  if (num_glyphs == *num_glyphs_in_out)
    return;
  glyph_data_.Shrink(num_glyphs);
  *num_glyphs_in_out = num_glyphs;
}

// Computes glyph positions, sets advance and offset of each glyph to RunInfo.
template <bool is_horizontal_run>
void ShapeResult::ComputeGlyphPositions(ShapeResult::RunInfo* run,
                                        unsigned start_glyph,
                                        unsigned num_glyphs,
                                        hb_buffer_t* harfbuzz_buffer) {
  DCHECK_EQ(is_horizontal_run, run->IsHorizontal());
  const unsigned start_cluster = run->StartIndex();
  const hb_glyph_info_t* glyph_infos =
      hb_buffer_get_glyph_infos(harfbuzz_buffer, nullptr);
  const hb_glyph_position_t* glyph_positions =
      hb_buffer_get_glyph_positions(harfbuzz_buffer, nullptr);

  DCHECK_LE(num_glyphs, HarfBuzzRunGlyphData::kMaxGlyphs);

  // Compute glyph_origin in physical, since offsets of glyphs are in physical.
  // It's the caller's responsibility to convert to logical.
  float total_advance = 0.0f;
  bool has_vertical_offsets = !is_horizontal_run;

  // HarfBuzz returns result in visual order, no need to flip for RTL.
  for (unsigned i = 0; i < num_glyphs; ++i) {
    const hb_glyph_info_t glyph = glyph_infos[start_glyph + i];
    const hb_glyph_position_t& pos = glyph_positions[start_glyph + i];

    // Offset is primarily used when painting glyphs. Keep it in physical.
    GlyphOffset offset(HarfBuzzPositionToFloat(pos.x_offset),
                       -HarfBuzzPositionToFloat(pos.y_offset));

    // One out of x_advance and y_advance is zero, depending on
    // whether the buffer direction is horizontal or vertical.
    // Convert to float and negate to avoid integer-overflow for ULONG_MAX.
    float advance = is_horizontal_run ? HarfBuzzPositionToFloat(pos.x_advance)
                                      : -HarfBuzzPositionToFloat(pos.y_advance);

    DCHECK_GE(glyph.cluster, start_cluster);
    const uint16_t character_index = glyph.cluster - start_cluster;
    DCHECK_LE(character_index, HarfBuzzRunGlyphData::kMaxCharacterIndex);
    DCHECK_LT(character_index, run->num_characters_);
    run->glyph_data_[i] = {glyph.codepoint, character_index,
                           IsSafeToBreakBefore(glyph_infos + start_glyph, i,
                                               num_glyphs, Direction()),
                           advance};
    run->glyph_data_.SetOffsetAt(i, offset);

    total_advance += advance;
    has_vertical_offsets |= (offset.y() != 0);
  }

  run->width_ = std::max(0.0f, total_advance);
  has_vertical_offsets_ |= has_vertical_offsets;
  run->CheckConsistency();
}

void ShapeResult::InsertRun(scoped_refptr<ShapeResult::RunInfo> run_to_insert,
                            unsigned start_glyph,
                            unsigned num_glyphs,
                            unsigned* next_start_glyph,
                            hb_buffer_t* harfbuzz_buffer) {
  DCHECK_GT(num_glyphs, 0u);
  scoped_refptr<ShapeResult::RunInfo> run(std::move(run_to_insert));

  const hb_glyph_info_t* glyph_infos =
      hb_buffer_get_glyph_infos(harfbuzz_buffer, nullptr);
  const bool is_ltr =
      HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfbuzz_buffer));
  // num_glyphs_removed will be non-zero if the first grapheme cluster of |run|
  // is too big to fit in a single run, in which case it is truncated and the
  // truncated glyphs won't be inserted into any run.
  unsigned num_glyphs_removed = 0;
  run->LimitNumGlyphs(start_glyph, &num_glyphs, &num_glyphs_removed, is_ltr,
                      glyph_infos);
  *next_start_glyph = start_glyph + run->NumGlyphs() + num_glyphs_removed;

  if (run->IsHorizontal()) {
    // Inserting a horizontal run into a horizontal or vertical result.
    ComputeGlyphPositions<true>(run.get(), start_glyph, num_glyphs,
                                harfbuzz_buffer);
  } else {
    // Inserting a vertical run to a vertical result.
    ComputeGlyphPositions<false>(run.get(), start_glyph, num_glyphs,
                                 harfbuzz_buffer);
  }
  width_ += run->width_;
  num_glyphs_ += run->NumGlyphs();
  DCHECK_GE(num_glyphs_, run->NumGlyphs());

  InsertRun(std::move(run));
}

void ShapeResult::InsertRun(scoped_refptr<ShapeResult::RunInfo> run) {
  // The runs are stored in result->m_runs in visual order. For LTR, we place
  // the run to be inserted before the next run with a bigger character start
  // index.
  const auto ltr_comparer = [](scoped_refptr<RunInfo>& run,
                               unsigned start_index) {
    return run->start_index_ < start_index;
  };

  // For RTL, we place the run before the next run with a lower character
  // index. Otherwise, for both directions, at the end.
  const auto rtl_comparer = [](scoped_refptr<RunInfo>& run,
                               unsigned start_index) {
    return run->start_index_ > start_index;
  };

  Vector<scoped_refptr<RunInfo>>::iterator iterator = std::lower_bound(
      runs_.begin(), runs_.end(), run->start_index_,
      HB_DIRECTION_IS_FORWARD(run->direction_) ? ltr_comparer : rtl_comparer);
  if (iterator != runs_.end()) {
    runs_.insert(static_cast<wtf_size_t>(iterator - runs_.begin()),
                 std::move(run));
  }

  // If we didn't find an existing slot to place it, append.
  if (run)
    runs_.push_back(std::move(run));
}

ShapeResult::RunInfo* ShapeResult::InsertRunForTesting(
    unsigned start_index,
    unsigned num_characters,
    TextDirection direction,
    Vector<uint16_t> safe_break_offsets) {
  auto run = RunInfo::Create(
      nullptr, blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL,
      CanvasRotationInVertical::kRegular, HB_SCRIPT_COMMON, start_index,
      num_characters, num_characters);
  for (unsigned i = 0; i < run->glyph_data_.size(); i++) {
    run->glyph_data_[i] = {0, i, false, 0};
  }
  for (uint16_t offset : safe_break_offsets)
    run->glyph_data_[offset].safe_to_break_before = true;
  // RTL runs have glyphs in the descending order of character_index.
  if (IsRtl())
    run->glyph_data_.Reverse();
  num_glyphs_ += run->NumGlyphs();
  RunInfo* run_ptr = run.get();
  InsertRun(std::move(run));
  return run_ptr;
}

// Moves runs at (run_size_before, end) to the front of |runs_|.
//
// Runs in RTL result are in visual order, and that new runs should be
// prepended. This function adjusts the run order after runs were appended.
void ShapeResult::ReorderRtlRuns(unsigned run_size_before) {
  DCHECK(IsRtl());
  DCHECK_GT(runs_.size(), run_size_before);
  if (runs_.size() == run_size_before + 1) {
    if (!run_size_before)
      return;
    scoped_refptr<RunInfo> new_run(std::move(runs_.back()));
    runs_.Shrink(runs_.size() - 1);
    runs_.push_front(std::move(new_run));
    return;
  }

  // |push_front| is O(n) that we should not call it multiple times.
  // Create a new list in the correct order and swap it.
  Vector<scoped_refptr<RunInfo>> new_runs;
  new_runs.ReserveInitialCapacity(runs_.size());
  for (unsigned i = run_size_before; i < runs_.size(); i++)
    new_runs.push_back(std::move(runs_[i]));

  // Then append existing runs.
  for (unsigned i = 0; i < run_size_before; i++)
    new_runs.push_back(std::move(runs_[i]));
  runs_.swap(new_runs);
}

void ShapeResult::CopyRange(unsigned start_offset,
                            unsigned end_offset,
                            ShapeResult* target) const {
  unsigned run_index = 0;
  CopyRangeInternal(run_index, start_offset, end_offset, target);
}

void ShapeResult::CopyRanges(const ShapeRange* ranges,
                             unsigned num_ranges) const {
  DCHECK_GT(num_ranges, 0u);

  // Ranges are in logical order so for RTL the ranges are proccessed back to
  // front to ensure that they're in a sequential visual order with regards to
  // the runs.
  if (IsRtl()) {
    unsigned run_index = 0;
    unsigned last_range = num_ranges - 1;
    for (unsigned i = 0; i < num_ranges; i++) {
      const ShapeRange& range = ranges[last_range - i];
#if DCHECK_IS_ON()
      DCHECK_GE(range.end, range.start);
      if (i != last_range)
        DCHECK_GE(range.start, ranges[last_range - (i + 1)].end);
#endif
      run_index =
          CopyRangeInternal(run_index, range.start, range.end, range.target);
    }
    return;
  }

  unsigned run_index = 0;
  for (unsigned i = 0; i < num_ranges; i++) {
    const ShapeRange& range = ranges[i];
#if DCHECK_IS_ON()
    DCHECK_GE(range.end, range.start);
    if (i)
      DCHECK_GE(range.start, ranges[i - 1].end);
#endif
    run_index =
        CopyRangeInternal(run_index, range.start, range.end, range.target);
  }
}

unsigned ShapeResult::CopyRangeInternal(unsigned run_index,
                                        unsigned start_offset,
                                        unsigned end_offset,
                                        ShapeResult* target) const {
#if DCHECK_IS_ON()
  unsigned target_num_characters_before = target->num_characters_;
#endif

  target->is_applied_spacing_ |= is_applied_spacing_;

  // When |target| is empty, its character indexes are the specified sub range
  // of |this|. Otherwise the character indexes are renumbered to be continuous.
  //
  // Compute the diff of index and the number of characters from the source
  // ShapeResult and given offsets, because computing them from runs/parts can
  // be inaccurate when all characters in a run/part are missing.
  int index_diff;
  if (!target->num_characters_) {
    index_diff = 0;
    target->start_index_ = start_offset;
  } else {
    index_diff = target->EndIndex() - std::max(start_offset, StartIndex());
  }
  target->num_characters_ +=
      std::min(end_offset, EndIndex()) - std::max(start_offset, StartIndex());

  unsigned target_run_size_before = target->runs_.size();
  bool should_merge = !target->runs_.empty();
  for (; run_index < runs_.size(); run_index++) {
    const auto& run = runs_[run_index];
    unsigned run_start = run->start_index_;
    unsigned run_end = run_start + run->num_characters_;

    if (start_offset < run_end && end_offset > run_start) {
      unsigned start = start_offset > run_start ? start_offset - run_start : 0;
      unsigned end = std::min(end_offset, run_end) - run_start;
      DCHECK(end > start);

      if (scoped_refptr<RunInfo> sub_run = run->CreateSubRun(start, end)) {
        sub_run->start_index_ += index_diff;
        target->width_ += sub_run->width_;
        target->num_glyphs_ += sub_run->glyph_data_.size();
        if (auto merged_run =
                should_merge ? target->runs_.back()->MergeIfPossible(*sub_run)
                             : scoped_refptr<RunInfo>()) {
          target->runs_.back() = std::move(merged_run);
        } else {
          target->runs_.push_back(std::move(sub_run));
        }
      }
      should_merge = false;

      // No need to process runs after the end of the range.
      if ((IsLtr() && end_offset <= run_end) ||
          (IsRtl() && start_offset >= run_start)) {
        break;
      }
    }
  }

  if (!target->num_glyphs_) {
    return run_index;
  }

  // Runs in RTL result are in visual order, and that new runs should be
  // prepended. Reorder appended runs.
  DCHECK_EQ(IsRtl(), target->IsRtl());
  if (UNLIKELY(IsRtl() && target->runs_.size() != target_run_size_before))
    target->ReorderRtlRuns(target_run_size_before);

  target->has_vertical_offsets_ |= has_vertical_offsets_;

#if DCHECK_IS_ON()
  DCHECK_EQ(
      target->num_characters_ - target_num_characters_before,
      std::min(end_offset, EndIndex()) - std::max(start_offset, StartIndex()));
  target->CheckConsistency();
#endif

  return run_index;
}

scoped_refptr<ShapeResult> ShapeResult::SubRange(unsigned start_offset,
                                                 unsigned end_offset) const {
  scoped_refptr<ShapeResult> sub_range =
      Create(primary_font_.get(), 0, 0, Direction());
  CopyRange(start_offset, end_offset, sub_range.get());
  return sub_range;
}

scoped_refptr<ShapeResult> ShapeResult::CopyAdjustedOffset(
    unsigned start_index) const {
  scoped_refptr<ShapeResult> result = base::AdoptRef(new ShapeResult(*this));

  if (start_index > result->StartIndex()) {
    unsigned delta = start_index - result->StartIndex();
    for (auto& run : result->runs_)
      run->start_index_ += delta;
  } else {
    unsigned delta = result->StartIndex() - start_index;
    for (auto& run : result->runs_) {
      DCHECK(run->start_index_ >= delta);
      run->start_index_ -= delta;
    }
  }

  result->start_index_ = start_index;
  return result;
}

#if DCHECK_IS_ON()
void ShapeResult::CheckConsistency() const {
  if (runs_.empty()) {
    DCHECK_EQ(0u, num_characters_);
    DCHECK_EQ(0u, num_glyphs_);
    return;
  }

  for (const auto& run : runs_)
    run->CheckConsistency();

  const unsigned start_index = StartIndex();
  unsigned index = start_index;
  unsigned num_glyphs = 0;
  if (IsLtr()) {
    for (const auto& run : runs_) {
      // Characters maybe missing, but must be in increasing order.
      DCHECK_GE(run->start_index_, index);
      index = run->start_index_ + run->num_characters_;
      num_glyphs += run->glyph_data_.size();
    }
  } else {
    // RTL on Mac may not have runs for the all characters. crbug.com/774034
    index = runs_.back()->start_index_;
    for (const auto& run : base::Reversed(runs_)) {
      DCHECK_GE(run->start_index_, index);
      index = run->start_index_ + run->num_characters_;
      num_glyphs += run->glyph_data_.size();
    }
  }
  const unsigned end_index = EndIndex();
  DCHECK_LE(index, end_index);
  DCHECK_EQ(end_index - start_index, num_characters_);
  DCHECK_EQ(num_glyphs, num_glyphs_);
}
#endif

scoped_refptr<ShapeResult> ShapeResult::CreateForTabulationCharacters(
    const Font* font,
    const TextRun& text_run,
    float position_offset,
    unsigned length) {
  return CreateForTabulationCharacters(
      font, text_run.Direction(), text_run.GetTabSize(),
      text_run.XPos() + position_offset, 0, length);
}

scoped_refptr<ShapeResult> ShapeResult::CreateForTabulationCharacters(
    const Font* font,
    TextDirection direction,
    const TabSize& tab_size,
    float position,
    unsigned start_index,
    unsigned length) {
  DCHECK_GT(length, 0u);
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start_index, length, direction);
  result->num_glyphs_ = length;
  DCHECK_EQ(result->num_glyphs_, length);  // no overflow
  result->has_vertical_offsets_ =
      font_data->PlatformData().IsVerticalAnyUpright();
  // Tab characters are always LTR or RTL, not TTB, even when
  // isVerticalAnyUpright().
  hb_direction_t hb_direction =
      blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;
  // Only the advance of the first tab is affected by |position|.
  float advance = font->TabWidth(font_data, tab_size, position);
  do {
    unsigned run_length = std::min(length, HarfBuzzRunGlyphData::kMaxGlyphs);
    scoped_refptr<ShapeResult::RunInfo> run = RunInfo::Create(
        font_data, hb_direction, CanvasRotationInVertical::kRegular,
        HB_SCRIPT_COMMON, start_index, run_length, run_length);
    float start_position = position;
    for (unsigned i = 0; i < run_length; i++) {
      // 2nd and following tabs have the base width, without using |position|.
      if (i == 1)
        advance = font->TabWidth(font_data, tab_size);
      const unsigned index = blink::IsLtr(direction) ? i : length - 1 - i;
      run->glyph_data_[i] = {font_data->SpaceGlyph(), index, true, advance};
      position += advance;
    }
    run->width_ = position - start_position;
    result->width_ += run->width_;
    result->runs_.push_back(std::move(run));
    DCHECK_GE(length, run_length);
    length -= run_length;
    start_index += run_length;
  } while (length);
  return result;
}

scoped_refptr<ShapeResult> ShapeResult::CreateForSpacesInternal(
    const Font* font,
    TextDirection direction,
    unsigned start_index,
    unsigned length,
    float total_width,
    float per_glyph_width) {
  DCHECK_GT(length, 0u);
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start_index, length, direction);
  result->num_glyphs_ = length;
  DCHECK_EQ(result->num_glyphs_, length);  // no overflow
  result->has_vertical_offsets_ =
      font_data->PlatformData().IsVerticalAnyUpright();
  hb_direction_t hb_direction =
      blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;
  scoped_refptr<ShapeResult::RunInfo> run = RunInfo::Create(
      font_data, hb_direction, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, start_index, length, length);
  result->width_ = run->width_ = total_width;
  if (per_glyph_width > 0 && length != run->NumGlyphs())
    per_glyph_width = per_glyph_width * length / run->NumGlyphs();
  length = run->NumGlyphs();
  for (unsigned i = 0; i < length; i++) {
    const unsigned index = blink::IsLtr(direction) ? i : length - 1 - i;
    run->glyph_data_[i] = {font_data->SpaceGlyph(), index, true,
                           per_glyph_width > 0 ? per_glyph_width : total_width};
    total_width = 0;
  }
  result->runs_.push_back(std::move(run));
  return result;
}

scoped_refptr<ShapeResult> ShapeResult::CreateForSpaces(const Font* font,
                                                        TextDirection direction,
                                                        unsigned start_index,
                                                        unsigned length,
                                                        float width) {
  return CreateForSpacesInternal(font, direction, start_index, length, width,
                                 -1);
}

scoped_refptr<ShapeResult> ShapeResult::CreateForSpacesWithPerGlyphWidth(
    const Font* font,
    TextDirection direction,
    unsigned start_index,
    unsigned length,
    float per_glyph_width) {
  return CreateForSpacesInternal(font, direction, start_index, length,
                                 per_glyph_width * length, per_glyph_width);
}

scoped_refptr<ShapeResult> ShapeResult::CreateForStretchyMathOperator(
    const Font* font,
    TextDirection direction,
    Glyph glyph_variant,
    float stretch_size) {
  unsigned start_index = 0;
  unsigned num_characters = 1;
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start_index, num_characters, direction);

  hb_direction_t hb_direction = HB_DIRECTION_LTR;
  unsigned glyph_index = 0;
  scoped_refptr<ShapeResult::RunInfo> run = RunInfo::Create(
      font->PrimaryFont(), hb_direction, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, start_index, 1 /* num_glyph */, num_characters);
  run->glyph_data_[glyph_index] = {glyph_variant, 0 /* character index */,
                                   true /* IsSafeToBreakBefore */,
                                   stretch_size};
  run->width_ = std::max(0.0f, stretch_size);

  result->width_ = run->width_;
  result->num_glyphs_ = run->NumGlyphs();
  result->runs_.push_back(std::move(run));

  return result;
}

scoped_refptr<ShapeResult> ShapeResult::CreateForStretchyMathOperator(
    const Font* font,
    TextDirection direction,
    OpenTypeMathStretchData::StretchAxis stretch_axis,
    const OpenTypeMathStretchData::AssemblyParameters& assembly_parameters) {
  DCHECK(!assembly_parameters.parts.empty());
  DCHECK_LE(assembly_parameters.glyph_count, HarfBuzzRunGlyphData::kMaxGlyphs);

  bool is_horizontal_assembly =
      stretch_axis == OpenTypeMathStretchData::StretchAxis::Horizontal;
  unsigned start_index = 0;
  unsigned num_characters = 1;
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(font, start_index, num_characters, direction);

  hb_direction_t hb_direction =
      is_horizontal_assembly ? HB_DIRECTION_LTR : HB_DIRECTION_TTB;
  scoped_refptr<ShapeResult::RunInfo> run = RunInfo::Create(
      font->PrimaryFont(), hb_direction, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, start_index, assembly_parameters.glyph_count,
      num_characters);

  float overlap = assembly_parameters.connector_overlap;
  unsigned part_index = 0;
  for (const auto& part : assembly_parameters.parts) {
    unsigned repetition_count =
        part.is_extender ? assembly_parameters.repetition_count : 1;
    if (!repetition_count)
      continue;
    DCHECK(part_index < assembly_parameters.glyph_count);
    for (unsigned repetition_index = 0; repetition_index < repetition_count;
         repetition_index++) {
      unsigned glyph_index =
          is_horizontal_assembly
              ? part_index
              : assembly_parameters.glyph_count - 1 - part_index;
      float full_advance = glyph_index == assembly_parameters.glyph_count - 1
                               ? part.full_advance
                               : part.full_advance - overlap;
      run->glyph_data_[glyph_index] = {part.glyph, 0 /* character index */,
                                       !glyph_index /* IsSafeToBreakBefore */,
                                       full_advance};
      if (!is_horizontal_assembly) {
        GlyphOffset glyph_offset(
            0, -assembly_parameters.stretch_size + part.full_advance);
        run->glyph_data_.SetOffsetAt(glyph_index, glyph_offset);
        result->has_vertical_offsets_ |= (glyph_offset.y() != 0);
      }
      part_index++;
    }
  }
  run->width_ = std::max(0.0f, assembly_parameters.stretch_size);

  result->width_ = run->width_;
  result->num_glyphs_ = run->NumGlyphs();
  result->runs_.push_back(std::move(run));
  return result;
}

void ShapeResult::ToString(StringBuilder* output) const {
  output->Append("#chars=");
  output->AppendNumber(num_characters_);
  output->Append(", #glyphs=");
  output->AppendNumber(num_glyphs_);
  output->Append(", dir=");
  output->AppendNumber(direction_);
  output->Append(", runs[");
  output->AppendNumber(runs_.size());
  output->Append("]{");
  for (unsigned run_index = 0; run_index < runs_.size(); run_index++) {
    output->AppendNumber(run_index);
    const auto& run = *runs_[run_index];
    output->Append(":{start=");
    output->AppendNumber(run.start_index_);
    output->Append(", #chars=");
    output->AppendNumber(run.num_characters_);
    output->Append(", dir=");
    output->AppendNumber(static_cast<uint32_t>(run.direction_));
    output->Append(", glyphs[");
    output->AppendNumber(run.glyph_data_.size());
    output->Append("]{");
    for (unsigned glyph_index = 0; glyph_index < run.glyph_data_.size();
         glyph_index++) {
      output->AppendNumber(glyph_index);
      const auto& glyph_data = run.glyph_data_[glyph_index];
      output->Append(":{char=");
      output->AppendNumber(glyph_data.character_index);
      output->Append(", glyph=");
      output->AppendNumber(glyph_data.glyph);
      output->Append("}");
    }
    output->Append("}}");
  }
  output->Append("}");
}

String ShapeResult::ToString() const {
  StringBuilder output;
  ToString(&output);
  return output.ToString();
}

std::ostream& operator<<(std::ostream& ostream,
                         const ShapeResult& shape_result) {
  return ostream << shape_result.ToString();
}

template <bool rtl>
void ShapeResult::ComputePositionData() const {
  auto& data = character_position_->data_;
  unsigned start_offset = StartIndex();
  unsigned next_character_index = 0;
  float run_advance = 0;
  float last_x_position = 0;

  // Iterate runs/glyphs in the visual order; i.e., from the left edge
  // regardless of the directionality, so that |x_position| is always in
  // ascending order.
  // TODO(kojii): It does not work when large negative letter-/word-
  // spacing is applied.
  for (const auto& run : runs_) {
    if (!run)
      continue;

    // Assumes all runs have the same directionality as the ShapeResult so that
    // |x_position| is in ascending order.
    DCHECK_EQ(IsRtl(), run->IsRtl());

    float total_advance = run_advance;
    for (const auto& glyph_data : run->glyph_data_) {
      DCHECK_GE(run->start_index_, start_offset);
      const unsigned logical_index =
          run->start_index_ + glyph_data.character_index - start_offset;

      // Make |character_index| to the visual offset.
      DCHECK_LT(logical_index, num_characters_);
      const unsigned character_index =
          rtl ? num_characters_ - logical_index - 1 : logical_index;

      // If this glyph is the first glyph of a new cluster, set the data.
      // Otherwise, |data[character_index]| is already set. Do not overwrite.
      if (character_index >= num_characters_) {
        // We are not sure why we reach here. See http://crbug.com/1286882
        NOTREACHED();
        continue;
      }
      if (next_character_index <= character_index) {
        if (next_character_index < character_index) {
          // Multiple glyphs may have the same character index and not all
          // character indices may have glyphs. For character indices without
          // glyphs set the x-position to that of the nearest preceding glyph in
          // the logical order; i.e., the last position for LTR or this position
          // for RTL.
          float x_position = !rtl ? last_x_position : total_advance;
          for (unsigned i = next_character_index; i < character_index; i++) {
            DCHECK_LT(i, num_characters_);
            data[i] = {x_position, false, false};
          }
        }

        data[character_index] = {total_advance, true,
                                 glyph_data.safe_to_break_before};
        last_x_position = total_advance;
      }

      total_advance += glyph_data.advance;
      next_character_index = character_index + 1;
    }
    run_advance += run->width_;
  }

  // Fill |x_position| for the rest of characters, when they don't have
  // corresponding glyphs.
  if (next_character_index < num_characters_) {
    float x_position = !rtl ? last_x_position : run_advance;
    for (unsigned i = next_character_index; i < num_characters_; i++) {
      data[i] = {x_position, false, false};
    }
  }

  character_position_->start_offset_ = start_offset;
}

void ShapeResult::EnsurePositionData() const {
  if (character_position_)
    return;

  character_position_ =
      std::make_unique<CharacterPositionData>(num_characters_, width_);
  if (Direction() == TextDirection::kLtr)
    ComputePositionData<false>();
  else
    ComputePositionData<true>();
}

void ShapeResult::DiscardPositionData() const {
  character_position_ = nullptr;
}

unsigned ShapeResult::CachedOffsetForPosition(float x) const {
  DCHECK(character_position_);
  unsigned offset = character_position_->OffsetForPosition(x, IsRtl());
#if 0
  // TODO(kojii): This DCHECK fails in ~10 tests. Needs investigations.
  DCHECK_EQ(OffsetForPosition(x, BreakGlyphsOption::DontBreakGlyphs), offset) << x;
#endif
  return offset;
}

float ShapeResult::CachedPositionForOffset(unsigned offset) const {
  DCHECK_GE(offset, 0u);
  DCHECK_LE(offset, num_characters_);
  DCHECK(character_position_);
  float position = character_position_->PositionForOffset(offset, IsRtl());
#if 0
  // TODO(kojii): This DCHECK fails in several tests. Needs investigations.
  DCHECK_EQ(PositionForOffset(offset), position) << offset;
#endif
  return position;
}

unsigned ShapeResult::CachedNextSafeToBreakOffset(unsigned offset) const {
  if (IsRtl())
    return NextSafeToBreakOffset(offset);

  DCHECK(character_position_);
  return character_position_->NextSafeToBreakOffset(offset);
}

unsigned ShapeResult::CachedPreviousSafeToBreakOffset(unsigned offset) const {
  if (IsRtl())
    return PreviousSafeToBreakOffset(offset);

  DCHECK(character_position_);
  return character_position_->PreviousSafeToBreakOffset(offset);
}

// TODO(eae): Might be worth trying to set midpoint to ~50% more than the number
// of characters in the previous line for the first try. Would cut the number
// of tries in the majority of cases for long strings.
unsigned ShapeResult::CharacterPositionData::OffsetForPosition(float x,
                                                               bool rtl) const {
  // At or before start, return offset *of* the first character.
  // At or beyond the end, return offset *after* the last character.
  if (x <= 0)
    return !rtl ? 0 : data_.size();
  if (x >= width_)
    return !rtl ? data_.size() : 0;

  // Do a binary search to find the largest x-position that is less than or
  // equal to the supplied x value.
  unsigned length = data_.size();
  unsigned low = 0;
  unsigned high = length - 1;
  while (low <= high) {
    unsigned midpoint = low + (high - low) / 2;
    if (data_[midpoint].x_position <= x &&
        (midpoint + 1 == length || data_[midpoint + 1].x_position > x)) {
      if (!rtl)
        return midpoint;
      // The border belongs to the logical next character.
      return data_[midpoint].x_position == x ? data_.size() - midpoint
                                             : data_.size() - midpoint - 1;
    }
    if (x < data_[midpoint].x_position)
      high = midpoint - 1;
    else
      low = midpoint + 1;
  }

  return 0;
}

float ShapeResult::CharacterPositionData::PositionForOffset(unsigned offset,
                                                            bool rtl) const {
  DCHECK_GT(data_.size(), 0u);
  if (!rtl) {
    if (offset < data_.size())
      return data_[offset].x_position;
  } else {
    if (offset >= data_.size())
      return 0;
    // Return the left edge of the next character because in RTL, the position
    // is the right edge of the character.
    for (unsigned visual_offset = data_.size() - offset - 1;
         visual_offset < data_.size(); visual_offset++) {
      if (data_[visual_offset].is_cluster_base) {
        return visual_offset + 1 < data_.size()
                   ? data_[visual_offset + 1].x_position
                   : width_;
      }
    }
  }
  return width_;
}

unsigned ShapeResult::CharacterPositionData::NextSafeToBreakOffset(
    unsigned offset) const {
  DCHECK_LE(start_offset_, offset);
  unsigned adjusted_offset = offset - start_offset_;
  DCHECK_LT(adjusted_offset, data_.size());

  // Assume it is always safe to break at the start. While not strictly correct
  // the text has already been segmented at that offset. This also matches the
  // non-CharacterPositionData implementation.
  if (adjusted_offset == 0)
    return start_offset_;

  unsigned length = data_.size();
  for (unsigned i = adjusted_offset; i < length; i++) {
    if (data_[i].safe_to_break_before)
      return start_offset_ + i;
  }

  // Next safe break is at the end of the run.
  return start_offset_ + length;
}

unsigned ShapeResult::CharacterPositionData::PreviousSafeToBreakOffset(
    unsigned offset) const {
  DCHECK_LE(start_offset_, offset);
  unsigned adjusted_offset = offset - start_offset_;
  DCHECK_LT(adjusted_offset, data_.size());

  // Assume it is always safe to break at the end of the run.
  if (adjusted_offset >= data_.size())
    return start_offset_ + data_.size();

  for (unsigned i = adjusted_offset + 1; i > 0; i--) {
    if (data_[i - 1].safe_to_break_before)
      return start_offset_ + (i - 1);
  }

  // Previous safe break is at the start of the run.
  return 0;
}

namespace {

void AddRunInfoRanges(const ShapeResult::RunInfo& run_info,
                      float offset,
                      Vector<CharacterRange>* ranges) {
  Vector<float> character_widths(run_info.num_characters_);
  for (const auto& glyph : run_info.glyph_data_) {
    // TODO(crbug.com/1147011): This should not happen, but crash logs indicate
    // that this is happening.
    if (UNLIKELY(glyph.character_index >= character_widths.size())) {
      NOTREACHED();
      character_widths.Grow(glyph.character_index + 1);
    }
    character_widths[glyph.character_index] += glyph.advance;
  }

  if (run_info.IsRtl())
    offset += run_info.width_;

  for (unsigned character_index = 0; character_index < character_widths.size();
       character_index++) {
    float start = offset;
    offset += character_widths[character_index] * (run_info.IsRtl() ? -1 : 1);
    float end = offset;

    // To match getCharacterRange we flip ranges to ensure start <= end.
    if (end < start)
      ranges->push_back(CharacterRange(end, start, 0, 0));
    else
      ranges->push_back(CharacterRange(start, end, 0, 0));
  }
}

}  // anonymous namespace

float ShapeResult::IndividualCharacterRanges(Vector<CharacterRange>* ranges,
                                             float start_x) const {
  DCHECK(ranges);
  float current_x = start_x;

  if (IsRtl()) {
    unsigned run_count = runs_.size();
    for (int index = run_count - 1; index >= 0; index--) {
      current_x -= runs_[index]->width_;
      AddRunInfoRanges(*runs_[index], current_x, ranges);
    }
  } else {
    for (const auto& run : runs_) {
      AddRunInfoRanges(*run, current_x, ranges);
      current_x += run->width_;
    }
  }

  return current_x;
}

template <bool is_horizontal_run, bool has_non_zero_glyph_offsets>
void ShapeResult::ComputeRunInkBounds(const ShapeResult::RunInfo& run,
                                      float run_advance,
                                      gfx::RectF* ink_bounds) const {
  // Get glyph bounds from Skia. It's a lot faster if we give it list of glyph
  // IDs rather than calling it for each glyph.
  // TODO(kojii): MacOS does not benefit from batching the Skia request due to
  // https://bugs.chromium.org/p/skia/issues/detail?id=5328, and the cost to
  // prepare batching, which is normally much less than the benefit of
  // batching, is not ignorable unfortunately.
  auto glyph_offsets = run.glyph_data_.GetOffsets<has_non_zero_glyph_offsets>();
  const SimpleFontData& current_font_data = *run.font_data_;
  unsigned num_glyphs = run.glyph_data_.size();
#if !BUILDFLAG(IS_MAC)
  Vector<Glyph, 256> glyphs(num_glyphs);
  unsigned i = 0;
  for (const auto& glyph_data : run.glyph_data_)
    glyphs[i++] = glyph_data.glyph;
  Vector<SkRect, 256> bounds_list(num_glyphs);
  current_font_data.BoundsForGlyphs(glyphs, &bounds_list);
#endif

  GlyphBoundsAccumulator bounds(run_advance);
  for (unsigned j = 0; j < num_glyphs; ++j) {
    const HarfBuzzRunGlyphData& glyph_data = run.glyph_data_[j];
#if BUILDFLAG(IS_MAC)
    gfx::RectF glyph_bounds =
        current_font_data.BoundsForGlyph(glyph_data.glyph);
#else
    gfx::RectF glyph_bounds = gfx::SkRectToRectF(bounds_list[j]);
#endif
    bounds.Unite<is_horizontal_run>(glyph_bounds, *glyph_offsets);
    ++glyph_offsets;
    bounds.origin += glyph_data.advance;
  }

  if (!is_horizontal_run)
    bounds.ConvertVerticalRunToLogical(current_font_data.GetFontMetrics());
  ink_bounds->Union(bounds.bounds);
}

gfx::RectF ShapeResult::ComputeInkBounds() const {
  gfx::RectF ink_bounds;
  float run_advance = 0.0f;
  for (const auto& run : runs_) {
    if (run->glyph_data_.HasNonZeroOffsets()) {
      if (run->IsHorizontal())
        ComputeRunInkBounds<true, true>(*run.get(), run_advance, &ink_bounds);
      else
        ComputeRunInkBounds<false, true>(*run.get(), run_advance, &ink_bounds);
    } else {
      if (run->IsHorizontal())
        ComputeRunInkBounds<true, false>(*run.get(), run_advance, &ink_bounds);
      else
        ComputeRunInkBounds<false, false>(*run.get(), run_advance, &ink_bounds);
    }
    run_advance += run->width_;
  }

  return ink_bounds;
}

}  // namespace blink
