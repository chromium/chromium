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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "third_party/blink/renderer/platform/fonts/shaping/text_auto_space.h"
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

struct SameSizeAsRunInfo {
  struct GlyphDataCollection {
    void* pointers[2];
    unsigned integer;
  } glyph_data;
  void* pointer;
  Vector<int> vector;
  int integers[6];
};

ASSERT_SIZE(ShapeResult::RunInfo, SameSizeAsRunInfo);

struct SameSizeAsShapeResult {
  float width;
  UntracedMember<void*> deprecated_ink_bounds_;
  Vector<int> runs_;
  Vector<int> character_position_;
  UntracedMember<void*> primary_font_;
  unsigned start_index_;
  unsigned num_characters_;
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
  for (const Member<RunInfo>& run : runs_) {
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
  InlineLayoutUnit glyph_sequence_advance;
  // the accumulated advance up to the current glyph sequence.
  InlineLayoutUnit accumulated_position;

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

  // Determine if the offset is at the beginning of the current glyph sequence.
  bool is_offset_at_glyph_sequence_start = (offset == glyph_sequence_start);

  // We calculate the number of Unicode grapheme clusters (actually cursor
  // position stops) on the subset of characters. We use this to divide
  // glyph_sequence_advance by the number of unicode grapheme clusters this
  // glyph sequence was shaped for, and thus linearly interpolate the cursor
  // position based on accumulated position and a fraction of
  // glyph_sequence_advance.
  unsigned graphemes = NumGraphemes(glyph_sequence_start, glyph_sequence_end);
  if (graphemes > 1) {
    DCHECK_GE(glyph_sequence_end, glyph_sequence_start);
    unsigned next_offset = offset + (offset == num_characters_ ? 0 : 1);
    unsigned num_graphemes_to_offset =
        NumGraphemes(glyph_sequence_start, next_offset) - 1;
    // |is_offset_at_glyph_sequence_start| bool variable above does not take
    // into account the case of broken glyphs (with multi graphemes) scenarios,
    // so make amend here. Check if the offset is at the beginning of the
    // specific grapheme cluster in the broken glyphs.
    if (offset > 0) {
      is_offset_at_glyph_sequence_start =
          (NumGraphemes(offset - 1, next_offset) != 1);
    }
    glyph_sequence_advance = glyph_sequence_advance / graphemes;
    const unsigned num_graphemes_from_left =
        IsLtr() ? num_graphemes_to_offset
                : graphemes - num_graphemes_to_offset - 1;
    accumulated_position += glyph_sequence_advance * num_graphemes_from_left;
  }

  // Re-adapt based on adjust_mid_cluster. On LTR, if we want AdjustToEnd and
  // offset is not at the beginning, we need to jump to the right side of the
  // grapheme. On RTL, if we want AdjustToStart and offset is not at the end, we
  // need to jump to the left side of the grapheme.
  if (IsLtr() && adjust_mid_cluster == AdjustMidCluster::kToEnd &&
      !is_offset_at_glyph_sequence_start) {
    accumulated_position += glyph_sequence_advance;
  } else if (IsRtl() && adjust_mid_cluster == AdjustMidCluster::kToEnd &&
             !is_offset_at_glyph_sequence_start) {
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
      result->advance += glyph_data.advance.ToFloat();
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

ShapeResult::ShapeResult(const SimpleFontData* font_data,
                         unsigned start_index,
                         unsigned num_characters,
                         TextDirection direction)
    : primary_font_(font_data),
      start_index_(start_index),
      num_characters_(num_characters),
      direction_(static_cast<unsigned>(direction)) {}

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
  runs_.ReserveInitialCapacity(other.runs_.size());
  for (const auto& run : other.runs_)
    runs_.push_back(MakeGarbageCollected<RunInfo>(*run));
}

ShapeResult::~ShapeResult() = default;

void ShapeResult::Trace(Visitor* visitor) const {
  visitor->Trace(deprecated_ink_bounds_);
  visitor->Trace(runs_);
  visitor->Trace(character_position_);
  visitor->Trace(primary_font_);
}

size_t ShapeResult::ByteSize() const {
  size_t self_byte_size = sizeof(*this);
  for (unsigned i = 0; i < runs_.size(); ++i) {
    self_byte_size += runs_[i]->ByteSize();
  }
  return self_byte_size;
}

const ShapeResultCharacterData& ShapeResult::CharacterData(
    unsigned offset) const {
  DCHECK_GE(offset, StartIndex());
  DCHECK_LT(offset, EndIndex());
  DCHECK(!character_position_.empty());
  return character_position_[offset - StartIndex()];
}

ShapeResultCharacterData& ShapeResult::CharacterData(unsigned offset) {
  DCHECK_GE(offset, StartIndex());
  DCHECK_LT(offset, EndIndex());
  DCHECK(!character_position_.empty());
  return character_position_[offset - StartIndex()];
}

bool ShapeResult::IsStartSafeToBreak() const {
  // Empty is likely a |SubRange| at the middle of a cluster or a ligature.
  if (runs_.empty()) [[unlikely]] {
    return false;
  }
  const RunInfo* run = nullptr;
  const HarfBuzzRunGlyphData* glyph_data = nullptr;
  if (IsLtr()) {
    run = runs_.front().Get();
    glyph_data = &run->glyph_data_.front();
  } else {
    run = runs_.back().Get();
    glyph_data = &run->glyph_data_.back();
  }
  return glyph_data->safe_to_break_before &&
         // If the glyph for the first character is missing, consider not safe.
         StartIndex() == run->start_index_ + glyph_data->character_index;
}

unsigned ShapeResult::NextSafeToBreakOffset(unsigned index) const {
  for (auto it = runs_.begin(); it != runs_.end(); ++it) {
    const auto& run = *it;
    if (!run)
      continue;

    unsigned run_start = run->start_index_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset < run->num_characters_) {
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

template <typename Iterator>
void ShapeResult::AddUnsafeToBreak(Iterator offsets_iter,
                                   const Iterator offsets_end) {
  CHECK(offsets_iter != offsets_end);
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK(character_position_.empty());
  DCHECK(std::is_sorted(
      offsets_iter, offsets_end,
      IsLtr() ? [](unsigned a, unsigned b) { return a < b; }
              : [](unsigned a, unsigned b) { return a > b; }));
  DCHECK_GE(*offsets_iter, StartIndex());
#endif
  unsigned offset = *offsets_iter;
  for (const auto& run : runs_) {
    unsigned run_offset = offset - run->StartIndex();
    if (run_offset >= run->num_characters_) {
      continue;
    }
    for (HarfBuzzRunGlyphData& glyph_data : run->glyph_data_) {
      if (glyph_data.character_index == run_offset) {
        glyph_data.safe_to_break_before = false;
        if (++offsets_iter == offsets_end) {
          return;
        }
        offset = *offsets_iter;
        run_offset = offset - run->StartIndex();
        if (run_offset >= run->num_characters_) {
          break;
        }
      }
    }
  }
}

void ShapeResult::AddUnsafeToBreak(base::span<const unsigned> offsets) {
  if (IsLtr()) {
    AddUnsafeToBreak(offsets.begin(), offsets.end());
  } else {
    AddUnsafeToBreak(offsets.rbegin(), offsets.rend());
  }
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

  for (const Member<RunInfo>& run : runs_) {
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

  // The absolute_offset argument represents the offset for the entire
  // ShapeResult while offset counts down the remaining offset as runs are
  // processed.
  unsigned offset = absolute_offset;

  if (IsRtl()) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    if (offset < NumCharacters())
      offset = NumCharacters() - offset - 1;
  }

  for (unsigned i = 0; i < runs_.size(); i++) {
    if (!runs_[i])
      continue;
    DCHECK_EQ(IsRtl(), runs_[i]->IsRtl());
    unsigned num_characters = runs_[i]->num_characters_;

    if (offset < num_characters) {
      return runs_[i]->XPositionForVisualOffset(offset, adjust_mid_cluster) + x;
    }

    offset -= num_characters;
    x += runs_[i]->width_;
  }

  // The position in question might be just after the text.
  if (absolute_offset == NumCharacters()) {
    return IsRtl() ? 0 : width_;
  }

  return 0;
}

float ShapeResult::CaretPositionForOffset(
    unsigned offset,
    const StringView& text,
    AdjustMidCluster adjust_mid_cluster) const {
  EnsureGraphemes(text);
  return PositionForOffset(offset, adjust_mid_cluster);
}

bool ShapeResult::HasFallbackFonts(const SimpleFontData* primary_font) const {
  for (const Member<RunInfo>& run : runs_) {
    if (run->font_data_ != primary_font) {
      return true;
    }
  }
  return false;
}

void ShapeResult::GetRunFontData(HeapVector<RunFontData>* font_data) const {
  for (const auto& run : runs_) {
    font_data->push_back(
        RunFontData({run->font_data_.Get(), run->glyph_data_.size()}));
  }
}

template <bool has_non_zero_glyph_offsets>
float ShapeResult::ForEachGlyphImpl(float initial_advance,
                                    GlyphCallback glyph_callback,
                                    void* context,
                                    const RunInfo& run) const {
  auto glyph_offsets = run.glyph_data_.GetOffsets<has_non_zero_glyph_offsets>();
  auto total_advance = InlineLayoutUnit::FromFloatRound(initial_advance);
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run.direction_);
  for (const auto& glyph_data : run.glyph_data_) {
    glyph_callback(context, run.start_index_ + glyph_data.character_index,
                   glyph_data.glyph, *glyph_offsets, total_advance,
                   is_horizontal, run.canvas_rotation_, run.font_data_.Get());
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
  auto total_advance = InlineLayoutUnit::FromFloatRound(initial_advance);
  unsigned run_start = run.start_index_ + index_offset;
  bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run.direction_);
  const SimpleFontData* font_data = run.font_data_.Get();

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
  InlineLayoutUnit advance_so_far =
      InlineLayoutUnit::FromFloatRound(initial_advance);
  for (const auto& run : runs_) {
    unsigned graphemes_in_cluster = 1;
    InlineLayoutUnit cluster_advance;

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
        cluster_advance = InlineLayoutUnit();
      }
    }
  }
  return advance_so_far;
}

// TODO(kojii): VC2015 fails to explicit instantiation of a member function.
// Typed functions + this private function are to instantiate instances.
template <typename TextContainerType>
float ShapeResult::ApplySpacingImpl(
    ShapeResultSpacing<TextContainerType>& spacing,
    int text_start_offset) {
  float offset = 0;
  float total_advance = 0;
  TextRunLayoutUnit space;
  for (auto& run : runs_) {
    if (!run)
      continue;
    unsigned run_start_index = run->start_index_ + text_start_offset;
    InlineLayoutUnit total_advance_for_run;
    for (wtf_size_t i = 0; i < run->glyph_data_.size(); i++) {
      HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];

      // Skip if it's not a grapheme cluster boundary.
      if (i + 1 < run->glyph_data_.size() &&
          glyph_data.character_index ==
              run->glyph_data_[i + 1].character_index) {
        total_advance_for_run += glyph_data.advance;
        continue;
      }

      typename ShapeResultSpacing<TextContainerType>::ComputeSpacingParameters
          parameters{.index = run_start_index + glyph_data.character_index,
                     .original_advance = glyph_data.advance};
      space = spacing.ComputeSpacing(parameters, offset);
      glyph_data.AddAdvance(space);
      total_advance_for_run += glyph_data.advance;

      // |offset| is non-zero only when justifying CJK characters that follow
      // non-CJK characters.
      if (offset) [[unlikely]] {
        if (run->IsHorizontal()) {
          run->glyph_data_.AddOffsetWidthAt(i, offset);
        } else {
          run->glyph_data_.AddOffsetHeightAt(i, offset);
          has_vertical_offsets_ = true;
        }
        offset = 0;
      }
    }
    run->width_ = total_advance_for_run;
    total_advance += run->width_;
  }
  width_ = total_advance;
  return space;
}

