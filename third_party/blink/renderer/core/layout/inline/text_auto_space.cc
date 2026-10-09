// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/text_auto_space.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/check.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

namespace {

bool IsMacrolanguageChinese(const InlineNode& node) {
  const ComputedStyle& style = node.Style();
  const LayoutLocale* locale = style.GetFontDescription().Locale();
  return locale && locale->IsMacrolanguageChinese();
}

// Resolve UTR#59: East Asian Spacing "Conditional".
// https://unicode.org/reports/tr59/
inline EastAsianSpacingType ResolveConditional(EastAsianSpacingType& type,
                                               bool is_chinese) {
  if (type != EastAsianSpacingType::kConditional) {
    return type;
  }
  return is_chinese ? EastAsianSpacingType::kNarrow
                    : EastAsianSpacingType::kOther;
}

// Returns a copy of `text` where characters belonging to ruby annotations are
// replaced with a combining mark. The autospace loop skips Gc=Mark characters,
// so masked ruby annotation text is excluded from spacing decisions.
constexpr UChar kRubyAnnotationMaskCharacter =
    0x0300;  // COMBINING GRAVE ACCENT

String MaskRubyAnnotations(const String& text,
                           base::span<Member<InlineItem>> items) {
  StringBuffer<UChar> buffer(text.length());
  text.CopyTo(buffer.Span(), 0);
  unsigned ruby_text_nesting_level = 0;
  for (const Member<InlineItem>& item : items) {
    const bool is_ruby_annotation =
        ruby_text_nesting_level ||
        item->Type() == InlineItem::kOpenRubyColumn ||
        item->Type() == InlineItem::kCloseRubyColumn ||
        item->Type() == InlineItem::kRubyLinePlaceholder;
    if (is_ruby_annotation) {
      for (unsigned i = item->StartOffset(); i < item->EndOffset(); ++i) {
        buffer[i] = kRubyAnnotationMaskCharacter;
      }
    }
    if (item->Type() == InlineItem::kOpenTag ||
        item->Type() == InlineItem::kCloseTag) {
      const LayoutObject* object = item->GetLayoutObject();
      if (object && object->IsInlineRubyText()) {
        if (item->Type() == InlineItem::kOpenTag) {
          ++ruby_text_nesting_level;
        } else {
          DCHECK_GT(ruby_text_nesting_level, 0u);
          --ruby_text_nesting_level;
        }
      }
    }
  }
  return String::Adopt(buffer);
}

//
// This class keeps track of the `InlineItem` in sync with the text offset, in
// order to apply the East Asian Spacing as defined by the [UTR#59].
//
// The spacing is determined for each `InlineItem`. When the text offset
// advances to the next `InlineItem`, this class finalizes the spacings for the
// `InlineItem` and applies them.
//
// [UTR#59]: https://unicode.org/reports/tr59/
//
class SpacingApplier {
  STACK_ALLOCATED();

 public:
  // Convert to `base::span` because its `iterator` is safe.
  using InlineItemList = base::span<Member<InlineItem>>;

  SpacingApplier(wtf_size_t offset,
                 InlineItemList items,
                 TextAutoSpace::Callback* callback,
                 bool ignore_ruby_annotation)
      : ignore_ruby_annotation_(ignore_ruby_annotation),
        item_iter_(items.begin()),
        item_end_(items.end()),
        callback_for_testing_(callback) {
    item_ = *item_iter_;
    DidChangeItem();
    if (ignore_ruby_annotation_) {
      // Skip zero-length items (e.g. ruby column markers) and stop on the text
      // item that owns `offset`.
      while ((offset > item_end_offset_ || !item_->Length() ||
              (offset == item_end_offset_ &&
               item_->Type() != InlineItem::kText)) &&
             item_iter_ + 1 != item_end_) {
        AdvanceItem();
      }
    } else {
      while (offset > item_end_offset_) {
        AdvanceItem();
      }
    }
    if (!is_disabled_ && !IsOffsetDisabled(offset)) {
      InsertSpaceBefore(offset);
    }
  }

