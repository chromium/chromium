// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result_ruby_column.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

constexpr UChar kTextCombineItemMarker = 0x3042;  // U+3042 Hiragana Letter A
constexpr UChar kBaseShorterRubyMarker = uchar::kObjectReplacementCharacter;

void SetupJustificationOpportunity(
    const InlineItemResults& results,
    unsigned end_offset,
    TextDirection base_direction,
    ShapeResultSpacing::ExpansionSetup& spacing_setup);

TextJustify GetTextJustify(const InlineItemResult& item_result) {
  if (!item_result.item->GetLayoutObject()) {
    // We can't justify items without LayoutObject such as control characters
    // and anonymous ruby containers.
    return TextJustify::kNone;
  }
  return item_result.item->Style()->GetTextJustify();
}

NOINLINE void SetupItemJustificationOpportunity(
    const InlineItemResult& item_result,
    unsigned end_offset,
    TextDirection base_direction,
    ShapeResultSpacing::ExpansionSetup& spacing_setup) {
  if (item_result.StartOffset() >= end_offset) {
    return;
  }
  if (item_result.has_only_pre_wrap_trailing_spaces) {
    return;
  }
  TextJustify method = GetTextJustify(item_result);
  if (item_result.shape_result) {
    wtf_size_t start_index = item_result.shape_result->StartIndex();
    spacing_setup.CountOpportunities(
        method,
        StringView(spacing_setup.Spacing()->Text(), start_index,
                   std::min(item_result.shape_result->EndIndex(), end_offset) -
                       start_index),
        base_direction);
  } else if (item_result.IsRubyColumn()) {
    const LineInfo& base_line = item_result.ruby_column->base_line;
    if (item_result.inline_size > base_line.Width()) {
      // Don't justify base-shorter rubies.
      spacing_setup.CountOpportunities(method, kBaseShorterRubyMarker);
      return;
    }
    const InlineItemResults& base_results = base_line.Results();
    if (base_results.empty()) {
      return;
    }
    SetupJustificationOpportunity(
        base_results, std::min(base_results.back().EndOffset(), end_offset),
        base_line.BaseDirection(), spacing_setup);
  } else if (item_result.item->Type() == InlineItem::kAtomicInline) {
    spacing_setup.CountOpportunities(method,
                                     item_result.item->IsTextCombine()
                                         ? kTextCombineItemMarker
                                         : uchar::kObjectReplacementCharacter);
  }
}

// Setup a ShapeResultSpacing without serializing a line text.
void SetupJustificationOpportunity(
    const InlineItemResults& results,
    unsigned end_offset,
    TextDirection base_direction,
    ShapeResultSpacing::ExpansionSetup& spacing_setup) {
  if (results.empty()) {
    return;
  }
  if (IsRtl(base_direction)) {
    // Handle items in the reversed order. Justification is applied before
    // BiDi reorder.
    if (results.back().hyphen) {
      spacing_setup.CountOpportunities(GetTextJustify(results.back()),
                                       results.back().hyphen.Text(),
                                       base_direction);
    }
    for (const auto& item_result : base::Reversed(results)) {
      SetupItemJustificationOpportunity(item_result, end_offset, base_direction,
                                        spacing_setup);
    }
  } else {
    for (const auto& item_result : results) {
      SetupItemJustificationOpportunity(item_result, end_offset, base_direction,
                                        spacing_setup);
    }
    if (results.back().hyphen) {
      spacing_setup.CountOpportunities(GetTextJustify(results.back()),
                                       results.back().hyphen.Text(),
                                       base_direction);
    }
  }
}