float ShapeResult::ApplySpacing(ShapeResultSpacing<String>& spacing,
                                int text_start_offset) {
  // For simplicity, we apply spacing once only. If you want to do multiple
  // time, please get rid of below |DCHECK()|.
  DCHECK(!is_applied_spacing_) << this;
  is_applied_spacing_ = true;
  return ApplySpacingImpl(spacing, text_start_offset);
}

ShapeResult* ShapeResult::ApplySpacingToCopy(
    ShapeResultSpacing<TextRun>& spacing,
    const TextRun& run) const {
  unsigned index_of_sub_run = spacing.Text().IndexOfSubRun(run);
  DCHECK_NE(std::numeric_limits<unsigned>::max(), index_of_sub_run);
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(*this);
  if (index_of_sub_run != std::numeric_limits<unsigned>::max())
    result->ApplySpacingImpl(spacing, index_of_sub_run);
  return result;
}

void ShapeResult::ApplyLeadingExpansion(LayoutUnit expansion) {
  DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
  if (expansion <= LayoutUnit()) {
    return;
  }
  for (auto& run : runs_) {
    if (!run) {
      continue;
    }
    for (wtf_size_t i = 0; i < run->glyph_data_.size(); i++) {
      HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];

      // Skip if it's not a grapheme cluster boundary.
      if (i + 1 < run->glyph_data_.size() &&
          glyph_data.character_index ==
              run->glyph_data_[i + 1].character_index) {
        continue;
      }

      const TextRunLayoutUnit advance = expansion.To<TextRunLayoutUnit>();
      glyph_data.AddAdvance(advance);
      const float expansion_as_float = advance.ToFloat();
      run->width_ += expansion_as_float;
      width_ += expansion_as_float;

      if (run->IsHorizontal()) {
        run->glyph_data_.AddOffsetWidthAt(i, expansion_as_float);
      } else {
        run->glyph_data_.AddOffsetHeightAt(i, expansion_as_float);
        has_vertical_offsets_ = true;
      }
      return;
    }
  }
  // No glyphs.
  NOTREACHED_IN_MIGRATION();
}