  wtf_size_t ItemEndOffset() const { return item_end_offset_; }
  bool IsDisabled() const { return is_disabled_; }
  bool IsOffsetDisabled(wtf_size_t offset) const {
    return is_last_disabled_ && offset == item_->StartOffset();
  }

  void InsertSpaceBefore(wtf_size_t offset) {
    DCHECK(item_);
    DCHECK_GE(offset, item_->StartOffset());
    DCHECK(!(is_disabled_ && offset < item_end_offset_));
    DCHECK(!IsOffsetDisabled(offset));

    // If the `offset` is for `item_`, buffer the `offset` and done.
    if (offset < item_end_offset_) {
      offsets_with_spacing_.push_back(OffsetWithSpacing{offset, 0});
      return;
    }

    // If the `offset` is at the boundary, add to the earlier item if it's LTR.
    const bool is_offset_for_last_item =
        offset == item_end_offset_ && IsLtr(item_->Direction());

    // Advance the item before applying because the next item may affect the
    // spacing.
    InlineItem* last_item = item_;
    const ComputedStyle* last_style = style_;
    while (offset >= item_end_offset_) {
      AdvanceItem();
    }
    if (is_disabled_ || IsOffsetDisabled(offset)) [[unlikely]] {
      ApplyIfNeeded(last_style, last_item);
    } else if (is_offset_for_last_item) {
      offsets_with_spacing_.push_back(OffsetWithSpacing{offset, 0});
      Apply(*last_style, *last_item);
    } else {
      ApplyIfNeeded(last_style, last_item);
      offsets_with_spacing_.push_back(OffsetWithSpacing{offset, 0});
    }
  }

  void ApplyIfNeeded() { ApplyIfNeeded(style_, item_); }

  void ApplyIfNeeded(const ComputedStyle* style, InlineItem* item) {
    if (!offsets_with_spacing_.empty()) [[unlikely]] {
      Apply(*style, *item);
    }
  }

  void Apply(const ComputedStyle& style, InlineItem& item) {
    const float spacing = style.GetFont()->TextAutoSpaceInlineSize();
    for (OffsetWithSpacing& offset_with_spacing : offsets_with_spacing_) {
      offset_with_spacing.spacing = spacing;
    }

    ShapeResult* shape_result = item.CloneTextShapeResult();
    DCHECK(shape_result);
    shape_result->ApplyTextAutoSpacing(offsets_with_spacing_);
    item.SetUnsafeToReuseShapeResult();
    if (callback_for_testing_) [[unlikely]] {
      callback_for_testing_->DidApply(offsets_with_spacing_);
    }
    offsets_with_spacing_.Shrink(0);
  }

 private:
  void AdvanceItem() {
    is_last_disabled_ = is_disabled_;
    item_ = *++item_iter_;
    DidChangeItem();
  }

  void DidChangeItem() {
    item_end_offset_ = item_->EndOffset();
    if (ignore_ruby_annotation_ &&
        (item_->Type() == InlineItem::kOpenRubyColumn ||
         item_->Type() == InlineItem::kCloseRubyColumn ||
         item_->Type() == InlineItem::kRubyLinePlaceholder)) [[unlikely]] {
      is_disabled_ = false;
      return;
    }
    if (!item_->Length()) [[unlikely]] {
      if (ignore_ruby_annotation_) {
        is_disabled_ = false;
      }
      return;
    }
    if (!item_->GetLayoutObject()) [[unlikely]] {
      is_disabled_ = true;
      return;
    }
    const ComputedStyle* style = item_->Style();
    DCHECK(style);
    if (style != style_) {
      style_ = style;
      is_disabled_by_style_ =
          style->TextAutospace() != ETextAutospace::kNormal ||
          // Upright non-ideographic characters are `kOther`.
          // https://drafts.csswg.org/css-text-4/#non-ideographic-letters
          style->GetFontDescription().Orientation() ==
              FontOrientation::kVerticalUpright;
    }
    is_disabled_ = is_disabled_by_style_ || !item_->TextShapeResult();
  }

