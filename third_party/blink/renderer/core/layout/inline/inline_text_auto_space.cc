// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/inline/inline_text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"

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
                  const InlineItem* current_item,
                  const ComputedStyle& style) {
    DCHECK(current_item->TextShapeResult());
    const float spacing = TextAutoSpace::GetSpacingWidth(&style.GetFont());
    auto offset = offsets.begin();
    if (!offsets.empty() && *offset == current_item->StartOffset()) {
      DCHECK(last_item_);
      // If the previous item's direction is from the left to the right, it is
      // clear that the last run is the rightest run, so it is safe to add
      // spacing behind that.
      if (last_item_->Direction() == TextDirection::kLtr) {
        // There would be spacing added to the previous item due to its last
        // glyph is next to `current_item`'s first glyph, since the two glyphs
        // meet the condition of adding spacing.
        // https://drafts.csswg.org/css-text-4/#propdef-text-autospace.
        offsets_with_spacing_.emplace_back(
            OffsetWithSpacing({.offset = *offset, .spacing = spacing}));
        ++offset;
      } else {
        // This branch holds an assumption that RTL texts cannot be ideograph.
        // The assumption might be wrong, but should work for almost all cases.
        // Just do nothing in this case, and ShapeResult::ApplyTextAutoSpacing
        // will insert spacing as an position offset to `offset`'s glyph,
        // (instead of advance), to ensure spacing is always add to the correct
        // position regardless of where the line is broken.
      }
    }
    // Apply all pending spaces to the previous item.
    ApplyIfNeeded();
    offsets_with_spacing_.Shrink(0);

    // Update the previous item in prepare for the next iteration.
    last_item_ = current_item;
    for (; offset != offsets.end(); ++offset) {
      offsets_with_spacing_.emplace_back(
          OffsetWithSpacing({.offset = *offset, .spacing = spacing}));
    }
  }

  void ApplyIfNeeded() {
    if (offsets_with_spacing_.empty()) {
      return;  // Nothing to update.
    }
    DCHECK(last_item_);

    InlineItem* item = const_cast<InlineItem*>(last_item_);
    ShapeResult* shape_result = item->CloneTextShapeResult();
    DCHECK(shape_result);
    shape_result->ApplyTextAutoSpacing(offsets_with_spacing_);
    item->SetUnsafeToReuseShapeResult();
  }

 private:
  const InlineItem* last_item_ = nullptr;
  // Stores the spacing (1/8 ic) and auto-space points's previous positions, for
  // the previous item.
  Vector<OffsetWithSpacing, 16> offsets_with_spacing_;
};

}  // namespace

void InlineTextAutoSpace::Initialize(const InlineItemsData& data) {
  const HeapVector<InlineItem>& items = data.items;
  if (items.empty()) [[unlikely]] {
    return;
  }

  // `RunSegmenterRange` is used to find where we can skip computing Unicode
  // properties. Compute them for the whole text content. It's pre-computed, but
  // packed in `InlineItemSegments` to save memory.
  const String& text = data.text_content;
  if (!data.segments) {
    for (const InlineItem& item : items) {
      if (item.Type() != InlineItem::kText) {
        // Only `kText` has the data, see `InlineItem::SetSegmentData`.
        continue;
      }
      RunSegmenter::RunSegmenterRange range = item.CreateRunSegmenterRange();
      if (!MaybeIdeograph(range.script, text)) {
        return;
      }
      range.end = text.length();
      ranges_.push_back(range);
      break;
    }
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

void InlineTextAutoSpace::Apply(InlineItemsData& data,
                                Vector<wtf_size_t>* offsets_out) {
  const String& text = data.text_content;
  DCHECK(!text.Is8Bit());
  DCHECK_EQ(text.length(), ranges_.back().end);

  Vector<wtf_size_t, 16> offsets;
  CHECK(!ranges_.empty());
  auto range = ranges_.begin();
  std::optional<CharType> last_type = kOther;

  // The initial value does not matter, as the value is used for determine
  // whether to add spacing into the bound of two items.
  TextDirection last_direction = TextDirection::kLtr;
  SpacingApplier applier;
  for (const InlineItem& item : data.items) {
    if (item.Type() != InlineItem::kText) {
      if (item.Length()) {
        // If `item` has a length, e.g., inline-block, set the `last_type`.
        last_type = kOther;
      }
      continue;
    }
    if (!item.Length()) [[unlikely]] {
      // Empty items may not have `ShapeResult`. Skip it.
      continue;
    }
    DCHECK(offsets.empty());
    const ComputedStyle* style = item.Style();
    DCHECK(style);
    if (style->TextAutospace() != ETextAutospace::kNormal) [[unlikely]] {
      applier.SetSpacing(offsets, &item, *style);
      last_type = kOther;
      continue;
    }
    if (style->GetFontDescription().Orientation() ==
        FontOrientation::kVerticalUpright) [[unlikely]] {
      applier.SetSpacing(offsets, &item, *style);
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
          if (type == kLetterOrNumeral && [&] {
                if (last_direction == item.Direction()) [[likely]] {
                  return true;
                }
                return false;
              }()) {
            offsets.push_back(saved_offset);
          } else if (last_direction == TextDirection::kLtr &&
                     item.Direction() == TextDirection::kRtl) [[unlikely]] {
            // (1) Fall into the first case of RTL-LTR mixing text.
            // Given an index i which is the last character of item[a], add
            // spacing to the end of the last item if: str[i] is ideograph &&
            // item[a] is LTR && ItemOfCharIndex(i+1) is RTL.
            offsets.push_back(saved_offset);
          }
          if (offset == end_offset) {
            last_type = type;
            last_direction = item.Direction();
            continue;
          }
        }
        // When moving the offset to the end of this range, also update the item
        // direction as it is the last opportunity to know it.
        offset = end_offset;
        last_direction = item.Direction();
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
        if (((type == kIdeograph && last_type == kLetterOrNumeral) ||
             (last_type == kIdeograph && type == kLetterOrNumeral))) {
          if (last_direction == item.Direction()) {
            offsets.push_back(saved_offset);
          } else if (last_direction == TextDirection::kRtl &&
                     item.Direction() == TextDirection::kLtr) [[unlikely]] {
            // (2) Fall into the second case of RTL-LTR mixing text.
            // Given an index i which is the first character of item[a], add
            // spacing to the *offset* of i's glyph if: str[i] is ideograph &&
            // item[a] is LTR && ItemOfCharIndex(i-1) is RTL.
            offsets.push_back(saved_offset);
          }
        }
        last_type = type;
        last_direction = item.Direction();
      }
    } while (offset < item.EndOffset());

    if (!offsets_out) {
      applier.SetSpacing(offsets, &item, *style);
    } else {
      offsets_out->AppendVector(offsets);
    }
    offsets.Shrink(0);
  }
  // Apply the pending spacing for the last item if needed.
  applier.ApplyIfNeeded();
}

}  // namespace blink