void ShapeResult::ApplyTrailingExpansion(LayoutUnit expansion) {
  DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
  if (expansion <= LayoutUnit()) {
    return;
  }
  for (auto& run : base::Reversed(runs_)) {
    if (!run) {
      continue;
    }
    if (run->glyph_data_.IsEmpty()) {
      continue;
    }
    HarfBuzzRunGlyphData& glyph_data = run->glyph_data_.back();
    const TextRunLayoutUnit advance = expansion.To<TextRunLayoutUnit>();
    glyph_data.AddAdvance(advance);
    const float expansion_as_float = advance.ToFloat();
    run->width_ += expansion_as_float;
    width_ += expansion_as_float;
    return;
  }
  // No glyphs.
  NOTREACHED_IN_MIGRATION();
}

bool ShapeResult::HasAutoSpacingAfter(unsigned offset) const {
  if (!character_position_.empty() && offset >= StartIndex() &&
      offset < EndIndex()) {
    return CharacterData(offset).has_auto_spacing_after;
  }
  return false;
}

bool ShapeResult::HasAutoSpacingBefore(unsigned offset) const {
  return HasAutoSpacingAfter(offset - 1);
}

void ShapeResult::ApplyTextAutoSpacing(
    const Vector<OffsetWithSpacing, 16>& offsets_with_spacing) {
  // `offsets_with_spacing` must be non-empty, ascending list without the same
  // offsets.
  DCHECK(!offsets_with_spacing.empty());
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK(std::is_sorted(
      offsets_with_spacing.begin(), offsets_with_spacing.end(),
      [](const OffsetWithSpacing& lhs, const OffsetWithSpacing& rhs) {
        return lhs.offset <= rhs.offset;
      }));
  DCHECK_GE(offsets_with_spacing.front().offset, StartIndex());
  DCHECK_LE(offsets_with_spacing.back().offset, EndIndex());
#endif

  EnsurePositionData();
  if (IsLtr()) [[likely]] {
    ApplyTextAutoSpacingCore<TextDirection::kLtr>(offsets_with_spacing.begin(),
                                                  offsets_with_spacing.end());
  } else {
    ApplyTextAutoSpacingCore<TextDirection::kRtl>(offsets_with_spacing.rbegin(),
                                                  offsets_with_spacing.rend());
  }
  RecalcCharacterPositions();
}

