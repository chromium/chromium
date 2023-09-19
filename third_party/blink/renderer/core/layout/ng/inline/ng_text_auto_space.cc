// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_auto_space.h"

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
                  const NGInlineItem* current_item,
                  const ComputedStyle& style) {
    DCHECK(current_item->TextShapeResult());
    const float spacing = NGTextAutoSpace::GetSpacingWidth(style.GetFont());
    const wtf_size_t* offset = offsets.begin();
    if (!offsets.empty() && *offset == current_item->StartOffset()) {
      DCHECK(last_item_);
      // There would be spacing added to the previous item due to its last glyph
      // is next to `current_item`'s first glyph, since the two glyphs meet the
      // condition of adding spacing.
      // https://drafts.csswg.org/css-text-4/#propdef-text-autospace.
      offsets_with_spacing_.emplace_back(
          OffsetWithSpacing({.offset = *offset, .spacing = spacing}));
      ++offset;
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

    // TODO(https://crbug.com/1463890): Using `const_cast` does not look good,
    // consider refactoring.
    // TODO(https://crbug.com/1463890): Instead of recreating a new
    // `ShapeResult`, maybe we can reuse the `ShapeResult` and skip the applying
    // text-space step.
    ShapeResult* shape_result =
        const_cast<ShapeResult*>(last_item_->TextShapeResult());
    DCHECK(shape_result);
    shape_result->ApplyTextAutoSpacing(offsets_with_spacing_);
    NGInlineItem* item = const_cast<NGInlineItem*>(last_item_);
    item->SetUnsafeToReuseShapeResult();
  }

 private:
  const NGInlineItem* last_item_ = nullptr;
  // Stores the spacing (1/8 ic) and auto-space points's previous positions, for
  // the previous item.
  Vector<OffsetWithSpacing, 16> offsets_with_spacing_;
};

}  // namespace

void NGTextAutoSpace::Initialize(const NGInlineItemsData& data) {
  const HeapVector<NGInlineItem>& items = data.items;
  if (UNLIKELY(items.empty())) {
    return;
  }

  // `RunSegmenterRange` is used to find where we can skip computing Unicode
  // properties. Compute them for the whole text content. It's pre-computed, but
  // packed in `NGInlineItemSegments` to save memory.
  const String& text = data.text_content;
  if (!data.segments) {
    for (const NGInlineItem& item : items) {
      if (item.Type() != NGInlineItem::kText) {
        // Only `kText` has the data, see `NGInlineItem::SetSegmentData`.
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

void NGTextAutoSpace::Apply(NGInlineItemsData& data,
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
    DCHECK(offsets.empty());
    const ComputedStyle* style = item.Style();
    DCHECK(style);
    if (UNLIKELY(style->TextAutospace() != ETextAutospace::kNormal)) {
      applier.SetSpacing(offsets, &item, *style);
      last_type = kOther;
      continue;
    }
    if (UNLIKELY(!style->IsHorizontalWritingMode()) &&
        UNLIKELY(style->GetTextOrientation() == ETextOrientation::kUpright)) {
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