  wtf_size_t item_end_offset_ = 0;
  bool is_disabled_ = false;
  bool is_disabled_by_style_ = false;
  bool is_last_disabled_ = false;
  const bool ignore_ruby_annotation_ = false;
  InlineItem* item_ = nullptr;
  const ComputedStyle* style_ = nullptr;
  InlineItemList::iterator item_iter_;
  const InlineItemList::iterator item_end_;
  Vector<OffsetWithSpacing, 16> offsets_with_spacing_;
  TextAutoSpace::Callback* callback_for_testing_ = nullptr;
};

}  // namespace

void TextAutoSpace::Apply(const InlineNode& node, InlineItemsData& data) {
  const String& text = data.text_content;
  DCHECK(!text.Is8Bit());
  DCHECK_EQ(text.length(), data.items.back()->EndOffset());
  DCHECK(MayApply());

  EastAsianSpacingType last_type = EastAsianSpacingType::kOther;
  bool is_last_wide = false;
  std::optional<SpacingApplier> applier_opt;
  std::optional<bool> is_chinese;

  // Use `MutableData()->HasRuby()` rather than `node.HasRuby()`: the latter
  // calls `Data()` which DCHECKs that inlines are not dirty, but this runs
  // during `ShapeText()` while inlines are still being prepared.
  const bool ignore_ruby_annotation =
      RuntimeEnabledFeatures::TextAutoSpaceIgnoreRubyAnnotationEnabled() &&
      node.MutableData()->HasRuby();
  const String iter_text =
      ignore_ruby_annotation ? MaskRubyAnnotations(text, data.items) : text;
  const CodePointIterator::Utf16 char_begin{iter_text.Span16()};
  const auto char_end = char_begin.EndForThis();
  for (auto char_iter = char_begin; char_iter != char_end;) {
    const UChar32 ch = *char_iter;
    if (Character::IsGcMark(ch)) [[unlikely]] {
      ++char_iter;
      continue;
    }
    EastAsianSpacingType type = unicode::GetEastAsianSpacingType(ch);
    const bool is_wide = type == EastAsianSpacingType::kWide;
    if (is_wide || is_last_wide) [[unlikely]] {
      // Resolve `kConditional` to `kNarrow` or `kOther`.
      if (!is_chinese) [[unlikely]] {
        is_chinese = IsMacrolanguageChinese(node);
      }
      type = ResolveConditional(type, *is_chinese);
      last_type = ResolveConditional(last_type, *is_chinese);

      const bool needs_space =
          (is_last_wide && type == EastAsianSpacingType::kNarrow) ||
          (is_wide && last_type == EastAsianSpacingType::kNarrow);
      if (needs_space) [[unlikely]] {
        const wtf_size_t offset = char_iter.DistanceByCodeUnits(char_begin);
        if (!applier_opt) {
          applier_opt.emplace(offset, data.items, callback_for_testing_,
                              ignore_ruby_annotation);
        } else {
          applier_opt->InsertSpaceBefore(offset);
        }

        SpacingApplier& applier = *applier_opt;
        if (applier.IsDisabled()) [[unlikely]] {
          const wtf_size_t item_end_offset = applier.ItemEndOffset();
          DCHECK_GE(item_end_offset, offset);
          char_iter.AdvanceByCodeUnits(item_end_offset - offset);
          last_type = EastAsianSpacingType::kOther;
          is_last_wide = false;
          continue;
        }
      }
    }
    last_type = type;
    is_last_wide = is_wide;
    ++char_iter;
  }

  // Apply the pending spacing for the last item if needed.
  if (applier_opt) {
    applier_opt->ApplyIfNeeded();
  }
}

}  // namespace blink