template <TextDirection direction, class Iterator>
void ShapeResult::ApplyTextAutoSpacingCore(Iterator offset_begin,
                                           Iterator offset_end) {
  DCHECK(offset_begin != offset_end);
  Iterator current_offset = offset_begin;
  if (current_offset->offset == StartIndex()) [[unlikely]] {
    // Enter this branch if the previous item's direction is RTL and current
    // item's direction is LTR. In this case, spacing cannot be added to the
    // advance of the previous run, otherwise it might be a wrong position after
    // line break. Instead, the spacing is added to the offset of the first run.
    if (Direction() == TextDirection::kRtl) {
      // TODO(https://crbug.com/1463890): Here should be item's direction !=
      // base direction .
      current_offset++;
    } else {
      for (auto& run : runs_) {
        if (!run) [[unlikely]] {
          continue;
        }
        DCHECK_EQ(run->start_index_, current_offset->offset);
        wtf_size_t last_glyph_of_first_char = 0;
        float uni_dim_offset = current_offset->spacing;
        // It is unfortunate to set glyph_data_'s offsets, but it should be
        // super rare to reach there, so it would not hurt memory usage.
        GlyphOffset glyph_offset = run->IsHorizontal()
                                       ? GlyphOffset(uni_dim_offset, 0)
                                       : GlyphOffset(0, uni_dim_offset);
        for (wtf_size_t i = 0; i < run->NumGlyphs(); i++) {
          if (run->glyph_data_[i].character_index != 0) {
            break;
          }
          run->glyph_data_.SetOffsetAt(i, glyph_offset);
          last_glyph_of_first_char = i;
        }
        run->glyph_data_[last_glyph_of_first_char].AddAdvance(uni_dim_offset);
        has_vertical_offsets_ |= (glyph_offset.y() != 0);
        run->width_ += uni_dim_offset;
        current_offset++;
        break;
      }
    }
  }

  for (auto& run : runs_) {
    if (!run) [[unlikely]] {
      continue;
    }
    if (current_offset == offset_end) {
      break;
    }
    wtf_size_t offset = current_offset->offset;
    DCHECK_GE(offset, run->start_index_);
    wtf_size_t offset_in_run = offset - run->start_index_;
    if (offset_in_run > run->num_characters_) {
      continue;
    }

    float total_space_for_run = 0;
    for (wtf_size_t i = 0; i < run->NumGlyphs(); i++) {
      // `character_index` may repeat or skip. Add the spacing to the glyph
      // before the first one that is equal to or greater than `offset_in_run`.
      wtf_size_t next_character_index;
      if (i + 1 < run->glyph_data_.size()) {
        next_character_index = run->glyph_data_[i + 1].character_index;
      } else {
        next_character_index = run->num_characters_;
      }
      bool should_add_spacing;
      if (blink::IsLtr(direction)) {
        // In the following example, add the spacing to the glyph 2 if the
        // `offset_in_run` is 1, 2, or 3.
        //   Glyph|0|1|2|3|4|5|
        //   Char |0|0|0|3|3|4|
        should_add_spacing = next_character_index >= offset_in_run;
      } else {
        // TODO(crbug.com/1463890): RTL might need more considerations, both
        // the protocol and the logic.
        // In the following example, add the spacing to the glyph 2 if the
        // `offset_in_run` is 1, 2, or 3.
        //   Glyph|0|1|2|3|4|5|
        //   Char |4|3|3|0|0|0|
        if (offset_in_run == run->num_characters_) {
          // Except when adding to the end of the run. In that case, add to the
          // last glyph.
          should_add_spacing = i == run->NumGlyphs() - 1;
        } else {
          should_add_spacing = next_character_index < offset_in_run;
        }
      }
      if (should_add_spacing) {
        HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];
        glyph_data.AddAdvance(current_offset->spacing);
        total_space_for_run += current_offset->spacing;

        ShapeResultCharacterData& data = CharacterData(offset - 1);
        DCHECK(!data.has_auto_spacing_after);
        data.has_auto_spacing_after = true;

        if (++current_offset == offset_end) {
          break;
        }
        offset = current_offset->offset;
        DCHECK_GE(offset, run->start_index_);
        offset_in_run = offset - run->start_index_;
      }
    }
    run->width_ += total_space_for_run;
  }
#if 0
  // TODO(crbug.com/333698368): Disable the DCHECK for now to unblock VS test.
  DCHECK(current_offset == offset_end);  // Check if all offsets are consumed.
#endif
  // `width_` will be updated in `RecalcCharacterPositions()`.
}

const ShapeResult* ShapeResult::UnapplyAutoSpacing(
    float spacing_width,
    unsigned start_offset,
    unsigned break_offset) const {
  DCHECK_GE(start_offset, StartIndex());
  DCHECK_GT(break_offset, start_offset);
  DCHECK_LE(break_offset, EndIndex());
  DCHECK(HasAutoSpacingBefore(break_offset));

  // Create a `ShapeResult` for the character before `break_offset`.
  ShapeResult* sub_range = SubRange(start_offset, break_offset);

  // Remove the auto-spacing from the last glyph.
  for (const Member<RunInfo>& run : base::Reversed(sub_range->runs_)) {
    if (!run->NumGlyphs()) [[unlikely]] {
      continue;
    }
    HarfBuzzRunGlyphData& last_glyph = run->glyph_data_.back();
    DCHECK_GE(last_glyph.advance.ToFloat(), spacing_width);
    last_glyph.AddAdvance(-spacing_width);
    run->width_ -= spacing_width;
    sub_range->width_ -= spacing_width;
    break;
  }
  return sub_range;
}

unsigned ShapeResult::AdjustOffsetForAutoSpacing(float spacing_width,
                                                 unsigned offset,
                                                 float position) const {
  DCHECK(!character_position_.empty());
  DCHECK(HasAutoSpacingAfter(offset));
  DCHECK_GE(offset, StartIndex());
  offset -= StartIndex();
  DCHECK_LT(offset, NumCharacters());
  // If the next character fits in `position + spacing_width`, then advance
  // the break offset. The auto-spacing at line edges will be removed by
  // `UnapplyAutoSpacing`.
  if (IsLtr()) {
    position += spacing_width;
    if (offset + 1 < NumCharacters()) {
      const ShapeResultCharacterData& data = character_position_[offset + 1];
      if (data.x_position <= position) {
        ++offset;
      }
    } else {
      if (Width() <= position) {
        offset = NumCharacters();
      }
    }
  } else {
    position -= spacing_width;
    if (offset + 1 < NumCharacters()) {
      const ShapeResultCharacterData& data = character_position_[offset + 1];
      if (data.x_position >= position) {
        ++offset;
      }
    } else {
      if (Width() <= -position) {
        offset = NumCharacters();
      }
    }
  }
  return offset + StartIndex();
}