// This function returns spacing amount on the right of the last glyph.
// It's zero if the last item is an atomic-inline.
float JustifyResults(ShapeResultSpacing& spacing, InlineItemResults& results) {
  float last_glyph_spacing = 0;
  for (wtf_size_t i = 0; i < results.size(); ++i) {
    InlineItemResult& item_result = results[i];
    if (item_result.has_only_pre_wrap_trailing_spaces) {
      break;
    }
    TextJustify method = GetTextJustify(item_result);
    if (item_result.shape_result) {
      ShapeResult* shape_result = item_result.shape_result->CreateShapeResult();
      DCHECK_EQ(shape_result->NumCharacters(), item_result.Length());
      last_glyph_spacing = shape_result->ApplyExpansion(
          method, spacing,
          item_result.StartOffset() - shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      if (item_result.is_hyphenated) [[unlikely]] {
        item_result.inline_size += item_result.hyphen.InlineSize();
      }
      item_result.shape_result = ShapeResultView::Create(shape_result);
    } else if (item_result.item->Type() == InlineItem::kAtomicInline) {
      last_glyph_spacing = 0;
      const auto [spacing_before, spacing_after] = spacing.ComputeExpansion(
          method, item_result.item->IsTextCombine()
                      ? kTextCombineItemMarker
                      : uchar::kObjectReplacementCharacter);
      if (item_result.item->IsTextCombine()) [[unlikely]] {
        // |spacing_before| is non-zero if this |item_result| is after
        // non-CJK character. See "text-combine-justify.html".
        item_result.inline_size += spacing_before + spacing_after;
        item_result.spacing_before = spacing_before.To<LayoutUnit>();
      } else {
        item_result.inline_size += spacing_before + spacing_after;
        item_result.spacing_before = spacing_before.To<LayoutUnit>();
      }
    } else if (item_result.IsRubyColumn()) {
      LineInfo& base_line = item_result.ruby_column->base_line;
      if (item_result.inline_size == base_line.Width()) {
        last_glyph_spacing =
            JustifyResults(spacing, *base_line.MutableResults());
        base_line.SetWidth(base_line.AvailableWidth(),
                           base_line.ComputeWidth());
        item_result.inline_size =
            std::max(item_result.inline_size, base_line.Width());
        item_result.ruby_column->last_base_glyph_spacing =
            LayoutUnit(last_glyph_spacing);
      } else {
        last_glyph_spacing = 0;
        [[maybe_unused]] const auto [spacing_before, spacing_after] =
            spacing.ComputeExpansion(method, kBaseShorterRubyMarker);
        // A base-shorter ruby following a cursive script character can have
        // non-zero spacing_before. Currently we just shift the ruby by
        // spacing_before, and don't change the ruby annotation width.
        LayoutUnit spacing_before_layout = spacing_before.To<LayoutUnit>();
        item_result.inline_size += spacing_before_layout;
        item_result.spacing_before = spacing_before_layout;
        // ShapeResultSpacing doesn't ask for adding space to
        // kBaseShorterRubyMarker, which is OBJECT REPLACEMENT CHARACTER, and
        // asks for adding space to the next item instead.
        DCHECK_EQ(spacing_after, TextRunLayoutUnit());
      }
    }
  }
  return last_glyph_spacing;
}

class ExpandableItemsFinder {
  STACK_ALLOCATED();

 public:
  void Find(base::span<LogicalLineItem> items) {
    for (auto& item : items) {
      if ((item.shape_result && item.shape_result->NumGlyphs() > 0) ||
          item.layout_result) {
        last_item_ = &item;
        if (!first_item_) {
          first_item_ = &item;
        }
      } else if (item.IsRubyLinePlaceholder()) {
        last_placeholder_item_ = &item;
        if (!first_placeholder_item_) {
          first_placeholder_item_ = &item;
        }
      }
    }
  }

  LogicalLineItem* FirstExpandable() const {
    return first_item_ ? first_item_ : first_placeholder_item_;
  }
  LogicalLineItem* LastExpandable() const {
    return last_item_ ? last_item_ : last_placeholder_item_;
  }

 private:
  // The first or the last LogicalLineItem which has a ShapeResult or is an
  // atomic-inline.
  LogicalLineItem* first_item_ = nullptr;
  LogicalLineItem* last_item_ = nullptr;
  // The first or the last kRubyLinePlaceholder.
  LogicalLineItem* first_placeholder_item_ = nullptr;
  LogicalLineItem* last_placeholder_item_ = nullptr;
};

void ApplyLeftAndRightExpansion(LayoutUnit left_expansion,
                                LayoutUnit right_expansion,
                                LogicalLineItem& item) {
  if (item.shape_result) {
    ShapeResult* shape_result = item.shape_result->CreateShapeResult();
    shape_result->ApplyLeadingExpansion(left_expansion);
    shape_result->ApplyTrailingExpansion(right_expansion);
    item.inline_size += left_expansion + right_expansion;
    item.shape_result = ShapeResultView::Create(shape_result);
  } else if (item.layout_result) {
    item.inline_size += left_expansion + right_expansion;
    item.rect.offset.inline_offset += left_expansion;
  } else {
    DCHECK(item.IsRubyLinePlaceholder());
    item.inline_size += left_expansion + right_expansion;
    item.margin_line_left += left_expansion;
  }
}

std::optional<LayoutUnit> ApplyJustificationInternal(
    LayoutUnit space,
    JustificationTarget target,
    const LineInfo& line_info,
    InlineItemResults* results) {
  // Empty lines should align to start.
  if (line_info.IsEmptyLine()) {
    return std::nullopt;
  }

  // Justify the end of visible text, ignoring preserved trailing spaces.
  unsigned end_offset = line_info.EndOffsetForJustify();

  // If this line overflows, fallback to 'text-align: start'.
  if (space <= 0) {
    return std::nullopt;
  }

  // Can't justify an empty string.
  if (end_offset == line_info.StartOffset()) {
    return std::nullopt;
  }

  // Construct the line text to compute spacing for.
  String text_content = line_info.ItemsData().text_content;
  String line_text = (end_offset == line_info.EndTextOffset()
                          ? text_content
                          // Cut text_content at end_offset because
                          // ShapeResult::ApplyExpansion() calls
                          // ShapeResultSpacing with an index over end_offset.
                          : text_content.substr(0, end_offset));
  if (line_text.empty()) {
    return std::nullopt;
  }

  // Compute the spacing to justify.
  ShapeResultSpacing spacing(line_text,
                             target == JustificationTarget::kSvgText);
  {
    ShapeResultSpacing::ExpansionSetup spacing_setup(space, &spacing);
    SetupJustificationOpportunity(line_info.Results(), end_offset,
                                  line_info.BaseDirection(), spacing_setup);
  }
  const bool is_ruby = target == JustificationTarget::kRubyText ||
                       target == JustificationTarget::kRubyBase;
  if (!spacing.HasExpansion()) {
    if (is_ruby) {
      return space / 2;
    }
    return std::nullopt;
  }

  LayoutUnit inset;
  if (is_ruby) {
    unsigned count = std::min(spacing.ExpansionOppotunityCount(),
                              static_cast<unsigned>(LayoutUnit::Max().Floor()));
    // Inset the ruby base/text by half the inter-ideograph expansion amount.
    inset = space / (count + 1);
    // For ruby text,  inset it by no more than a full-width ruby character on
    // each side.
    if (target == JustificationTarget::kRubyText) {
      inset = std::min(LayoutUnit(2 * line_info.LineStyle().FontSize()), inset);
    }
    ShapeResultSpacing::ExpansionSetup spacing_setup(space - inset, &spacing);
    SetupJustificationOpportunity(line_info.Results(), end_offset,
                                  line_info.BaseDirection(), spacing_setup);
  }

  if (results) {
    DCHECK_EQ(&line_info.Results(), results);
    JustifyResults(spacing, *results);
  }
  return inset / 2;
}

}  // namespace

