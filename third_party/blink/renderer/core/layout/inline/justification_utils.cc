// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"

#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

constexpr UChar kTextCombineItemMarker = 0x3042;  // U+3042 Hiragana Letter A

// Build the source text for ShapeResultSpacing. This needs special handling
// for text-combine items, ruby annotations, and hyphenations.
String BuildJustificationText(const String& text_content,
                              const InlineItemResults& results,
                              unsigned line_text_start_offset,
                              unsigned end_offset,
                              bool may_have_text_combine) {
  if (results.empty()) {
    return String();
  }

  StringBuilder line_text_builder;
  if (UNLIKELY(may_have_text_combine)) {
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
  } else if (RuntimeEnabledFeatures::TextAlignLastJustifyNewLineEnabled()) {
    // Remove the trailing \n.  See crbug.com/331729346.
    wtf_size_t text_length = line_text_builder.length();
    if (text_length > 0u &&
        line_text_builder[text_length - 1] == kNewlineCharacter) {
      if (text_length == 1u) {
        return String();
      }
      line_text_builder.Resize(text_length - 1);
    }
  }

  return line_text_builder.ReleaseString();
}

void JustifyResults(const String& line_text,
                    unsigned line_text_start_offset,
                    ShapeResultSpacing<String>& spacing,
                    InlineItemResults& results) {
  for (InlineItemResult& item_result : results) {
    if (item_result.has_only_pre_wrap_trailing_spaces) {
      break;
    }
    if (item_result.shape_result) {
      ShapeResult* shape_result = item_result.shape_result->CreateShapeResult();
      DCHECK_GE(item_result.StartOffset(), line_text_start_offset);
      DCHECK_EQ(shape_result->NumCharacters(), item_result.Length());
      shape_result->ApplySpacing(spacing, item_result.StartOffset() -
                                              line_text_start_offset -
                                              shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      if (UNLIKELY(item_result.is_hyphenated)) {
        item_result.inline_size += item_result.hyphen.InlineSize();
      }
      item_result.shape_result = ShapeResultView::Create(shape_result);
    } else if (item_result.item->Type() == InlineItem::kAtomicInline) {
      float spacing_before = 0.0f;
      DCHECK_LE(line_text_start_offset, item_result.StartOffset());
      const unsigned line_text_offset =
          item_result.StartOffset() - line_text_start_offset;
      const float spacing_after =
          spacing.ComputeSpacing(line_text_offset, spacing_before);
      if (UNLIKELY(item_result.item->IsTextCombine())) {
        // |spacing_before| is non-zero if this |item_result| is after
        // non-CJK character. See "text-combine-justify.html".
        DCHECK_EQ(kTextCombineItemMarker, line_text[line_text_offset]);
        item_result.inline_size += spacing_after;
        item_result.spacing_before = LayoutUnit(spacing_before);
      } else {
        DCHECK_EQ(kObjectReplacementCharacter, line_text[line_text_offset]);
        item_result.inline_size += spacing_after;
        // |spacing_before| is non-zero only before CJK characters.
        DCHECK_EQ(spacing_before, 0.0f);
      }
    }
  }
}

}  // namespace

std::optional<LayoutUnit> ApplyJustification(LayoutUnit space,
                                             JustificationTarget target,
                                             LineInfo* line_info) {
  // Empty lines should align to start.
  if (line_info->IsEmptyLine()) {
    return std::nullopt;
  }

  // Justify the end of visible text, ignoring preserved trailing spaces.
  unsigned end_offset = line_info->EndOffsetForJustify();

  // If this line overflows, fallback to 'text-align: start'.
  if (space <= 0) {
    return std::nullopt;
  }

  // Can't justify an empty string.
  if (end_offset == line_info->StartOffset()) {
    return std::nullopt;
  }

  // Note: |line_info->StartOffset()| can be different from
  // |ItemsResults[0].StartOffset()|, e.g. <b><input> <input></b> when
  // line break before space (leading space). See http://crbug.com/1240791
  const unsigned line_text_start_offset =
      line_info->Results().front().StartOffset();

  // Construct the line text to compute spacing for.
  String line_text = BuildJustificationText(
      line_info->ItemsData().text_content, line_info->Results(),
      line_text_start_offset, end_offset, line_info->MayHaveTextCombineItem());
  if (line_text.empty()) {
    return std::nullopt;
  }

  // Compute the spacing to justify.
  ShapeResultSpacing<String> spacing(line_text,
                                     target == JustificationTarget::kSvgText);
  spacing.SetExpansion(space, line_info->BaseDirection());
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
      inset =
          std::min(LayoutUnit(2 * line_info->LineStyle().FontSize()), inset);
    }
    spacing.SetExpansion(space - inset, line_info->BaseDirection());
  }

  JustifyResults(line_text, line_text_start_offset, spacing,
                 *line_info->MutableResults());
  return inset / 2;
}

}  // namespace blink
