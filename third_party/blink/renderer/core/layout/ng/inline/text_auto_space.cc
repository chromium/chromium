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

// Check if the argument maybe "Ideographs" defined in CSS Text:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
// without getting Unicode properties, which is not slow but also not trivial.
//
// If this returns `false`, the text with the script does not contain
// "Ideographs."
//
// Note, this doesn't cover all ideographs as defined in Unicode.
inline bool MaybeIdeograph(UScriptCode script, StringView text) {
  // `ScriptRunIterator` normalizes these scripts to `USCRIPT_HIRAGANA`.
  DCHECK_NE(script, USCRIPT_KATAKANA);
  DCHECK_NE(script, USCRIPT_KATAKANA_OR_HIRAGANA);
  if (script == USCRIPT_HAN || script == USCRIPT_HIRAGANA) {
    return true;
  }
  // The "Ideographs" definition contains `USCRIPT_COMMON` and
  // `USCRIPT_INHERITED`, which can inherit scripts from its previous character.
  // They will be, for example, `USCRIPT_LATIN` if the previous character is
  // `USCRIPT_LATIN`. Check if we have any such characters.
  CHECK(!text.Is8Bit());
  return std::any_of(text.Characters16(), text.Characters16() + text.length(),
                     [](const UChar ch) {
                       return ch >= TextAutoSpace::kNonHanIdeographMin &&
                              ch <= TextAutoSpace::kNonHanIdeographMax;
                     });
}

}  // namespace

// static
void TextAutoSpace::ApplyIfNeeded(NGInlineItemsData& data,
                                  Vector<wtf_size_t>* offsets_out) {
  const String& text = data.text_content;
  if (text.Is8Bit()) {
    return;  // 8-bits never be `kIdeograph`. See `TextAutoSpaceTest`.
  }

  HeapVector<NGInlineItem>& items = data.items;
  if (UNLIKELY(items.empty())) {
    return;
  }

  // `RunSegmenterRange` is used to find where we can skip computing Unicode
  // properties. Compute them for the whole text content. It's pre-computed, but
  // packed in `NGInlineItemSegments` to save memory.
  NGInlineItemSegments::RunSegmenterRanges ranges;
  if (!data.segments) {
    const NGInlineItem& item0 = items.front();
    RunSegmenter::RunSegmenterRange range = item0.CreateRunSegmenterRange();
    if (!MaybeIdeograph(range.script, text)) {
      return;
    }
    range.end = text.length();
    ranges.push_back(range);
  } else {
    data.segments->ToRanges(ranges);
    if (std::none_of(ranges.begin(), ranges.end(),
                     [&text](const RunSegmenter::RunSegmenterRange& range) {
                       return MaybeIdeograph(
                           range.script, StringView(text, range.start,
                                                    range.end - range.start));
                     })) {
      return;
    }
  }
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
      last_type.reset();
      continue;
    }
    if (UNLIKELY(!style->IsHorizontalWritingMode()) &&
        UNLIKELY(style->GetTextOrientation() == ETextOrientation::kUpright)) {
      // Upright non-ideographic characters are `kOther`.
      // https://drafts.csswg.org/css-text-4/#non-ideographic-letters
      last_type = GetPrevType(text, item.EndOffset());
      if (last_type == kLetterOrNumeral) {
        last_type = kOther;
      }
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

      // If the range is known not to contain any `kIdeograph` characters, check
      // only the first and the last character.
      const wtf_size_t end_offset = std::min(range->end, item.EndOffset());
      DCHECK_LT(offset, end_offset);
      if (!MaybeIdeograph(range->script,
                          StringView(text, offset, end_offset - offset))) {
        if (last_type == kIdeograph) {
          const wtf_size_t saved_offset = offset;
          const CharType type = GetTypeAndNext(text, offset);
          DCHECK_NE(type, kIdeograph);
          if (type == kLetterOrNumeral) {
            offsets.push_back(saved_offset);
          }
          if (offset == end_offset) {
            last_type = type;
            continue;
          }
        }
        offset = end_offset;
        last_type.reset();
        continue;
      }

      // Compute the `CharType` for each character and check if spacings should
      // be inserted.
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
  CHECK(!text.Is8Bit());
  UChar32 ch;
  U16_NEXT(text.Characters16(), offset, text.length(), ch);
  return GetType(ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetPrevType(const String& text,
                                                   wtf_size_t offset) {
  DCHECK_GT(offset, 0u);
  CHECK(!text.Is8Bit());
  UChar32 last_ch;
  U16_PREV(text.Characters16(), 0, offset, last_ch);
  return GetType(last_ch);
}

// static
TextAutoSpace::CharType TextAutoSpace::GetType(UChar32 ch) {
  // This logic is based on:
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  const uint32_t gc_mask = U_GET_GC_MASK(ch);
  static_assert(kNonHanIdeographMin <= 0x30FF && 0x30FF <= kNonHanIdeographMax);
  if (ch >= kNonHanIdeographMin && ch <= 0x30FF && !(gc_mask & U_GC_P_MASK)) {
    return kIdeograph;
  }
  static_assert(kNonHanIdeographMin <= 0x31C0 && 0x31C0 <= kNonHanIdeographMax);
  if (ch >= 0x31C0 && ch <= kNonHanIdeographMax) {
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