std::optional<LayoutUnit> ApplyJustification(LayoutUnit space,
                                             JustificationTarget target,
                                             LineInfo* line_info) {
  return ApplyJustificationInternal(space, target, *line_info,
                                    line_info->MutableResults());
}

std::optional<LayoutUnit> ComputeRubyBaseInset(LayoutUnit space,
                                               const LineInfo& line_info) {
  DCHECK(line_info.IsRubyBase());
  return ApplyJustificationInternal(space, JustificationTarget::kRubyBase,
                                    line_info, nullptr);
}

bool ApplyLeftAndRightExpansion(LayoutUnit left_expansion,
                                LayoutUnit right_expansion,
                                base::span<LogicalLineItem> items) {
  if (!left_expansion && !right_expansion) {
    return true;
  }
  ExpandableItemsFinder finder;
  finder.Find(items);
  LogicalLineItem* first_expandable = finder.FirstExpandable();
  LogicalLineItem* last_expandable = finder.LastExpandable();
  if (first_expandable && last_expandable) {
    ApplyLeftAndRightExpansion(left_expansion, LayoutUnit(), *first_expandable);
    ApplyLeftAndRightExpansion(LayoutUnit(), right_expansion, *last_expandable);
    return true;
  }
  return false;
}

}  // namespace blink
