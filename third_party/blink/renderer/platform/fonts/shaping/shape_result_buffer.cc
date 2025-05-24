// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

namespace {

unsigned CharactersInShapeResult(
    const HeapVector<Member<const ShapeResult>, 64>& results) {
  unsigned num_characters = 0;
  for (const Member<const ShapeResult>& result : results) {
    num_characters += result->NumCharacters();
  }
  return num_characters;
}

}  // namespace

void ShapeResultBuffer::ComputeRangeIn(const ShapeResult& result,
                                       const gfx::RectF& ink_bounds,
                                       CharacterRangeContext& context) {
  result.EnsureGraphemes(StringView(context.text, context.total_num_characters,
                                    result.NumCharacters()));
  if (context.is_rtl) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    if (!context.from_x && context.from >= 0 &&
        static_cast<unsigned>(context.from) < result.NumCharacters()) {
      context.from = result.NumCharacters() - context.from - 1;
    }
    if (!context.to_x && context.to >= 0 &&
        static_cast<unsigned>(context.to) < result.NumCharacters()) {
      context.to = result.NumCharacters() - context.to - 1;
    }
    context.current_x -= result.Width();
  }
  for (unsigned i = 0; i < result.runs_.size(); i++) {
    if (!result.runs_[i]) {
      continue;
    }
    DCHECK_EQ(context.is_rtl, result.runs_[i]->IsRtl());
    int num_characters = result.runs_[i]->num_characters_;
    if (!context.from_x && context.from >= 0 && context.from < num_characters) {
      context.from_x = result.runs_[i]->XPositionForVisualOffset(
                           context.from, AdjustMidCluster::kToStart) +
                       context.current_x;
    } else {
      context.from -= num_characters;
    }

    if (!context.to_x && context.to >= 0 && context.to < num_characters) {
      context.to_x = result.runs_[i]->XPositionForVisualOffset(
                         context.to, AdjustMidCluster::kToEnd) +
                     context.current_x;
    } else {
      context.to -= num_characters;
    }

    if (context.from_x || context.to_x) {
      context.min_y = std::min(context.min_y, ink_bounds.y());
      context.max_y = std::max(context.max_y, ink_bounds.bottom());
    }

    if (context.from_x && context.to_x) {
      break;
    }
    context.current_x += result.runs_[i]->width_;
  }
  if (context.is_rtl) {
    context.current_x -= result.Width();
  }
  context.total_num_characters += result.NumCharacters();
}

CharacterRange ShapeResultBuffer::GetCharacterRange(
    const StringView& text,
    TextDirection direction,
    float total_width,
    unsigned absolute_from,
    unsigned absolute_to) const {
  DCHECK_EQ(CharactersInShapeResult(results_), text.length());

  // The absolute_from and absolute_to arguments represent the start/end offset
  // for the entire run, from/to are continuously updated to be relative to
  // the current word (ShapeResult instance).
  int from = absolute_from;
  int to = absolute_to;

  CharacterRangeContext context{text, IsRtl(direction), from, to,
                                IsRtl(direction) ? total_width : 0};
  for (unsigned j = 0; j < results_.size(); j++) {
    const ShapeResult* result = results_[j];
    ComputeRangeIn(*result, result->GetDeprecatedInkBounds(), context);
  }

  // The position in question might be just after the text.
  if (!context.from_x && absolute_from == context.total_num_characters) {
    context.from_x = direction == TextDirection::kRtl ? 0 : total_width;
  }
  if (!context.to_x && absolute_to == context.total_num_characters) {
    context.to_x = direction == TextDirection::kRtl ? 0 : total_width;
  }
  if (!context.from_x) {
    context.from_x = 0;
  }
  if (!context.to_x) {
    context.to_x = direction == TextDirection::kRtl ? 0 : total_width;
  }

  // None of our runs is part of the selection, possibly invalid arguments.
  if (!context.to_x && !context.from_x) {
    context.from_x = context.to_x = 0;
  }
  if (*context.from_x < *context.to_x) {
    return CharacterRange(*context.from_x, *context.to_x, -context.min_y,
                          context.max_y);
  }
  return CharacterRange(*context.to_x, *context.from_x, -context.min_y,
                        context.max_y);
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
      const Member<const ShapeResult>& word_result = results_[i - 1];
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
    for (const Member<const ShapeResult>& word_result : results_) {
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

HeapVector<ShapeResult::RunFontData> ShapeResultBuffer::GetRunFontData() const {
  HeapVector<ShapeResult::RunFontData> font_data;
  for (const auto& result : results_)
    result->GetRunFontData(&font_data);
  return font_data;
}

ShapeResultView* ShapeResultBuffer::ViewAt(wtf_size_t index) const {
  return ShapeResultView::Create(results_[index]);
}

GlyphData ShapeResultBuffer::EmphasisMarkGlyphData(
    const FontDescription& font_description) const {
  for (const auto& result : results_) {
    for (const auto& run : result->runs_) {
      DCHECK(run->font_data_);
      if (run->glyph_data_.IsEmpty())
        continue;

      return GlyphData(run->glyph_data_[0].glyph,
                       run->font_data_->EmphasisMarkFontData(font_description),
                       run->CanvasRotation());
    }
  }

  return GlyphData();
}

}  // namespace blink