namespace {

float HarfBuzzPositionToFloat(hb_position_t value) {
  return static_cast<float>(value) / (1 << 16);
}

inline TextRunLayoutUnit HarfBuzzPositionToTextLayoutUnit(hb_position_t value) {
  return TextRunLayoutUnit::FromFixed<16>(value);
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
    if (last_cluster > max_cluster) [[unlikely]] {
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
    if (last_cluster > max_cluster) [[unlikely]] {
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
  if (num_glyphs > HarfBuzzRunGlyphData::kMaxGlyphs) [[unlikely]] {
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
  InlineLayoutUnit total_advance;
  bool has_vertical_offsets = !is_horizontal_run;

  // HarfBuzz returns result in visual order, no need to flip for RTL.
  for (unsigned i = 0; i < num_glyphs; ++i) {
    const hb_glyph_info_t glyph = glyph_infos[start_glyph + i];
    const hb_glyph_position_t& pos = glyph_positions[start_glyph + i];

    // One out of x_advance and y_advance is zero, depending on
    // whether the buffer direction is horizontal or vertical.
    // Convert to float and negate to avoid integer-overflow for ULONG_MAX.
    const TextRunLayoutUnit advance =
        is_horizontal_run ? HarfBuzzPositionToTextLayoutUnit(pos.x_advance)
                          : -HarfBuzzPositionToTextLayoutUnit(pos.y_advance);

    DCHECK_GE(glyph.cluster, start_cluster);
    const uint16_t character_index = glyph.cluster - start_cluster;
    DCHECK_LE(character_index, HarfBuzzRunGlyphData::kMaxCharacterIndex);
    DCHECK_LT(character_index, run->num_characters_);
    run->glyph_data_[i] = {glyph.codepoint, character_index,
                           IsSafeToBreakBefore(glyph_infos + start_glyph, i,
                                               num_glyphs, Direction()),
                           advance};

    // Offset is primarily used when painting glyphs. Keep it in physical.
    if (pos.x_offset || pos.y_offset) [[unlikely]] {
      has_vertical_offsets |= (pos.y_offset != 0);
      const GlyphOffset offset(HarfBuzzPositionToFloat(pos.x_offset),
                               -HarfBuzzPositionToFloat(pos.y_offset));
      run->glyph_data_.SetOffsetAt(i, offset);
    }

    total_advance += advance;
  }

  run->width_ = total_advance.ClampNegativeToZero().ToFloat();
  has_vertical_offsets_ |= has_vertical_offsets;
  run->CheckConsistency();
}

void ShapeResult::InsertRun(ShapeResult::RunInfo* run,
                            unsigned start_glyph,
                            unsigned num_glyphs,
                            unsigned* next_start_glyph,
                            hb_buffer_t* harfbuzz_buffer) {
  DCHECK_GT(num_glyphs, 0u);

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
    ComputeGlyphPositions<true>(run, start_glyph, num_glyphs, harfbuzz_buffer);
  } else {
    // Inserting a vertical run to a vertical result.
    ComputeGlyphPositions<false>(run, start_glyph, num_glyphs, harfbuzz_buffer);
  }
  width_ += run->width_;
  num_glyphs_ += run->NumGlyphs();
  DCHECK_GE(num_glyphs_, run->NumGlyphs());

  InsertRun(run);
}

void ShapeResult::InsertRun(ShapeResult::RunInfo* run) {
  // The runs are stored in result->m_runs in visual order. For LTR, we place
  // the run to be inserted before the next run with a bigger character start
  // index.
  const auto ltr_comparer = [](Member<RunInfo>& run, unsigned start_index) {
    return run->start_index_ < start_index;
  };

  // For RTL, we place the run before the next run with a lower character
  // index. Otherwise, for both directions, at the end.
  const auto rtl_comparer = [](Member<RunInfo>& run, unsigned start_index) {
    return run->start_index_ > start_index;
  };

  auto it = std::lower_bound(
      runs_.begin(), runs_.end(), run->start_index_,
      HB_DIRECTION_IS_FORWARD(run->direction_) ? ltr_comparer : rtl_comparer);
  if (it != runs_.end()) {
    runs_.insert(static_cast<wtf_size_t>(it - runs_.begin()), run);
  } else {
    // If we didn't find an existing slot to place it, append.
    runs_.push_back(run);
  }
}

ShapeResult::RunInfo* ShapeResult::InsertRunForTesting(
    unsigned start_index,
    unsigned num_characters,
    TextDirection direction,
    Vector<uint16_t> safe_break_offsets) {
  auto* run = MakeGarbageCollected<RunInfo>(
      nullptr, blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL,
      CanvasRotationInVertical::kRegular, HB_SCRIPT_COMMON, start_index,
      num_characters, num_characters);
  for (unsigned i = 0; i < run->glyph_data_.size(); i++) {
    run->glyph_data_[i] = {0, i, false, TextRunLayoutUnit()};
  }
  for (uint16_t offset : safe_break_offsets)
    run->glyph_data_[offset].safe_to_break_before = true;
  // RTL runs have glyphs in the descending order of character_index.
  if (IsRtl())
    run->glyph_data_.Reverse();
  num_glyphs_ += run->NumGlyphs();
  InsertRun(run);
  return run;
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
    RunInfo* new_run = runs_.back();
    runs_.pop_back();
    runs_.push_front(new_run);
    return;
  }

  // |push_front| is O(n) that we should not call it multiple times.
  // Create a new list in the correct order and swap it.
  HeapVector<Member<RunInfo>> new_runs;
  new_runs.ReserveInitialCapacity(runs_.size());
  for (unsigned i = run_size_before; i < runs_.size(); i++)
    new_runs.push_back(runs_[i]);

  // Then append existing runs.
  for (unsigned i = 0; i < run_size_before; i++)
    new_runs.push_back(runs_[i]);
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

      if (RunInfo* sub_run = run->CreateSubRun(start, end)) {
        sub_run->start_index_ += index_diff;
        target->width_ += sub_run->width_;
        target->num_glyphs_ += sub_run->glyph_data_.size();
        if (auto* merged_run =
                should_merge ? target->runs_.back()->MergeIfPossible(*sub_run)
                             : nullptr) {
          target->runs_.back() = merged_run;
        } else {
          target->runs_.push_back(sub_run);
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
  if (IsRtl() && target->runs_.size() != target_run_size_before) [[unlikely]] {
    target->ReorderRtlRuns(target_run_size_before);
  }

  target->has_vertical_offsets_ |= has_vertical_offsets_;

#if DCHECK_IS_ON()
  DCHECK_EQ(
      target->num_characters_ - target_num_characters_before,
      std::min(end_offset, EndIndex()) - std::max(start_offset, StartIndex()));
  target->CheckConsistency();
#endif

  return run_index;
}

ShapeResult* ShapeResult::SubRange(unsigned start_offset,
                                   unsigned end_offset) const {
  ShapeResult* sub_range =
      MakeGarbageCollected<ShapeResult>(primary_font_.Get(), 0, 0, Direction());
  CopyRange(start_offset, end_offset, sub_range);
  return sub_range;
}

const ShapeResult* ShapeResult::CopyAdjustedOffset(unsigned start_index) const {
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(*this);

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

const ShapeResult* ShapeResult::CreateForTabulationCharacters(
    const Font* font,
    TextDirection direction,
    const TabSize& tab_size,
    float position,
    unsigned start_index,
    unsigned length) {
  DCHECK_GT(length, 0u);
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(font, start_index, length, direction);
  result->num_glyphs_ = length;
  DCHECK_EQ(result->num_glyphs_, length);  // no overflow
  result->has_vertical_offsets_ =
      font_data->PlatformData().IsVerticalAnyUpright();
  // Tab characters are always LTR or RTL, not TTB, even when
  // isVerticalAnyUpright().
  hb_direction_t hb_direction =
      blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;
  // Only the advance of the first tab is affected by |position|.
  TextRunLayoutUnit advance = TextRunLayoutUnit::FromFloatRound(
      font->TabWidth(font_data, tab_size, position));
  do {
    unsigned run_length = std::min(length, HarfBuzzRunGlyphData::kMaxGlyphs);
    RunInfo* run = MakeGarbageCollected<RunInfo>(
        font_data, hb_direction, CanvasRotationInVertical::kRegular,
        HB_SCRIPT_COMMON, start_index, run_length, run_length);
    InlineLayoutUnit run_width;
    for (unsigned i = 0; i < run_length; i++) {
      // 2nd and following tabs have the base width, without using |position|.
      if (i == 1) {
        advance = TextRunLayoutUnit::FromFloatRound(
            font->TabWidth(font_data, tab_size));
      }
      const unsigned index = blink::IsLtr(direction) ? i : length - 1 - i;
      run->glyph_data_[i] = {font_data->SpaceGlyph(), index, true, advance};
      run_width += advance;
    }
    run->width_ = run_width;
    result->width_ += run->width_;
    result->runs_.push_back(run);
    DCHECK_GE(length, run_length);
    length -= run_length;
    start_index += run_length;
  } while (length);
  return result;
}

const ShapeResult* ShapeResult::CreateForSpaces(const Font* font,
                                                TextDirection direction,
                                                unsigned start_index,
                                                unsigned length,
                                                float width) {
  DCHECK_GT(length, 0u);
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(font, start_index, length, direction);
  result->num_glyphs_ = length;
  DCHECK_EQ(result->num_glyphs_, length);  // no overflow
  result->has_vertical_offsets_ =
      font_data->PlatformData().IsVerticalAnyUpright();
  hb_direction_t hb_direction =
      blink::IsLtr(direction) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL;
  RunInfo* run = MakeGarbageCollected<RunInfo>(
      font_data, hb_direction, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, start_index, length, length);
  result->width_ = run->width_ = width;
  length = run->NumGlyphs();
  TextRunLayoutUnit glyph_width = TextRunLayoutUnit::FromFloatRound(width);
  for (unsigned i = 0; i < length; i++) {
    const unsigned index = blink::IsLtr(direction) ? i : length - 1 - i;
    run->glyph_data_[i] = {font_data->SpaceGlyph(), index, true, glyph_width};
    glyph_width = TextRunLayoutUnit();
  }
  result->runs_.push_back(run);
  return result;
}

const ShapeResult* ShapeResult::CreateForStretchyMathOperator(
    const Font* font,
    TextDirection direction,
    Glyph glyph_variant,
    float stretch_size) {
  unsigned start_index = 0;
  unsigned num_characters = 1;
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(
      font, start_index, num_characters, direction);

  hb_direction_t hb_direction = HB_DIRECTION_LTR;
  unsigned glyph_index = 0;
  RunInfo* run = MakeGarbageCollected<RunInfo>(
      font->PrimaryFont(), hb_direction, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, start_index, 1 /* num_glyph */, num_characters);
  run->glyph_data_[glyph_index] = {
      glyph_variant, 0 /* character index */, true /* IsSafeToBreakBefore */,
      TextRunLayoutUnit::FromFloatRound(stretch_size)};
  run->width_ = std::max(0.0f, stretch_size);

  result->width_ = run->width_;
  result->num_glyphs_ = run->NumGlyphs();
  result->runs_.push_back(run);

  return result;
}

const ShapeResult* ShapeResult::CreateForStretchyMathOperator(
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
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(
      font, start_index, num_characters, direction);

  hb_direction_t hb_direction =
      is_horizontal_assembly ? HB_DIRECTION_LTR : HB_DIRECTION_TTB;
  RunInfo* run = MakeGarbageCollected<RunInfo>(
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
    float glyph_ink_ascent;
    if (!is_horizontal_assembly) {
      glyph_ink_ascent = -font->PrimaryFont()->BoundsForGlyph(part.glyph).y();
    }
    for (unsigned repetition_index = 0; repetition_index < repetition_count;
         repetition_index++) {
      unsigned glyph_index =
          is_horizontal_assembly
              ? part_index
              : assembly_parameters.glyph_count - 1 - part_index;
      float full_advance = glyph_index == assembly_parameters.glyph_count - 1
                               ? part.full_advance
                               : part.full_advance - overlap;
      run->glyph_data_[glyph_index] = {
          part.glyph, 0 /* character index */,
          !glyph_index /* IsSafeToBreakBefore */,
          TextRunLayoutUnit::FromFloatRound(full_advance)};
      if (!is_horizontal_assembly) {
        GlyphOffset glyph_offset(
            0, -assembly_parameters.stretch_size + glyph_ink_ascent);
        run->glyph_data_.SetOffsetAt(glyph_index, glyph_offset);
        result->has_vertical_offsets_ |= (glyph_offset.y() != 0);
      }
      part_index++;
    }
  }
  run->width_ = std::max(0.0f, assembly_parameters.stretch_size);

  result->width_ = run->width_;
  result->num_glyphs_ = run->NumGlyphs();
  result->runs_.push_back(run);
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
  unsigned next_character_index = 0;
  InlineLayoutUnit total_advance;
  InlineLayoutUnit last_x_position;

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

    for (const auto& glyph_data : run->glyph_data_) {
      DCHECK_GE(run->start_index_, start_index_);
      const unsigned logical_index =
          run->start_index_ + glyph_data.character_index - start_index_;

      // Make |character_index| to the visual offset.
      DCHECK_LT(logical_index, num_characters_);
      const unsigned character_index =
          rtl ? num_characters_ - logical_index - 1 : logical_index;

      // If this glyph is the first glyph of a new cluster, set the data.
      // Otherwise, |character_position_[character_index]| is already set.
      // Do not overwrite.
      if (character_index >= num_characters_) {
        // We are not sure why we reach here. See http://crbug.com/1286882
        NOTREACHED_IN_MIGRATION();
        continue;
      }
      if (next_character_index <= character_index) {
        if (next_character_index < character_index) {
          // Multiple glyphs may have the same character index and not all
          // character indices may have glyphs. For character indices without
          // glyphs set the x-position to that of the nearest preceding glyph in
          // the logical order; i.e., the last position for LTR or this position
          // for RTL.
          const LayoutUnit x_position =
              (!rtl ? last_x_position : total_advance).ToCeil<LayoutUnit>();
          for (unsigned i = next_character_index; i < character_index; i++) {
            DCHECK_LT(i, num_characters_);
            character_position_[i].SetCachedData(x_position, false, false);
          }
        }

        const LayoutUnit x_position = total_advance.ToCeil<LayoutUnit>();
        character_position_[character_index].SetCachedData(
            x_position, true, glyph_data.safe_to_break_before);
        last_x_position = total_advance;
      }

      total_advance += glyph_data.advance;
      next_character_index = character_index + 1;
    }
  }

  // Fill |x_position| for the rest of characters, when they don't have
  // corresponding glyphs.
  if (next_character_index < num_characters_) {
    const LayoutUnit x_position =
        (!rtl ? last_x_position : total_advance).ToCeil<LayoutUnit>();
    for (unsigned i = next_character_index; i < num_characters_; i++) {
      character_position_[i].SetCachedData(x_position, false, false);
    }
  }

  width_ = total_advance;
}

void ShapeResult::EnsurePositionData() const {
  if (!character_position_.empty()) {
    return;
  }

  character_position_ = HeapVector<ShapeResultCharacterData>(num_characters_);
  RecalcCharacterPositions();
}

void ShapeResult::RecalcCharacterPositions() const {
  DCHECK(!character_position_.empty());

  if (IsLtr()) {
    ComputePositionData<false>();
  } else {
    ComputePositionData<true>();
  }
}

// TODO(eae): Might be worth trying to set midpoint to ~50% more than the number
// of characters in the previous line for the first try. Would cut the number
// of tries in the majority of cases for long strings.
unsigned ShapeResult::CachedOffsetForPosition(LayoutUnit x) const {
  DCHECK(!character_position_.empty());

  // At or before start, return offset *of* the first character.
  // At or beyond the end, return offset *after* the last character.
  const bool rtl = IsRtl();
  const unsigned length = character_position_.size();
  if (x <= 0)
    return !rtl ? 0 : length;
  if (x >= width_)
    return !rtl ? length : 0;

  // Do a binary search to find the largest x-position that is less than or
  // equal to the supplied x value.
  unsigned low = 0;
  unsigned high = length - 1;
  while (low <= high) {
    unsigned midpoint = low + (high - low) / 2;
    const LayoutUnit x_position = character_position_[midpoint].x_position;
    if (x_position <= x && (midpoint + 1 == length ||
                            character_position_[midpoint + 1].x_position > x)) {
      if (!rtl)
        return midpoint;
      // The border belongs to the logical next character.
      return x_position == x ? length - midpoint : length - midpoint - 1;
    }
    if (x < x_position) {
      high = midpoint - 1;
    } else {
      low = midpoint + 1;
    }
  }

  return 0;
}

LayoutUnit ShapeResult::CachedPositionForOffset(unsigned offset) const {
  DCHECK_GE(offset, 0u);
  DCHECK_LE(offset, num_characters_);
  DCHECK(!character_position_.empty());

  const bool rtl = IsRtl();
  const unsigned length = character_position_.size();
  if (!rtl) {
    if (offset < length) {
      return character_position_[offset].x_position;
    }
  } else {
    if (offset >= length) {
      return LayoutUnit();
    }
    // Return the left edge of the next character because in RTL, the position
    // is the right edge of the character.
    for (unsigned visual_offset = length - offset - 1; visual_offset < length;
         visual_offset++) {
      if (character_position_[visual_offset].is_cluster_base) {
        return visual_offset + 1 < length
                   ? character_position_[visual_offset + 1].x_position
                   : LayoutUnit::FromFloatCeil(width_);
      }
    }
  }
  return LayoutUnit::FromFloatCeil(width_);
}

LayoutUnit ShapeResult::CachedWidth(unsigned start_offset,
                                    unsigned end_offset) const {
  const unsigned offset_adjust = StartIndex();
  const LayoutUnit start_position =
      CachedPositionForOffset(start_offset - offset_adjust);
  const LayoutUnit end_position =
      CachedPositionForOffset(end_offset - offset_adjust);
  return IsLtr() ? end_position - start_position
                 : start_position - end_position;
}

unsigned ShapeResult::CachedNextSafeToBreakOffset(unsigned offset) const {
  if (IsRtl()) {
    return NextSafeToBreakOffset(offset);
  }

  DCHECK(!character_position_.empty());
  DCHECK_LE(start_index_, offset);

  const unsigned adjusted_offset = offset - start_index_;
  const unsigned length = character_position_.size();
  DCHECK_LT(adjusted_offset, length);

  for (unsigned i = adjusted_offset; i < length; i++) {
    if (character_position_[i].safe_to_break_before) {
      return start_index_ + i;
    }
  }

  // Next safe break is at the end of the run.
  return start_index_ + length;
}

unsigned ShapeResult::CachedPreviousSafeToBreakOffset(unsigned offset) const {
  if (IsRtl()) {
    return PreviousSafeToBreakOffset(offset);
  }

  DCHECK(!character_position_.empty());
  DCHECK_LE(start_index_, offset);

  const unsigned adjusted_offset = offset - start_index_;
  const unsigned length = character_position_.size();
  DCHECK_LE(adjusted_offset, length);

  // Assume it is always safe to break at the end of the run.
  if (adjusted_offset >= length) {
    return start_index_ + length;
  }

  for (unsigned i = adjusted_offset + 1; i > 0; i--) {
    if (character_position_[i - 1].safe_to_break_before) {
      return start_index_ + (i - 1);
    }
  }

  // Previous safe break is at the start of the run.
  return RuntimeEnabledFeatures::
                 ShapeResultCachedPreviousSafeToBreakOffsetEnabled()
             ? start_index_
             : 0;
}

namespace {

void AddRunInfoRanges(const ShapeResult::RunInfo& run_info,
                      float offset,
                      Vector<CharacterRange>* ranges) {
  Vector<float> character_widths(run_info.num_characters_);
  for (const auto& glyph : run_info.glyph_data_) {
    // TODO(crbug.com/1147011): This should not happen, but crash logs indicate
    // that this is happening.
    if (glyph.character_index >= character_widths.size()) [[unlikely]] {
      NOTREACHED_IN_MIGRATION();
      character_widths.Grow(glyph.character_index + 1);
    }
    character_widths[glyph.character_index] += glyph.advance.ToFloat();
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
#if !BUILDFLAG(IS_APPLE)
  Vector<Glyph, 256> glyphs(num_glyphs);
  unsigned i = 0;
  for (const auto& glyph_data : run.glyph_data_)
    glyphs[i++] = glyph_data.glyph;
  Vector<SkRect, 256> bounds_list(num_glyphs);
  current_font_data.BoundsForGlyphs(glyphs, &bounds_list);
#endif

  GlyphBoundsAccumulator<is_horizontal_run> bounds;
  InlineLayoutUnit origin = InlineLayoutUnit::FromFloatCeil(run_advance);
  for (unsigned j = 0; j < num_glyphs; ++j) {
    const HarfBuzzRunGlyphData& glyph_data = run.glyph_data_[j];
#if BUILDFLAG(IS_APPLE)
    gfx::RectF glyph_bounds =
        current_font_data.BoundsForGlyph(glyph_data.glyph);
#else
    gfx::RectF glyph_bounds = gfx::SkRectToRectF(bounds_list[j]);
#endif
    bounds.Unite(glyph_bounds, origin, *glyph_offsets);
    ++glyph_offsets;
    origin += glyph_data.advance;
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
        ComputeRunInkBounds<true, true>(*run, run_advance, &ink_bounds);
      else
        ComputeRunInkBounds<false, true>(*run, run_advance, &ink_bounds);
    } else {
      if (run->IsHorizontal())
        ComputeRunInkBounds<true, false>(*run, run_advance, &ink_bounds);
      else
        ComputeRunInkBounds<false, false>(*run, run_advance, &ink_bounds);
    }
    run_advance += run->width_;
  }

  return ink_bounds;
}

}  // namespace blink
