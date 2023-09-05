// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"

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

// `TextAutoSpace::ApplyIfNeeded` computes offsets to insert spacing *before*,
// but `ShapeResult` can handle spacing *after* a glyph. Due to this difference,
// when adding a spacing before the start offset of an item, the spacing
// should be added to the end of the previous item. This class keeps the
// previous item's `shape_result_` for this purpose.
class SpacingApplier {
 public:
  void SetSpacing(const Vector<wtf_size_t, 16>& offsets,
                  float spacing,
                  const NGInlineItem* current_item) {
    DCHECK(current_item->TextShapeResult());
    const wtf_size_t* offset = offsets.begin();
    bool has_adjacent_glyph = false;
    if (!offsets.empty() && *offset == current_item->StartOffset()) {
      DCHECK(last_item_);
      // There would be spacing added to the previous item due to its last glyph
      // is next to `current_item`'s first glyph, since the two glyphs meet the
      // condition of adding spacing.
      // https://drafts.csswg.org/css-text-4/#propdef-text-autospace.
      // In this case, when applying text spacing to `current_item`, also tells
      // it to set the first glyph unsafe to break before.
      has_adjacent_glyph = true;
      offsets_with_spacing_.emplace_back(
          OffsetWithSpacing({.offset = *offset - 1, .spacing = spacing}));
      ++offset;
    }
    // Apply all pending spaces to the previous item.
    ApplyIfNeeded();
    offsets_with_spacing_.Shrink(0);
    has_spacing_added_to_adjacent_glyph_ = has_adjacent_glyph;

    // Update the previous item in prepare for the next iteration.
    last_item_ = current_item;
    for (; offset != offsets.end(); ++offset) {
      offsets_with_spacing_.emplace_back(
          OffsetWithSpacing({.offset = *offset - 1, .spacing = spacing}));
    }
  }

  void ApplyIfNeeded() {
    // Nothing to update.
    if (offsets_with_spacing_.empty() &&
        !has_spacing_added_to_adjacent_glyph_) {
      return;
    }
    DCHECK(last_item_);

    // TODO(https://crbug.com/1463890): Using `const_cast` does not look good,
    // consider refactoring.
    // TODO(https://crbug.com/1463890): Instead of recreating a new
    // `ShapeResult`, maybe we can reuse the `ShapeResult` and skip the applying
    // text-space step.
    ShapeResult* shape_result =
        const_cast<ShapeResult*>(last_item_->TextShapeResult());
    DCHECK(shape_result);
    shape_result->ApplyTextAutoSpacing(has_spacing_added_to_adjacent_glyph_,
                                       offsets_with_spacing_);
    NGInlineItem* item = const_cast<NGInlineItem*>(last_item_);
    item->SetUnsafeToReuseShapeResult();
  }

 private:
  bool has_spacing_added_to_adjacent_glyph_ = false;
  const NGInlineItem* last_item_ = nullptr;
  // Stores the spacing (1/8 ic) and auto-space points's previous positions, for
  // the previous item.
  Vector<OffsetWithSpacing, 16> offsets_with_spacing_;
};

// https://drafts.csswg.org/css-text-4/#inter-script-spacing
float GetSpacingWidth(const ComputedStyle* style) {
  const SimpleFontData* font_data = style->GetFont().PrimaryFont();
  if (!font_data) {
    return style->ComputedFontSize() / 8;
  }
  return font_data->GetFontMetrics().IdeographicFullWidth().value_or(
             style->ComputedFontSize()) /
         8;
}

}  // namespace

void TextAutoSpace::Initialize(const NGInlineItemsData& data) {
  const HeapVector<NGInlineItem>& items = data.items;
  if (UNLIKELY(items.empty())) {
    return;
  }

  // `RunSegmenterRange` is used to find where we can skip computing Unicode
  // properties. Compute them for the whole text content. It's pre-computed, but
  // packed in `NGInlineItemSegments` to save memory.
  const String& text = data.text_content;
  if (!data.segments) {
    const NGInlineItem& item0 = items.front();
    RunSegmenter::RunSegmenterRange range = item0.CreateRunSegmenterRange();
    if (!MaybeIdeograph(range.script, text)) {
      return;
    }
    range.end = text.length();
    ranges_.push_back(range);
  } else {
    data.segments->ToRanges(ranges_);
    if (std::none_of(ranges_.begin(), ranges_.end(),
                     [&text](const RunSegmenter::RunSegmenterRange& range) {
                       return MaybeIdeograph(
                           range.script, StringView(text, range.start,
                                                    range.end - range.start));
                     })) {
      ranges_.clear();
      return;
    }
  }
}

void TextAutoSpace::Apply(NGInlineItemsData& data,
                          Vector<wtf_size_t>* offsets_out) {
  const String& text = data.text_content;
  DCHECK(!text.Is8Bit());
  DCHECK_EQ(text.length(), ranges_.back().end);

  Vector<wtf_size_t, 16> offsets;
  CHECK(!ranges_.empty());
  const RunSegmenter::RunSegmenterRange* range = ranges_.begin();
  absl::optional<CharType> last_type = kOther;
  SpacingApplier applier;
  for (const NGInlineItem& item : data.items) {
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
        CHECK_NE(range, ranges_.end());
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
      DCHECK(item.TextShapeResult());
      float spacing = GetSpacingWidth(style);
      applier.SetSpacing(offsets, spacing, &item);
    } else {
      offsets_out->AppendVector(offsets);
    }
    offsets.Shrink(0);
  }
  // Apply the pending spacing for the last item if needed.
  applier.ApplyIfNeeded();
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
