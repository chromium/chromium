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

// Build the source text for ShapeResultSpacing. This needs special handling
// for text-combine items, ruby annotations, and hyphenations.
String BuildJustificationText(const String& text_content,
                              const InlineItemResults& results,
                              unsigned line_text_start_offset,
                              unsigned end_offset,
                              bool may_have_text_combine_or_ruby) {
  DCHECK(!RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled());
  if (results.empty()) {
    return String();
  }

  StringBuilder line_text_builder;
  if (may_have_text_combine_or_ruby) [[unlikely]] {
    for (const InlineItemResult& item_result : results) {
      if (item_result.StartOffset() >= end_offset) {
        break;
      }
      if (item_result.item->IsTextCombine()) {
        // To apply justification before and after the combined text, we put
        // ideographic character to increment |ShapeResultSpacing::
        // expansion_opportunity_count_| for legacy layout compatibility.
        // See "fast/writing-mode/text-combine-justify.html".
        // Note: The spec[1] says we should treat combined text as U+FFFC.
        // [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
        line_text_builder.Append(kTextCombineItemMarker);
        continue;
      }
      if (item_result.IsRubyColumn()) {
        // No need to add k*IsolateCharacter for kOpenRubyColumn if
        // is_continuation is true. It is not followed by `base_line` results.
        if (!item_result.ruby_column->is_continuation) {
          line_text_builder.Append(StringView(text_content,
                                              item_result.item->StartOffset(),
                                              item_result.item->Length()));
        }
        // Add the ruby-base results only if the ruby-base is wider than its
        // ruby-text. Shorter ruby-bases produces OBJECT REPLACEMENT CHARACTER,
        // and it is treated as a single Latin character.
        if (item_result.inline_size ==
            item_result.ruby_column->base_line.Width()) {
          const LineInfo& base_line = item_result.ruby_column->base_line;
          const InlineItemResults& base_results = base_line.Results();
          if (!base_results.empty()) {
            const unsigned base_end =
                std::min(base_results.back().EndOffset(), end_offset);
            line_text_builder.Append(BuildJustificationText(
                text_content, base_results, base_results.front().StartOffset(),
                base_end, base_line.MayHaveTextCombineOrRubyItem()));
          }
        } else {
          line_text_builder.Append(kBaseShorterRubyMarker);
        }
        continue;
      }
      line_text_builder.Append(StringView(text_content,
                                          item_result.StartOffset(),
                                          item_result.Length()));
    }
  } else {
    line_text_builder.Append(StringView(text_content,
                                        line_text_start_offset,
                                        end_offset - line_text_start_offset));
  }

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  DCHECK(!results.empty());
  const InlineItemResult& last_item_result = results.back();
  if (last_item_result.hyphen) {
    line_text_builder.Append(last_item_result.hyphen.Text());
  } else {
    // Remove the trailing \n.  See crbug.com/331729346.
    wtf_size_t text_length = line_text_builder.length();
    if (text_length > 0u &&
        line_text_builder[text_length - 1] == uchar::kLineFeed) {
      if (text_length == 1u) {
        return String();
      }
      line_text_builder.Resize(text_length - 1);
    }
  }

  return line_text_builder.ReleaseString();
}

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
  DCHECK(RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled());
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
float JustifyResults(const String& text_content,
                     const String& line_text,
                     unsigned line_text_start_offset,
                     ShapeResultSpacing& spacing,
                     InlineItemResults& results) {
  // If this flag is true, `line_text` is different from `text_content`, and
  // `line_text_start_offset` may be non-zero.
  const bool apply_line_text =
      !RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled();
  if (!apply_line_text) {
    DCHECK_EQ(line_text_start_offset, 0u);
    DCHECK(text_content.StartsWith(line_text));
  }
  float last_glyph_spacing = 0;
  for (wtf_size_t i = 0; i < results.size(); ++i) {
    InlineItemResult& item_result = results[i];
    if (item_result.has_only_pre_wrap_trailing_spaces) {
      break;
    }
    TextJustify method = GetTextJustify(item_result);
    if (item_result.shape_result) {
#if DCHECK_IS_ON()
      // This `if` is necessary for external/wpt/css/css-text/text-justify/
      // text-justify-and-trailing-spaces-*.html.
      if (apply_line_text && item_result.StartOffset() -
                                     line_text_start_offset +
                                     item_result.Length() <=
                                 line_text.length()) {
        DCHECK_EQ(StringView(text_content, item_result.StartOffset(),
                             item_result.Length()),
                  StringView(line_text,
                             item_result.StartOffset() - line_text_start_offset,
                             item_result.Length()));
      }
#endif
      ShapeResult* shape_result = item_result.shape_result->CreateShapeResult();
      DCHECK_GE(item_result.StartOffset(), line_text_start_offset);
      DCHECK_EQ(shape_result->NumCharacters(), item_result.Length());
      last_glyph_spacing = shape_result->ApplyExpansion(
          method, spacing,
          item_result.StartOffset() - line_text_start_offset -
              shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      if (item_result.is_hyphenated) [[unlikely]] {
        item_result.inline_size += item_result.hyphen.InlineSize();
      }
      item_result.shape_result = ShapeResultView::Create(shape_result);
    } else if (item_result.item->Type() == InlineItem::kAtomicInline) {
      last_glyph_spacing = 0;
      DCHECK_LE(line_text_start_offset, item_result.StartOffset());
      const unsigned line_text_offset =
          item_result.StartOffset() - line_text_start_offset;
      const auto [spacing_before, spacing_after] =
          apply_line_text
              ? spacing.ComputeExpansion(method, line_text_offset)
              : spacing.ComputeExpansion(
                    method, item_result.item->IsTextCombine()
                                ? kTextCombineItemMarker
                                : uchar::kObjectReplacementCharacter);
      if (item_result.item->IsTextCombine()) [[unlikely]] {
        // |spacing_before| is non-zero if this |item_result| is after
        // non-CJK character. See "text-combine-justify.html".
        if (apply_line_text) {
          DCHECK_EQ(kTextCombineItemMarker, line_text[line_text_offset]);
        }
        item_result.inline_size += spacing_before + spacing_after;
        item_result.spacing_before = spacing_before.To<LayoutUnit>();
      } else {
        if (apply_line_text) {
          DCHECK_EQ(uchar::kObjectReplacementCharacter,
                    line_text[line_text_offset]);
        }
        item_result.inline_size += spacing_before + spacing_after;
        if (RuntimeEnabledFeatures::CssTextJustifyEnabled()) {
          item_result.spacing_before = spacing_before.To<LayoutUnit>();
        } else {
          // |spacing_before| is non-zero only before CJK characters.
          DCHECK_EQ(spacing_before, TextRunLayoutUnit());
        }
      }
    } else if (item_result.IsRubyColumn()) {
      LineInfo& base_line = item_result.ruby_column->base_line;
      if (item_result.inline_size == base_line.Width()) {
        last_glyph_spacing =
            JustifyResults(text_content, line_text, line_text_start_offset,
                           spacing, *base_line.MutableResults());
        base_line.SetWidth(base_line.AvailableWidth(),
                           base_line.ComputeWidth());
        item_result.inline_size =
            std::max(item_result.inline_size, base_line.Width());
        item_result.ruby_column->last_base_glyph_spacing =
            LayoutUnit(last_glyph_spacing);
      } else {
        last_glyph_spacing = 0;
        unsigned offset = item_result.StartOffset() - line_text_start_offset;
        if (!item_result.ruby_column->is_continuation) {
          // Skip k*IsolateCharacter.
          offset += item_result.item->Length();
        }
        [[maybe_unused]] const auto [spacing_before, spacing_after] =
            apply_line_text
                ? spacing.ComputeExpansion(method, offset)
                : spacing.ComputeExpansion(method, kBaseShorterRubyMarker);
        if (RuntimeEnabledFeatures::CssTextJustifyEnabled()) {
          // A base-shorter ruby following a cursive script character can have
          // non-zero spacing_before. Currently we just shift the ruby by
          // spacing_before, and don't change the ruby annotation width.
          LayoutUnit spacing_before_layout = spacing_before.To<LayoutUnit>();
          item_result.inline_size += spacing_before_layout;
          item_result.spacing_before = spacing_before_layout;
        } else {
          DCHECK_EQ(spacing_before, TextRunLayoutUnit());
        }
        // ShapeResultSpacing doesn't ask for adding space to
        // kBaseShorterRubyMarker, which is OBJECT REPLACEMENT CHARACTER, and
        // asks for adding space to the next item instead.
        DCHECK_EQ(spacing_after, TextRunLayoutUnit());
      }
      if (apply_line_text && i + 1 < results.size()) {
        // Adjust line_text_start_offset because line_text is intermittent due
        // to ruby annotations.
        wtf_size_t next_start_offset = results[i + 1].StartOffset();
        if (item_result.inline_size == base_line.Width()) {
          // BuildJustificationText() didn't produce text for the annotation.
          line_text_start_offset +=
              next_start_offset - base_line.EndTextOffset();
        } else {
          // BuildJustificationText() produced only OBJECT REPLACEMENT
          // CHARACTER.
          line_text_start_offset +=
              next_start_offset - base_line.StartOffset() - 1;
        }
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

  // Note: |line_info->StartOffset()| can be different from
  // |ItemsResults[0].StartOffset()|, e.g. <b><input> <input></b> when
  // line break before space (leading space). See http://crbug.com/1240791
  const unsigned line_text_start_offset =
      RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled()
          ? 0u
          : line_info.Results().front().StartOffset();

  // Construct the line text to compute spacing for.
  String text_content = line_info.ItemsData().text_content;
  String line_text =
      RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled()
          ? (end_offset == line_info.EndTextOffset()
                 ? text_content
                 // Cut text_content at end_offset because
                 // ShapeResult::ApplyExpansion() calls ShapeResultSpacing with
                 // an index over end_offset.
                 : text_content.Substring(0, end_offset))
          : BuildJustificationText(text_content, line_info.Results(),
                                   line_text_start_offset, end_offset,
                                   line_info.MayHaveTextCombineOrRubyItem());
  if (line_text.empty()) {
    return std::nullopt;
  }

  // Compute the spacing to justify.
  ShapeResultSpacing spacing(line_text,
                             target == JustificationTarget::kSvgText);
  if (RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled()) {
    ShapeResultSpacing::ExpansionSetup spacing_setup(space, &spacing);
    SetupJustificationOpportunity(line_info.Results(), end_offset,
                                  line_info.BaseDirection(), spacing_setup);
  } else {
    spacing.SetExpansion(line_info.LineStyle().GetTextJustify(), space,
                         line_info.BaseDirection());
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
    if (RuntimeEnabledFeatures::JustifyWithoutLineTextEnabled()) {
      ShapeResultSpacing::ExpansionSetup spacing_setup(space - inset, &spacing);
      SetupJustificationOpportunity(line_info.Results(), end_offset,
                                    line_info.BaseDirection(), spacing_setup);
    } else {
      spacing.SetExpansion(line_info.LineStyle().GetTextJustify(),
                           space - inset, line_info.BaseDirection());
    }
  }

  if (results) {
    DCHECK_EQ(&line_info.Results(), results);
    JustifyResults(text_content, line_text, line_text_start_offset, spacing,
                   *results);
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
