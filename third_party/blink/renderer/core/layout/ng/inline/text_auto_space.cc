// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_segment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_data.h"

namespace blink {

namespace {

// True if the script is one of "Ideographs" defined in CSS Text:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
// Note, this doesn't cover all ideographs as defined in Unicode.
inline bool IsIdeograph(UScriptCode script) {
  // `ScriptRunIterator` normalizes these scripts to `USCRIPT_HIRAGANA`.
  DCHECK_NE(script, USCRIPT_KATAKANA);
  DCHECK_NE(script, USCRIPT_KATAKANA_OR_HIRAGANA);
  return script == USCRIPT_HAN || script == USCRIPT_HIRAGANA;
}

}  // namespace

// static
void TextAutoSpace::ApplyIfNeeded(NGInlineItemsData& data,
                                  Vector<wtf_size_t>* offsets_out) {
  HeapVector<NGInlineItem>& items = data.items;
  if (UNLIKELY(items.empty())) {
    return;
  }

  // Compute `RunSegmenterRange` for the whole text content. It's pre-computed,
  // but packed in `NGInlineItemSegments` to save memory.
  NGInlineItemSegments::RunSegmenterRanges ranges;
  const String& text = data.text_content;
  if (!data.segments) {
    const NGInlineItem& item0 = items.front();
    RunSegmenter::RunSegmenterRange range = item0.CreateRunSegmenterRange();
    if (!IsIdeograph(range.script)) {
      return;
    }
    range.end = text.length();
    ranges.push_back(range);
  } else {
    data.segments->ToRanges(ranges);
    if (std::none_of(ranges.begin(), ranges.end(),
                     [](const RunSegmenter::RunSegmenterRange& range) {
                       return IsIdeograph(range.script);
                     })) {
      return;
    }
  }
  DCHECK(!text.Is8Bit());
  DCHECK_EQ(text.length(), ranges.back().end);

  Vector<wtf_size_t, 16> offsets;
  CHECK(!ranges.empty());
  const RunSegmenter::RunSegmenterRange* range = ranges.begin();
  absl::optional<CharType> last_type = kOther;
  for (const NGInlineItem& item : items) {
    if (item.Type() != NGInlineItem::kText) {
      if (item.Length()) {
        // If `item` has a length, e.g., inline-block, set the `last_type`.
        last_type = kOther;
      }
      continue;
    }
    if (UNLIKELY(!item.Length())) {
      // Empty items may not have `ShapeResult`. Skip it.
      continue;
    }
    const ComputedStyle* style = item.Style();
    DCHECK(style);
    if (UNLIKELY(style->TextAutospace() != ETextAutospace::kNormal)) {
      continue;
    }

    wtf_size_t offset = item.StartOffset();
    do {
      // Find the `RunSegmenterRange` for `offset`.
      while (offset >= range->end) {
        ++range;
        CHECK_NE(range, ranges.end());
      }
      DCHECK_GE(offset, range->start);
      DCHECK_LT(offset, range->end);

      const wtf_size_t end_offset = std::min(range->end, item.EndOffset());
      DCHECK_LT(offset, end_offset);
      if (IsIdeograph(range->script)) {
        // When the script is ideograph, it may contain digits because they are
        // COMMON. Scan the text.
        if (!last_type) {
          DCHECK_GT(offset, 0u);
          last_type = GetPrevType(text, offset);
        }
        while (offset < end_offset) {
          const wtf_size_t saved_offset = offset;
          const CharType type = GetTypeAndNext(text, offset);
          if ((type == kIdeograph && last_type == kLetterOrNumeral) ||
              (last_type == kIdeograph && type == kLetterOrNumeral)) {
            offsets.push_back(saved_offset);
          }
          last_type = type;
        }
      } else {
        // When the script isn't ideograph, it must not contain ideographs.
        // Check the first and the last character.
        if (last_type == kIdeograph) {
          const wtf_size_t saved_offset = offset;
          const CharType type = GetTypeAndNext(text, offset);
          DCHECK_NE(type, kIdeograph);
          if (type == kLetterOrNumeral) {
            offsets.push_back(saved_offset);
          }
          if (offset == end_offset) {
            last_type = type;
          } else {
            last_type.reset();
            offset = end_offset;
          }
        } else {
          last_type.reset();
          offset = end_offset;
        }
      }
    } while (offset < item.EndOffset());

    if (!offsets_out) {
      // TODO(crbug.com/1463890): Apply to `ShapeResult` not implemented yet.
    } else {
      offsets_out->AppendVector(offsets);
    }
    offsets.Shrink(0);
  }
}

// static
TextAutoSpace::CharType TextAutoSpace::GetTypeAndNext(const String& text,
                                                      wtf_size_t& offset) {
  if (text.Is8Bit()) {
    return GetType(text[offset++]);
  }
  UChar ch;
  U16_NEXT(text.Characters16(), offset, text.length(), ch);
  return GetType(ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetPrevType(const String& text,
                                                   wtf_size_t offset) {
  DCHECK_GT(offset, 0u);
  if (text.Is8Bit()) {
    return GetType(text[offset - 1]);
  }
  UChar last_ch;
  U16_PREV(text.Characters16(), 0, offset, last_ch);
  return GetType(last_ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetType(UChar32 ch) {
  // This logic is based on:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  const uint32_t gc_mask = U_GET_GC_MASK(ch);
  if (ch >= 0x3041 && ch <= 0x30FF && !(gc_mask & U_GC_P_MASK)) {
    return kIdeograph;
  }
  if (ch >= 0x31C0 && ch <= 0x31FF) {
    return kIdeograph;
  }
  UErrorCode err = U_ZERO_ERROR;
  const UScriptCode script = uscript_getScript(ch, &err);
  DCHECK(U_SUCCESS(err));
  if (U_SUCCESS(err) && script == USCRIPT_HAN) {
    return kIdeograph;
  }

  if (gc_mask & (U_GC_L_MASK | U_GC_M_MASK | U_GC_ND_MASK)) {
    const UEastAsianWidth eaw = static_cast<UEastAsianWidth>(
        u_getIntPropertyValue(ch, UCHAR_EAST_ASIAN_WIDTH));
    if (eaw != UEastAsianWidth::U_EA_FULLWIDTH) {
      return kLetterOrNumeral;
    }
  }
  return kOther;
}

std::ostream& operator<<(std::ostream& ostream, TextAutoSpace::CharType type) {
  switch (type) {
    case TextAutoSpace::kIdeograph:
      return ostream << "kIdeograph";
    case TextAutoSpace::kLetterOrNumeral:
      return ostream << "kLetterOrNumeral";
    case TextAutoSpace::kOther:
      return ostream << "kOther";
  }
  return ostream << static_cast<int>(type);
}

}  // namespace blink
