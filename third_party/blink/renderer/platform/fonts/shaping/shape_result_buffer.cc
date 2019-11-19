// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

namespace {

unsigned CharactersInShapeResult(
    const Vector<scoped_refptr<const ShapeResult>, 64>& results) {
  unsigned num_characters = 0;
  for (const scoped_refptr<const ShapeResult>& result : results)
    num_characters += result->NumCharacters();
  return num_characters;
}

}  // namespace

CharacterRange ShapeResultBuffer::GetCharacterRange(
    const StringView& text,
    TextDirection direction,
    float total_width,
    unsigned absolute_from,
    unsigned absolute_to) const {
  DCHECK_EQ(CharactersInShapeResult(results_), text.length());

  float current_x = 0;
  float from_x = 0;
  float to_x = 0;
  bool found_from_x = false;
  bool found_to_x = false;
  float min_y = 0;
  float max_y = 0;

  if (direction == TextDirection::kRtl)
    current_x = total_width;

  // The absoluteFrom and absoluteTo arguments represent the start/end offset
  // for the entire run, from/to are continuously updated to be relative to
  // the current word (ShapeResult instance).
  int from = absolute_from;
  int to = absolute_to;

  unsigned total_num_characters = 0;
  for (unsigned j = 0; j < results_.size(); j++) {
    const scoped_refptr<const ShapeResult> result = results_[j];
    result->EnsureGraphemes(
        StringView(text, total_num_characters, result->NumCharacters()));
    if (direction == TextDirection::kRtl) {
      // Convert logical offsets to visual offsets, because results are in
      // logical order while runs are in visual order.
      if (!found_from_x && from >= 0 &&
          static_cast<unsigned>(from) < result->NumCharacters())
        from = result->NumCharacters() - from - 1;
      if (!found_to_x && to >= 0 &&
          static_cast<unsigned>(to) < result->NumCharacters())
        to = result->NumCharacters() - to - 1;
      current_x -= result->Width();
    }
    for (unsigned i = 0; i < result->runs_.size(); i++) {
      if (!result->runs_[i])
        continue;
      DCHECK_EQ(direction == TextDirection::kRtl, result->runs_[i]->Rtl());
      int num_characters = result->runs_[i]->num_characters_;
      if (!found_from_x && from >= 0 && from < num_characters) {
        from_x = result->runs_[i]->XPositionForVisualOffset(
                     from, AdjustMidCluster::kToStart) +
                 current_x;
        found_from_x = true;
      } else {
        from -= num_characters;
      }

      if (!found_to_x && to >= 0 && to < num_characters) {
        to_x = result->runs_[i]->XPositionForVisualOffset(
                   to, AdjustMidCluster::kToEnd) +
               current_x;
        found_to_x = true;
      } else {
        to -= num_characters;
      }

      if (found_from_x || found_to_x) {
        min_y = std::min(min_y, result->DeprecatedInkBounds().Y());
        max_y = std::max(max_y, result->DeprecatedInkBounds().MaxY());
      }

      if (found_from_x && found_to_x)
        break;
      current_x += result->runs_[i]->width_;
    }
    if (direction == TextDirection::kRtl)
      current_x -= result->Width();
    total_num_characters += result->NumCharacters();
  }

  // The position in question might be just after the text.
  if (!found_from_x && absolute_from == total_num_characters) {
    from_x = direction == TextDirection::kRtl ? 0 : total_width;
    found_from_x = true;
  }
  if (!found_to_x && absolute_to == total_num_characters) {
    to_x = direction == TextDirection::kRtl ? 0 : total_width;
    found_to_x = true;
  }
  if (!found_from_x)
    from_x = 0;
  if (!found_to_x)
    to_x = direction == TextDirection::kRtl ? 0 : total_width;

  // None of our runs is part of the selection, possibly invalid arguments.
  if (!found_to_x && !found_from_x)
    from_x = to_x = 0;
  if (from_x < to_x)
    return CharacterRange(from_x, to_x, -min_y, max_y);
  return CharacterRange(to_x, from_x, -min_y, max_y);
}

Vector<CharacterRange> ShapeResultBuffer::IndividualCharacterRanges(
    TextDirection direction,
    float total_width) const {
  Vector<CharacterRange> ranges;
  float current_x = direction == TextDirection::kRtl ? total_width : 0;
  for (const scoped_refptr<const ShapeResult> result : results_)
    current_x = result->IndividualCharacterRanges(&ranges, current_x);
  return ranges;
}

void ShapeResultBuffer::AddRunInfoAdvances(const ShapeResult::RunInfo& run_info,
                                           double offset,
                                           Vector<double>& advances) {
  const unsigned num_glyphs = run_info.glyph_data_.size();
  const unsigned num_chars = run_info.num_characters_;

  if (run_info.Rtl())
    offset += run_info.width_;

  double current_width = 0;
  for (unsigned glyph_id = 0; glyph_id < num_glyphs; glyph_id++) {
    unsigned gid = run_info.Rtl() ? num_glyphs - glyph_id - 1 : glyph_id;
    unsigned next_gid =
        run_info.Rtl() ? num_glyphs - glyph_id - 2 : glyph_id + 1;
    const HarfBuzzRunGlyphData& glyph = run_info.glyph_data_[gid];

    unsigned char_id = glyph.character_index;
    unsigned next_char_id =
        (glyph_id + 1 == num_glyphs)
            ? num_chars
            : run_info.glyph_data_[next_gid].character_index;

    current_width += glyph.advance;

    if (char_id == next_char_id)
      continue;

    unsigned num_graphemes = run_info.NumGraphemes(char_id, next_char_id);

    for (unsigned i = char_id; i < next_char_id; i++) {
      if (run_info.Rtl()) {
        advances.push_back(offset - (current_width / num_graphemes));
      } else {
        advances.push_back(offset);
      }

      if (num_graphemes == next_char_id - char_id) {
        offset += (current_width / num_graphemes) * (run_info.Rtl() ? -1 : 1);
      }
    }

    if (num_graphemes != next_char_id - char_id) {
      offset += current_width * (run_info.Rtl() ? -1 : 1);
    }

    current_width = 0;
  }
}

Vector<double> ShapeResultBuffer::IndividualCharacterAdvances(
    const StringView& text,
    TextDirection direction,
    float total_width) const {
  unsigned character_offset = 0;
  Vector<double> advances;
  double current_x = direction == TextDirection::kRtl ? total_width : 0;

  for (const scoped_refptr<const ShapeResult> result : results_) {
    unsigned run_count = result->runs_.size();

    result->EnsureGraphemes(
        StringView(text, character_offset, result->NumCharacters()));

    if (result->Rtl()) {
      for (int index = run_count - 1; index >= 0; index--) {
        current_x -= result->runs_[index]->width_;
        AddRunInfoAdvances(*result->runs_[index], current_x, advances);
      }
    } else {
      for (unsigned index = 0; index < run_count; index++) {
        AddRunInfoAdvances(*result->runs_[index], current_x, advances);
        current_x += result->runs_[index]->width_;
      }
    }

    character_offset += result->NumCharacters();
  }
  return advances;
}

int ShapeResultBuffer::OffsetForPosition(
    const TextRun& run,
    float target_x,
    IncludePartialGlyphsOption partial_glyphs,
    BreakGlyphsOption break_glyphs) const {
  StringView text = run.ToStringView();
  unsigned total_offset;
  if (run.Rtl()) {
    total_offset = run.length();
    for (unsigned i = results_.size(); i; --i) {
      const scoped_refptr<const ShapeResult>& word_result = results_[i - 1];
      if (!word_result)
        continue;
      total_offset -= word_result->NumCharacters();
      if (target_x >= 0 && target_x <= word_result->Width()) {
        int offset_for_word = word_result->OffsetForPosition(
            target_x,
            StringView(text, total_offset, word_result->NumCharacters()),
            partial_glyphs, break_glyphs);
        return total_offset + offset_for_word;
      }
      target_x -= word_result->Width();
    }
  } else {
    total_offset = 0;
    for (const scoped_refptr<const ShapeResult>& word_result : results_) {
      if (!word_result)
        continue;
      int offset_for_word = word_result->OffsetForPosition(
          target_x, StringView(text, 0, word_result->NumCharacters()),
          partial_glyphs, break_glyphs);
      DCHECK_GE(offset_for_word, 0);
      total_offset += offset_for_word;
      if (target_x >= 0 && target_x <= word_result->Width())
        return total_offset;
      text = StringView(text, word_result->NumCharacters());
      target_x -= word_result->Width();
    }
  }
  return total_offset;
}

void ShapeResultBuffer::ExpandRangeToIncludePartialGlyphs(int* from,
                                                          int* to) const {
  int offset = 0;
  for (unsigned j = 0; j < results_.size(); j++) {
    const scoped_refptr<const ShapeResult> result = results_[j];
    for (unsigned i = 0; i < result->runs_.size(); i++) {
      if (!result->runs_[i])
        continue;
      result->runs_[i]->ExpandRangeToIncludePartialGlyphs(offset, from, to);
      offset += result->runs_[i]->num_characters_;
    }
  }
}

Vector<ShapeResult::RunFontData> ShapeResultBuffer::GetRunFontData() const {
  Vector<ShapeResult::RunFontData> font_data;
  for (const auto& result : results_)
    result->GetRunFontData(&font_data);
  return font_data;
}

GlyphData ShapeResultBuffer::EmphasisMarkGlyphData(
    const FontDescription& font_description) const {
  for (const auto& result : results_) {
    for (const auto& run : result->runs_) {
      DCHECK(run->font_data_);
      if (run->glyph_data_.IsEmpty())
        continue;

      return GlyphData(
          run->glyph_data_[0].glyph,
          run->font_data_->EmphasisMarkFontData(font_description).get(),
          run->CanvasRotation());
    }
  }

  return GlyphData();
}

}  // namespace blink
