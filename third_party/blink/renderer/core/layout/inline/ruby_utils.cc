// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"

#include <tuple>

#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result_ruby_column.h"
#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_container.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/inline/used_font.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_height.h"
#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr float kHanKerningHalf = 0.5f;
constexpr float kHanKerningQuarter = 0.25f;

inline bool IsSpaceForRubyOverhang(UChar32 ch) {
  return unicode::Category(ch) == unicode::CharCategory::kSeparator_Space;
}

std::tuple<LayoutUnit, LayoutUnit> AdjustTextOverUnderOffsetsForEmHeight(
    LayoutUnit over,
    LayoutUnit under,
    FontBaseline font_baseline,
    const UsedFont& used_font,
    const ShapeResultView& shape_view) {
  DCHECK_LE(over, under);
  if (!used_font.PrimaryFont()) {
    return std::make_pair(over, under);
  }
  const LayoutUnit line_height = under - over;
  const float paint_scale = used_font.ScalingFactor();
  const LayoutUnit primary_ascent = used_font.FixedAscent(font_baseline);
  const LayoutUnit primary_descent = line_height - primary_ascent;

  HeapHashSet<Member<const SimpleFontData>> run_fonts = shape_view.UsedFonts();
  ClearCollectionScope clear_scope(&run_fonts);

  const LayoutUnit kNoDiff = LayoutUnit::Max();
  LayoutUnit over_diff = kNoDiff;
  LayoutUnit under_diff = kNoDiff;
  for (const auto& run_font : run_fonts) {
    FontHeight normalized_height =
        run_font->NormalizedTypoAscentAndDescent(font_baseline);
    normalized_height.ascent *= paint_scale;
    normalized_height.descent *= paint_scale;
    // Floor() is better than Round().  We should not subtract pixels larger
    // than |primary_ascent - em_box.ascent|.
    const LayoutUnit current_over_diff(
        (primary_ascent - normalized_height.ascent)
            .ClampNegativeToZero()
            .Floor());
    const LayoutUnit current_under_diff(
        (primary_descent - normalized_height.descent)
            .ClampNegativeToZero()
            .Floor());
    over_diff = std::min(over_diff, current_over_diff);
    under_diff = std::min(under_diff, current_under_diff);
  }
  if (over_diff == kNoDiff)
    over_diff = LayoutUnit();
  if (under_diff == kNoDiff)
    under_diff = LayoutUnit();
  return std::make_tuple(over + over_diff, under - under_diff);
}

FontHeight ComputeEmHeight(const LogicalLineItem& line_item) {
  if (const auto& shape_result_view = line_item.shape_result) {
    const ComputedStyle* style = line_item.Style();
    const SimpleFontData* primary_font_data = style->GetFont()->PrimaryFont();
    if (!primary_font_data) {
      return FontHeight();
    }
    const auto font_baseline = style->GetFontBaseline();
    const FontHeight primary_height =
        primary_font_data->GetFontMetrics().GetFloatFontHeight(font_baseline);
    FontHeight result_height;

    HeapHashSet<Member<const SimpleFontData>> run_fonts =
        shape_result_view->UsedFonts();
    ClearCollectionScope clear_scope(&run_fonts);

    for (const auto& run_font : run_fonts) {
      result_height.Unite(
          run_font->NormalizedTypoAscentAndDescent(font_baseline));
    }
    result_height.ascent = std::min(LayoutUnit(result_height.ascent.Ceil()),
                                    primary_height.ascent);
    result_height.descent = std::min(LayoutUnit(result_height.descent.Ceil()),
                                     primary_height.descent);
    result_height.Move(line_item.rect.offset.block_offset +
                       primary_height.ascent);
    return result_height;
  }
  if (const auto& layout_result = line_item.layout_result) {
    const auto& fragment = layout_result->GetPhysicalFragment();
    const auto& style = fragment.Style();
    const LayoutUnit inline_size =
        LogicalFragment(style.GetWritingDirection(), fragment)
            .Size()
            .inline_size;
    if (inline_size && fragment.IsAtomicInline()) {
      LogicalRect overflow =
          WritingModeConverter(
              {ToLineWritingMode(style.GetWritingMode()), style.Direction()},
              fragment.Size())
              .ToLogical(
                  To<PhysicalBoxFragment>(fragment).ScrollableOverflow());
      // Assume 0 is the baseline.  BlockOffset() is always negative.
      return FontHeight(-overflow.offset.block_offset - line_item.BlockOffset(),
                        overflow.BlockEndOffset() + line_item.BlockOffset());
    }
  }
  return FontHeight();
}

bool IsFullWidthGlyph(const ShapeResult& text_shape_result,
                      const SimpleFontData& font,
                      const String& text_content,
                      wtf_size_t text_offset) {
  UChar32 character = text_content.CodePointAtOrZero(text_offset);
  wtf_size_t code_unit_length = U16_LENGTH(character);
  const float end_position = text_shape_result.PositionForOffset(
      text_offset + code_unit_length - text_shape_result.StartIndex());
  const float start_position = text_shape_result.PositionForOffset(
      text_offset - text_shape_result.StartIndex());
  const float glyph_width = std::abs(end_position - start_position);
  const float advance_min = font.PlatformData().size() * .9;
  return glyph_width > advance_min;
}

bool CanTrimHanKerningOpen(const ShapeResult& shape_result,
                           const ComputedStyle& style,
                           const String& text_content,
                           wtf_size_t text_offset) {
  UChar32 character = text_content.CodePointAtOrZero(text_offset);
  if (!Character::MaybeHanKerningOpen(character)) {
    return false;
  }
  const SimpleFontData* primary_font = style.GetFont()->PrimaryFont();
  if (!primary_font || !IsFullWidthGlyph(shape_result, *primary_font,
                                         text_content, text_offset)) {
    return false;
  }
  const FontDescription& font_description = style.GetFontDescription();
  HanKerning::FontData font_data = primary_font->HanKerningData(
      font_description.LocaleOrDefault(), style.IsHorizontalTypographicMode());
  if (text_offset == 0 ||
      font_description.GetTextSpacingTrim() == TextSpacingTrim::kSpaceAll) {
    return true;
  }

  HanKerningCharType type = HanKerning::GetCharType(character, font_data);
  UChar32 previous_character =
      text_content.CodePointAtAndPrevious(0u, text_offset);
  HanKerningCharType previous_type =
      HanKerning::GetCharType(previous_character, font_data);
  return !HanKerning::ShouldKern(type, previous_type);
}

bool CanTrimHanKerningClose(const ShapeResult& shape_result,
                            const ComputedStyle& style,
                            const String& text_content,
                            wtf_size_t text_offset) {
  wtf_size_t next_index = text_offset;
  UChar32 character = text_content.CodePointAtAndNext(next_index);
  if (!Character::MaybeHanKerningClose(character)) {
    return false;
  }
  const SimpleFontData* primary_font = style.GetFont()->PrimaryFont();
  if (!primary_font || !IsFullWidthGlyph(shape_result, *primary_font,
                                         text_content, text_offset)) {
    return false;
  }
  const FontDescription& font_description = style.GetFontDescription();
  HanKerning::FontData font_data = primary_font->HanKerningData(
      font_description.LocaleOrDefault(), style.IsHorizontalTypographicMode());
  if (next_index >= text_content.length() ||
      font_description.GetTextSpacingTrim() == TextSpacingTrim::kSpaceAll) {
    return true;
  }

  HanKerningCharType type = HanKerning::GetCharType(character, font_data);
  UChar32 next_character = text_content.CodePointAtOrZero(next_index);
  HanKerningCharType next_type =
      HanKerning::GetCharType(next_character, font_data);
  return !HanKerning::ShouldKernLast(/*type=*/next_type, /*last_type=*/type);
}

wtf_size_t FindPreviousRubyIndex(const InlineItemResults& items,
                                 wtf_size_t index) {
  DCHECK_LT(0u, index);
  wtf_size_t previous_ruby_index = index - 1;
  while (!items[previous_ruby_index].IsRubyColumn()) {
    const auto type = items[previous_ruby_index].item->Type();
    if (type != InlineItem::kOpenTag && type != InlineItem::kCloseTag &&
        type != InlineItem::kCloseRubyColumn &&
        type != InlineItem::kOpenRubyColumn &&
        type != InlineItem::kRubyLinePlaceholder &&
        // For consecutive spaces
        type != InlineItem::kText && type != InlineItem::kControl) {
      return kNotFound;
    }
    if (previous_ruby_index-- == 0) {
      return kNotFound;
    }
  }
  return previous_ruby_index;
}

}  // anonymous namespace

RubyItemIndexes ParseRubyInInlineItems(const InlineItems& items,
                                       wtf_size_t start_item_index) {
  CHECK_LT(start_item_index, items.size());
  CHECK_EQ(items[start_item_index]->Type(), InlineItem::kOpenRubyColumn);
  RubyItemIndexes indexes = {start_item_index, kNotFound, kNotFound, kNotFound};
  for (wtf_size_t i = start_item_index + 1; i < items.size(); ++i) {
    const InlineItem& item = *items[i];
    if (item.Type() == InlineItem::kCloseRubyColumn) {
      if (indexes.base_end == kNotFound) {
        DCHECK_EQ(indexes.annotation_start, kNotFound);
        indexes.base_end = i;
      } else {
        DCHECK_NE(indexes.annotation_start, kNotFound);
      }
      indexes.column_end = i;
      return indexes;
    }
    if (item.Type() == InlineItem::kOpenTag &&
        item.GetLayoutObject()->IsInlineRubyText()) {
      DCHECK_EQ(indexes.base_end, kNotFound);
      DCHECK_EQ(indexes.annotation_start, kNotFound);
      indexes.base_end = i;
      indexes.annotation_start = i;
    } else if (item.Type() == InlineItem::kOpenRubyColumn) {
      RubyItemIndexes sub_indexes = ParseRubyInInlineItems(items, i);
      i = sub_indexes.column_end;
    }
  }
  NOTREACHED();
}

AnnotationOverhang GetOverhang(
    LayoutUnit ruby_size,
    const LineInfo& base_line,
    const HeapVector<LineInfo, 1> annotation_line_list,
    const LineInfo& line_info,
    wtf_size_t ruby_index) {
  AnnotationOverhang overhang;
  const ComputedStyle& base_line_style = base_line.LineStyle();

  ERubyAlign ruby_align = base_line_style.RubyAlign();
  switch (ruby_align) {
    case ERubyAlign::kSpaceBetween:
      return overhang;
    case ERubyAlign::kStart:
    case ERubyAlign::kSpaceAround:
    case ERubyAlign::kCenter:
      break;
  }
  LayoutUnit half_width_of_annotation_font;
  for (const auto& annotation_line : annotation_line_list) {
    if (annotation_line.Width() == ruby_size) {
      half_width_of_annotation_font =
          LayoutUnit(annotation_line.LineStyle().FontSize() / 2);
      break;
    }
  }
  if (half_width_of_annotation_font == LayoutUnit()) {
    return overhang;
  }
  LayoutUnit space = ruby_size - base_line.Width();
  if (space <= LayoutUnit()) {
    return overhang;
  }

  std::optional<LayoutUnit> ruby_base_inset =
      ComputeRubyBaseInset(space, base_line);
  if (base_line_style.RubyOverhang() != ERubyOverhang::kSpaces) {
    if (ruby_align == ERubyAlign::kStart) {
      overhang.end = std::min(space, half_width_of_annotation_font);
      return overhang;
    }
    if (!ruby_base_inset.has_value()) {
      return overhang;
    }
    overhang.start =
        std::min(ruby_base_inset.value(), half_width_of_annotation_font);
    overhang.end = overhang.start;

    return overhang;
  }

  // Handle 'ruby-overhang: spaces' from here.

  if (ruby_index == 0) {
    // Handle an edge case for crbug.com/520181855
    if (!ruby_base_inset.has_value()) {
      return overhang;
    }
    overhang.end = (ruby_align == ERubyAlign::kStart)
                       ? std::min(space, half_width_of_annotation_font)
                       : ruby_base_inset.value();
    return overhang;
  }

  const InlineItemResults& items = line_info.Results();
  // Find a previous item other than kOpenTag/kCloseTag.
  // Searching items in the logical order doesn't work well with bidi
  // reordering. However, it's difficult to compute overhang after bidi
  // reordering because it affects line breaking.
  wtf_size_t previous_index = ruby_index - 1;
  while ((items[previous_index].item->Type() == InlineItem::kOpenTag ||
          items[previous_index].item->Type() == InlineItem::kCloseTag) &&
         previous_index > 0) {
    --previous_index;
  }

  // Find a previous ruby column to avoid overlapping annotations.
  LayoutUnit previous_ruby_overhang_end;
  wtf_size_t previous_ruby_index = FindPreviousRubyIndex(items, ruby_index);
  if (previous_ruby_index != kNotFound) {
    previous_ruby_overhang_end =
        items[previous_ruby_index].ruby_column->end_overhang;
  }

  LayoutUnit space_overhang;
  LayoutUnit previous_item_inline_size_sum;
  wtf_size_t space_start_offset;
  const String& text_content = line_info.ItemsData().text_content;
  while (true) {
    const InlineItemResult& previous_item = items[previous_index];
    previous_item_inline_size_sum += previous_item.inline_size;
    // Handled space separators and control items.
    // - Preserved white space
    // - No-break space (U+00A0)
    // - Other space separators
    const TextOffsetRange& previous_item_text_offset =
        previous_item.TextOffset();
    if (previous_item.item->Type() == InlineItem::kControl) {
      space_overhang += previous_item.inline_size;
      space_start_offset = previous_item_text_offset.start;
    } else if (previous_item.item->Type() == InlineItem::kText) {
      space_start_offset = previous_item_text_offset.end;
      while (space_start_offset > previous_item_text_offset.start) {
        wtf_size_t previous_space_start_offset = space_start_offset;
        UChar32 previous_character = text_content.CodePointAtAndPrevious(
            /*start_offset=*/previous_item_text_offset.start,
            /*i=*/previous_space_start_offset);
        if (!IsSpaceForRubyOverhang(previous_character)) {
          break;
        }
        space_start_offset = previous_space_start_offset;
      }

      if (space_start_offset == previous_item_text_offset.end) {
        // There are no space characters.
        break;
      }
      if (space_start_offset == previous_item_text_offset.start) {
        // All characters are spaces.
        space_overhang += previous_item.inline_size;
      } else if (const ShapeResult* shape_result =
                     previous_item.item->TextShapeResult()) {
        // Some characters are spaces.
        ShapeResultView* space_shape_result = ShapeResultView::Create(
            shape_result, space_start_offset, previous_item_text_offset.end);
        space_overhang += LayoutUnit(space_shape_result->Width());
        break;
      } else {
        // Some characters are spaces and it doesn't have |TextShapeResult|.
        break;
      }
    } else {
      // There are no control and text.
      break;
    }

    if (previous_index-- == 0) {
      previous_index = kNotFound;
      break;
    }
  }

  LayoutUnit kerning_overhang;
  if (previous_index != kNotFound) {
    const InlineItemResult& previous_item = items[previous_index];
    if (previous_item.item->Type() == InlineItem::kText &&
        space_start_offset > previous_item.TextOffset().start) {
      const ComputedStyle* previous_item_style = previous_item.item->Style();
      wtf_size_t last_non_space_index = space_start_offset;
      UChar32 last_non_space_character = text_content.CodePointAtAndPrevious(
          /*start_offset=*/0u, last_non_space_index);
      LayoutUnit font_size(previous_item_style->FontSize());
      if (CanTrimHanKerningClose(*previous_item.item->TextShapeResult(),
                                 *previous_item_style, text_content,
                                 last_non_space_index)) {
        kerning_overhang = LayoutUnit(font_size * kHanKerningHalf);
      } else if (Character::MaybeHanKerningMiddle(last_non_space_character)) {
        kerning_overhang = LayoutUnit(font_size * kHanKerningQuarter);
      }
    }
  }

  if (!ruby_base_inset.has_value()) {
    return overhang;
  }
  if (ruby_align == ERubyAlign::kStart) {
    overhang.end = std::min(space, ruby_base_inset.value() * 2);
    return overhang;
  }
  overhang.start =
      std::min(ruby_base_inset.value(), space_overhang + kerning_overhang);
  overhang.start =
      std::min(previous_item_inline_size_sum - previous_ruby_overhang_end,
               overhang.start);
  overhang.end = ruby_base_inset.value();
  return overhang;
}

AnnotationOverhang GetOverhang(const InlineItemResult& item,
                               const LineInfo& line_info,
                               wtf_size_t ruby_index) {
  DCHECK(item.IsRubyColumn());
  const InlineItemResultRubyColumn& column = *item.ruby_column;
  return GetOverhang(item.inline_size, column.base_line,
                     column.annotation_line_list, line_info, ruby_index);
}

bool CanApplyStartOverhang(const LineInfo& line_info,
                           wtf_size_t ruby_index,
                           const ComputedStyle& ruby_style,
                           LayoutUnit& start_overhang) {
  if (start_overhang <= LayoutUnit())
    return false;
  const InlineItemResults& items = line_info.Results();
  // Requires at least the ruby item and the previous item.
  if (ruby_index < 1) {
    return false;
  }
  // Find a previous item other than kOpenTag/kCloseTag.
  // Searching items in the logical order doesn't work well with bidi
  // reordering. However, it's difficult to compute overhang after bidi
  // reordering because it affects line breaking.
  wtf_size_t previous_index = ruby_index - 1;
  while ((items[previous_index].item->Type() == InlineItem::kOpenTag ||
          items[previous_index].item->Type() == InlineItem::kCloseTag) &&
         previous_index > 0) {
    --previous_index;
  }
  const InlineItemResult& previous_item = items[previous_index];
  if (ruby_style.RubyOverhang() == ERubyOverhang::kSpaces &&
      previous_item.item->Type() == InlineItem::kControl) {
    return true;
  }
  if (previous_item.item->Type() != InlineItem::kText) {
    return false;
  }
  const ComputedStyle& previous_item_style = *previous_item.item->Style();
  if (previous_item_style.FontSize() > ruby_style.FontSize()) {
    return false;
  }
  if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled() &&
      previous_item_style.GetTextEmphasisMark() != TextEmphasisMark::kNone &&
      ruby_style.GetRubyPosition() ==
          previous_item_style.GetTextEmphasisLineLogicalSide()) {
    return false;
  }
  if (ruby_style.RubyOverhang() == ERubyOverhang::kSpaces) {
    const String& text_content = line_info.ItemsData().text_content;
    wtf_size_t previous_character_index = previous_item.TextOffset().end;
    UChar32 previous_character = text_content.CodePointAtAndPrevious(
        /*start_offset=*/previous_item.TextOffset().start,
        /*i=*/previous_character_index);
    if (!IsSpaceForRubyOverhang(previous_character) &&
        !CanTrimHanKerningClose(*previous_item.item->TextShapeResult(),
                                previous_item_style, text_content,
                                previous_character_index) &&
        !Character::MaybeHanKerningMiddle(previous_character)) {
      return false;
    }
    return true;
  }
  start_overhang = std::min(start_overhang, previous_item.inline_size / 2);
  return true;
}

LayoutUnit CommitPendingEndOverhang(const InlineItem& text_item,
                                    const ShapeResult& shape_result,
                                    LineInfo* line_info) {
  DCHECK(line_info);
  InlineItemResults* items = line_info->MutableResults();
  if (items->size() < 1U) {
    return LayoutUnit();
  }
  DCHECK(text_item.Type() == InlineItem::kText ||
         text_item.Type() == InlineItem::kControl);
  wtf_size_t i = items->size() - 1;
  bool has_previous_text_or_control = false;
  while (!(*items)[i].IsRubyColumn()) {
    const auto type = (*items)[i].item->Type();
    if (type != InlineItem::kOpenTag && type != InlineItem::kCloseTag &&
        type != InlineItem::kCloseRubyColumn &&
        type != InlineItem::kOpenRubyColumn &&
        type != InlineItem::kRubyLinePlaceholder &&
        // For consecutive spaces
        type != InlineItem::kText && type != InlineItem::kControl) {
      return LayoutUnit();
    }
    if (type == InlineItem::kText || type == InlineItem::kControl) {
      has_previous_text_or_control = true;
    }
    if (i-- == 0) {
      return LayoutUnit();
    }
  }
  InlineItemResult& column_item = (*items)[i];
  if (column_item.pending_end_overhang <= LayoutUnit()) {
    return LayoutUnit();
  }
  const ComputedStyle& column_base_line_style =
      column_item.ruby_column->base_line.LineStyle();
  if (column_base_line_style.RubyOverhang() != ERubyOverhang::kSpaces &&
      (has_previous_text_or_control ||
       text_item.Type() == InlineItem::kControl)) {
    return LayoutUnit();
  }
  if (column_base_line_style.FontSize() < text_item.Style()->FontSize()) {
    return LayoutUnit();
  }
  if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled() &&
      text_item.Style()->GetTextEmphasisMark() != TextEmphasisMark::kNone &&
      column_base_line_style.GetRubyPosition() ==
          text_item.Style()->GetTextEmphasisLineLogicalSide()) {
    return LayoutUnit();
  }

  InlineItemResult& end_item =
      column_item.ruby_column->base_line.MutableResults()->back();

  if (column_base_line_style.RubyOverhang() != ERubyOverhang::kSpaces) {
    // Ideally we should refer to inline_size of |text_item| instead of the
    // width of the InlineItem's ShapeResult. However it's impossible to compute
    // inline_size of |text_item| before calling BreakText(), and BreakText()
    // requires precise |position_| which takes |end_overhang| into account.
    LayoutUnit text_inline_size = LayoutUnit(shape_result.Width());
    LayoutUnit end_overhang =
        std::min(column_item.pending_end_overhang, text_inline_size / 2);
    end_item.margins.inline_end -= end_overhang;
    column_item.pending_end_overhang = LayoutUnit();
    return end_overhang;
  }

  // Handle 'ruby-overhang: spaces'
  const LayoutUnit item_inline_size(shape_result.Width());
  LayoutUnit end_overhang;
  bool is_exhausted = false;
  if (text_item.Type() == InlineItem::kControl) {
    end_overhang = std::min(item_inline_size, column_item.pending_end_overhang);
    column_item.pending_end_overhang -= end_overhang;
  } else {
    DCHECK_EQ(text_item.Type(), InlineItem::kText);
    const String& text_content = line_info->ItemsData().text_content;
    wtf_size_t space_end = text_item.StartOffset();
    while (space_end < text_item.EndOffset()) {
      wtf_size_t next_space_end = space_end;
      UChar32 character = text_content.CodePointAtAndNext(next_space_end);
      if (!IsSpaceForRubyOverhang(character)) {
        break;
      }
      space_end = next_space_end;
    }

    LayoutUnit space_overhang;
    if (space_end == text_item.EndOffset()) {
      space_overhang = item_inline_size;
    } else {
      ShapeResultView* space_view = ShapeResultView::Create(
          &shape_result, text_item.StartOffset(), space_end);
      space_overhang = LayoutUnit(space_view->Width());
    }

    LayoutUnit kerning_overhang;
    if (space_end < text_item.EndOffset()) {
      LayoutUnit font_size(text_item.Style()->FontSize());
      if (CanTrimHanKerningOpen(shape_result, *text_item.Style(), text_content,
                                space_end)) {
        kerning_overhang = LayoutUnit(font_size * kHanKerningHalf);
      } else if (Character::MaybeHanKerningMiddle(
                     text_content.CodePointAtOrZero(space_end))) {
        kerning_overhang = LayoutUnit(font_size * kHanKerningQuarter);
      }
    }

    end_overhang = std::min(column_item.pending_end_overhang,
                            space_overhang + kerning_overhang);

    if (space_end < text_item.EndOffset()) {
      is_exhausted = true;
    } else {
      column_item.pending_end_overhang -= end_overhang;
    }
  }

  column_item.ruby_column->end_overhang += end_overhang;
  if (is_exhausted) {
    column_item.pending_end_overhang = LayoutUnit();
  }
  end_item.margins.inline_end -= end_overhang;
  return end_overhang;
}

std::pair<LayoutUnit, LayoutUnit> ApplyRubyAlign(LayoutUnit available_line_size,
                                                 bool on_start_edge,
                                                 bool on_end_edge,
                                                 LineInfo& line_info) {
  DCHECK(line_info.IsRubyBase() || line_info.IsRubyText());
  LayoutUnit space = available_line_size - line_info.WidthForAlignment();
  if (space <= LayoutUnit()) {
    return {LayoutUnit(), LayoutUnit()};
  }

  ERubyAlign ruby_align = line_info.LineStyle().RubyAlign();
  ETextAlign text_align = line_info.TextAlign();
  switch (ruby_align) {
    case ERubyAlign::kSpaceAround:
      // We respect to the text-align value as ever if ruby-align is the
      // initial value.
      break;
    case ERubyAlign::kSpaceBetween:
      on_start_edge = true;
      on_end_edge = true;
      text_align = ETextAlign::kJustify;
      break;
    case ERubyAlign::kStart:
      return IsLtr(line_info.BaseDirection())
                 ? std::make_pair(LayoutUnit(), space)
                 : std::make_pair(space, LayoutUnit());
    case ERubyAlign::kCenter:
      return {space / 2, space / 2};
  }

  // Handle `space-around` and `space-between`.
  if (text_align == ETextAlign::kJustify) {
    JustificationTarget target;
    if (on_start_edge && on_end_edge) {
      // Switch to `space-between` if this needs to align both edges.
      target = JustificationTarget::kNormal;
    } else if (line_info.IsRubyBase()) {
      target = JustificationTarget::kRubyBase;
    } else {
      DCHECK(line_info.IsRubyText());
      target = JustificationTarget::kRubyText;
    }
    std::optional<LayoutUnit> inset =
        ApplyJustification(space, target, &line_info);
    // https://drafts.csswg.org/css-ruby/#line-edge
    if (inset) {
      if (on_start_edge && !on_end_edge) {
        return {LayoutUnit(), *inset * 2};
      }
      if (!on_start_edge && on_end_edge) {
        return {*inset * 2, LayoutUnit()};
      }
      return {*inset, *inset};
    }
    if (on_start_edge && !on_end_edge) {
      return {LayoutUnit(), space};
    }
    if (!on_start_edge && on_end_edge) {
      return {space, LayoutUnit()};
    }
    return {space / 2, space / 2};
  }

  bool is_ltr = IsLtr(line_info.BaseDirection());
  if (text_align == ETextAlign::kStart) {
    text_align = is_ltr ? ETextAlign::kLeft : ETextAlign::kRight;
  } else if (text_align == ETextAlign::kEnd) {
    text_align = is_ltr ? ETextAlign::kRight : ETextAlign::kLeft;
  }
  switch (text_align) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      return {LayoutUnit(), space};

    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      return {space, LayoutUnit()};

    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      return {space / 2, space / 2};

    case ETextAlign::kStart:
    case ETextAlign::kEnd:
    case ETextAlign::kJustify:
    case ETextAlign::kMatchParent:
      NOTREACHED();
  }
  return {LayoutUnit(), LayoutUnit()};
}

AnnotationMetrics ComputeAnnotationOverflow(
    const LogicalLineItems& logical_line,
    const FontHeight& line_box_metrics,
    LayoutUnit line_font_size,
    std::optional<FontHeight> annotation_metrics) {
  // Min/max position of content and annotations, ignoring line-height.
  // They are distance from the line box top.
  const LayoutUnit line_over;
  LayoutUnit content_over = line_over + line_box_metrics.ascent;
  LayoutUnit content_under = content_over;

  bool has_over_annotation = false;
  bool has_under_annotation = false;
  bool has_over_emphasis = false;
  bool has_under_emphasis = false;

  const LayoutUnit line_under = line_over + line_box_metrics.LineHeight();
  LayoutUnit over_emphasis;
  LayoutUnit under_emphasis;
  // TODO(crbug.com/324111880): This loop can be replaced with
  // ComputeLogicalLineEmHeight() after enabling RubyLineBreakable flag.
  for (const LogicalLineItem& item : logical_line) {
    if (!item.HasInFlowFragment())
      continue;
    if (item.IsControl() || item.IsRubyLinePlaceholder()) {
      continue;
    }
    UsedFont used_font = item.GetUsedFont();
    LayoutUnit text_box_over = line_box_metrics.ascent + item.BlockOffset();
    LayoutUnit text_box_under = line_box_metrics.ascent + item.BlockEndOffset();
    LayoutUnit item_over = text_box_over;
    LayoutUnit item_under = text_box_under;
    if (item.shape_result) {
      if (const auto* style = item.Style()) {
        std::tie(item_over, item_under) = AdjustTextOverUnderOffsetsForEmHeight(
            item_over, item_under, style->GetFontBaseline(), used_font,
            *item.shape_result);
      }
    } else {
      if (item.IsAtomicInline() && !item.IsInitialLetterBox()) {
        item_under = ComputeEmHeight(item).LineHeight();
      } else if (item.IsInlineBox()) {
        continue;
      }
    }

    if (const auto* style = item.Style()) {
      if (style->GetTextEmphasisMark() != TextEmphasisMark::kNone) {
        if (RuntimeEnabledFeatures::TextEmphasisAsRubyEnabled()) {
          const auto emphasis_mark_height =
              InlineBoxState::ComputeEmphasisMarkOutsets(*style, used_font)
                  .LineHeight();
          if (style->GetTextEmphasisLineLogicalSide() ==
              LineLogicalSide::kOver) {
            item_over = text_box_over -
                        (emphasis_mark_height + item.annotation_metrics.ascent);
            has_over_emphasis = true;
          } else {
            item_under = text_box_under + emphasis_mark_height +
                         item.annotation_metrics.descent;
            has_under_emphasis = true;
          }
        } else if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled()) {
          const auto emphasis_mark_height =
              InlineBoxState::ComputeEmphasisMarkOutsets(*style, used_font)
                  .LineHeight();
          if (style->GetTextEmphasisLineLogicalSide() ==
              LineLogicalSide::kOver) {
            over_emphasis = std::max(emphasis_mark_height, over_emphasis);
          } else {
            under_emphasis = std::max(emphasis_mark_height, under_emphasis);
          }
        } else {
          if (style->GetTextEmphasisLineLogicalSide() ==
              LineLogicalSide::kOver) {
            over_emphasis = LayoutUnit(1);
          } else {
            under_emphasis = LayoutUnit(1);
          }
        }
      }
    }
    content_over = std::min(content_over, item_over);
    content_under = std::max(content_under, item_under);
  }

  if (annotation_metrics) {
    if (annotation_metrics->ascent) {
      LayoutUnit item_over =
          line_box_metrics.ascent - annotation_metrics->ascent;
      if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled()) {
        item_over -= over_emphasis;
      }
      content_over = std::min(content_over, item_over);
      has_over_annotation = true;
    }
    if (annotation_metrics->descent) {
      LayoutUnit item_under =
          line_box_metrics.ascent + annotation_metrics->descent;
      if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled()) {
        item_under += under_emphasis;
      }
      content_under = std::max(content_under, item_under);
      has_under_annotation = true;
    }
  }

  // Probably this is an empty line. We should secure font-size space.
  if (content_under - content_over < line_font_size) {
    LayoutUnit half_leading =
        (line_box_metrics.LineHeight() - line_font_size) / 2;
    half_leading = half_leading.ClampNegativeToZero();
    content_over = line_over + half_leading;
    content_under = line_under - half_leading;
  }

  if (!RuntimeEnabledFeatures::TextEmphasisAsRubyEnabled()) {
    // Don't provide annotation space if text-emphasis exists.
    // TODO(layout-dev): If the text-emphasis is in [line_over, line_under],
    // this line can provide annotation space.
    if (over_emphasis > LayoutUnit()) {
      content_over = std::min(content_over, line_over);
    }
    if (under_emphasis > LayoutUnit()) {
      content_under = std::max(content_under, line_under);
    }
  }

  // With some fonts, text fragment sizes can exceed line-height.
  // We'd like to set overflow only if we have annotations.
  // This affects fast/ruby/line-height.html on macOS.
  if (content_over < line_over && !has_over_annotation && !has_over_emphasis) {
    content_over = line_over;
  }
  if (content_under > line_under && !has_under_annotation &&
      !has_under_emphasis) {
    content_under = line_under;
  }

  return {(line_over - content_over).ClampNegativeToZero(),
          (content_under - line_under).ClampNegativeToZero(),
          (content_over - line_over).ClampNegativeToZero(),
          (line_under - content_under).ClampNegativeToZero()};
}

// ================================================================

void UpdateRubyColumnInlinePositions(
    const LogicalLineItems& line_items,
    LayoutUnit inline_size,
    HeapVector<Member<LogicalRubyColumn>>& column_list) {
  for (auto& column : column_list) {
    LayoutUnit inline_offset;
    wtf_size_t start_index = column->start_index;
    if (start_index < line_items.size()) {
      inline_offset = line_items[start_index].rect.offset.inline_offset;
    } else if (start_index == line_items.size()) {
      if (line_items.size() > 0) {
        const LogicalLineItem& last_item = line_items[start_index - 1];
        inline_offset = last_item.rect.offset.inline_offset +
                        last_item.rect.InlineEndOffset();
      } else {
        inline_offset = inline_size;
      }
    } else {
      NOTREACHED() << " LogicalLineItems::size()=" << line_items.size()
                   << " LogicalRubyColumn::start_index=" << start_index;
    }
    // TODO(crbug.com/324111880): Handle overhang.
    column->annotation_items->MoveInInlineDirection(inline_offset);
    column->state_stack.MoveBoxDataInInlineDirection(inline_offset);
    UpdateRubyColumnInlinePositions(*column->annotation_items, inline_size,
                                    column->RubyColumnList());
  }
}

void SetTextEmphasisAnnotationMetrics(
    const HeapVector<Member<LogicalRubyColumn>>& column_list,
    LogicalLineItems& line_box) {
  for (wtf_size_t idx = 0; idx < line_box.size(); ++idx) {
    LogicalLineItem& item = line_box[idx];
    if (!item.IsItemType(InlineItem::kText)) {
      continue;
    }
    const ComputedStyle* style = item.Style();
    if (!style || style->GetTextEmphasisMark() == TextEmphasisMark::kNone) {
      continue;
    }

    const LogicalRubyColumn* matched_column = nullptr;
    for (const auto& column : column_list) {
      if (column->start_index <= idx &&
          idx < column->start_index + column->size) {
        matched_column = column.Get();
        break;
      }
    }

    const FontBaseline font_baseline = style->GetFontBaseline();
    UsedFont used_font = item.GetUsedFont();
    const LayoutUnit over_initial = -used_font.FixedAscent(font_baseline);
    const LayoutUnit under_initial = used_font.FixedDescent(font_baseline);

    LayoutUnit over = over_initial;
    LayoutUnit under = under_initial;
    if (item.shape_result) {
      std::tie(over, under) = AdjustTextOverUnderOffsetsForEmHeight(
          over, under, font_baseline, used_font, *item.shape_result);
    }

    if (matched_column) {
      if (matched_column->layout_annotation_metrics.ascent) {
        over = -matched_column->layout_annotation_metrics.ascent;
      }
      if (matched_column->layout_annotation_metrics.descent) {
        under = matched_column->layout_annotation_metrics.descent;
      }
    }
    item.annotation_metrics = {over_initial - over, under - under_initial};
  }

  for (const auto& column : column_list) {
    if (column->annotation_items) {
      SetTextEmphasisAnnotationMetrics(column->RubyColumnList(),
                                       *column->annotation_items);
    }
  }
}

// ================================================================

namespace {

FontHeight ComputeLogicalLineEmHeight(const LogicalLineItems& line_items) {
  FontHeight height;
  for (const auto& item : line_items) {
    height.Unite(ComputeEmHeight(item));
  }
  return height;
}

FontHeight ComputeLogicalLineEmHeight(const LogicalLineItems& line_items,
                                      const Vector<wtf_size_t>& index_list) {
  if (index_list.empty()) {
    return ComputeLogicalLineEmHeight(line_items);
  }
  FontHeight height;
  for (const auto index : index_list) {
    height.Unite(ComputeEmHeight(line_items[index]));
  }
  return height;
}

// Computes the maximum emphasis mark heights (outsets) among all items in
// `line_items` that have a text-emphasis mark applied.
FontHeight ComputeEmphasisHeights(const LogicalLineItems& line_items) {
  FontHeight heights;
  for (const auto& item : line_items) {
    if (!item.HasInFlowFragment()) {
      continue;
    }
    const auto* style = item.Style();
    if (!style || style->GetTextEmphasisMark() == TextEmphasisMark::kNone) {
      continue;
    }
    heights.Unite(
        InlineBoxState::ComputeEmphasisMarkOutsets(*style, item.GetUsedFont()));
  }
  return heights;
}

// Computes the maximum emphasis mark heights (outsets) among the items in
// `line_items` that are explicitly specified by `index_list`.
FontHeight ComputeEmphasisHeights(const LogicalLineItems& line_items,
                                  base::span<const wtf_size_t> index_list) {
  FontHeight heights;
  for (wtf_size_t idx : index_list) {
    if (idx >= line_items.size()) {
      continue;
    }
    const auto& item = line_items[idx];
    if (!item.HasInFlowFragment()) {
      continue;
    }
    const auto* style = item.Style();
    if (!style || style->GetTextEmphasisMark() == TextEmphasisMark::kNone) {
      continue;
    }
    heights.Unite(
        InlineBoxState::ComputeEmphasisMarkOutsets(*style, item.GetUsedFont()));
  }
  return heights;
}

}  // namespace

RubyBlockPositionCalculator::RubyBlockPositionCalculator() = default;

RubyBlockPositionCalculator& RubyBlockPositionCalculator::GroupLines(
    const HeapVector<Member<LogicalRubyColumn>>& column_list) {
  HandleRubyLine(EnsureRubyLine(RubyLevel()), column_list);
  return *this;
}

FontHeight RubyBlockPositionCalculator::HandleRubyLine(
    const RubyLine& current_ruby_line,
    const HeapVector<Member<LogicalRubyColumn>>& column_list) {
  if (column_list.empty()) {
    return FontHeight();
  }

  auto create_level_and_update_depth =
      [](const RubyLevel& current, const AnnotationDepth& current_depth) {
        AnnotationDepth depth = current_depth;
        RubyLevel new_level;
        new_level.reserve(current.size() + 1);
        new_level.append_range(current);
        if (depth.column->ruby_position == RubyPosition::kUnder) {
          new_level.push_back(--depth.under_depth);
        } else {
          new_level.push_back(++depth.over_depth);
        }
        return std::make_pair(new_level, depth);
      };

  HeapVector<AnnotationDepth, 1> depth_stack;
  const RubyLevel& current_level = current_ruby_line.Level();
  FontHeight max_annotation_metrics;
  for (wtf_size_t i = 0; i < column_list.size(); ++i) {
    // Push depth values with zeros.  Actual depths are fixed on closing this
    // ruby column.
    depth_stack.push_back(AnnotationDepth{column_list[i].Get(), 0, 0});

    // Close this ruby column and parent ruby columns which are not parents of
    // the next column.
    auto should_close_column = [=]() {
      const LogicalRubyColumn* column = depth_stack.back().column;
      return i + 1 >= column_list.size() ||
             column->EndIndex() <= column_list[i + 1]->start_index;
    };
    FontHeight annotation_metrics;
    while (!depth_stack.empty() && should_close_column()) {
      const auto [annotation_level, closing_depth] =
          create_level_and_update_depth(current_level, depth_stack.back());
      RubyLine& annotation_line = EnsureRubyLine(annotation_level);
      annotation_line.Append(*closing_depth.column);
      FontHeight closing_metrics = HandleRubyLine(
          annotation_line, closing_depth.column->RubyColumnList());
      annotation_line.MaybeRecordBaseIndexes(*closing_depth.column);

      LayoutUnit annotation_height = LayoutUnit();
      if (closing_depth.column->annotation_items) {
        annotation_height =
            ComputeLogicalLineEmHeight(*closing_depth.column->annotation_items)
                .LineHeight();
      }
      if (closing_depth.column->ruby_position == RubyPosition::kOver) {
        closing_metrics.ascent += annotation_height;
      } else {
        closing_metrics.descent += annotation_height;
      }
      closing_depth.column->annotation_metrics = closing_metrics;
      annotation_metrics = closing_metrics;
      max_annotation_metrics.Unite(closing_metrics);

      depth_stack.pop_back();
      if (!depth_stack.empty()) {
        AnnotationDepth& parent_depth = depth_stack.back();
        parent_depth.over_depth =
            std::max(parent_depth.over_depth, closing_depth.over_depth);
        parent_depth.under_depth =
            std::min(parent_depth.under_depth, closing_depth.under_depth);
      }
    }
  }
  CHECK(depth_stack.empty());
  return max_annotation_metrics;
}

RubyBlockPositionCalculator::RubyLine&
RubyBlockPositionCalculator::EnsureRubyLine(const RubyLevel& level) {
  // We do linear search because ruby_lines_ typically has only two items.
  auto it =
      std::ranges::find_if(ruby_lines_, [&](const Member<RubyLine>& line) {
        return std::ranges::equal(line->Level(), level);
      });
  if (it != ruby_lines_.end()) {
    return **it;
  }
  ruby_lines_.push_back(MakeGarbageCollected<RubyLine>(level));
  return *ruby_lines_.back();
}

RubyBlockPositionCalculator& RubyBlockPositionCalculator::PlaceLines(
    const LogicalLineItems& base_line_items,
    const FontHeight& line_box_metrics) {
  DCHECK(!ruby_lines_.empty()) << "This must be called after GroupLines().";
  annotation_metrics_ = FontHeight();

  if (RuntimeEnabledFeatures::TreeRubyPlacementEnabled()) {
    RubyLine* root = BuildTree();
    CHECK(root);
    FontHeight total_subtree_metrics =
        ComputeRelativeOffsets(*root, base_line_items, line_box_metrics);
    ComputeOffsetsFromBase(*root, LayoutUnit());

    if (!root->OverChildren().empty()) {
      annotation_metrics_.ascent = total_subtree_metrics.ascent;
    }
    if (!root->UnderChildren().empty()) {
      annotation_metrics_.descent = total_subtree_metrics.descent;
    }
    return *this;
  }

  // Sort `ruby_lines` from the lowest to the highest.
  std::ranges::sort(ruby_lines_, [](const Member<RubyLine>& line1,
                                    const Member<RubyLine>& line2) {
    return *line1 < *line2;
  });

  auto base_iterator = std::ranges::find_if(
      ruby_lines_,
      [](const Member<RubyLine>& line) { return line->Level().empty(); });
  CHECK_NE(base_iterator, ruby_lines_.end());

  // Place "under" annotations from the base level to the lowest one.
  if (base_iterator != ruby_lines_.begin()) {
    auto first_under_iterator = std::ranges::find_if(
        ruby_lines_.begin(), base_iterator,
        [](const Member<RubyLine>& line) { return line->IsFirstUnderLevel(); });
    FontHeight em_height = ComputeLogicalLineEmHeight(
        base_line_items, (**first_under_iterator).BaseIndexList());
    if (!em_height.LineHeight()) {
      em_height = line_box_metrics;
    }
    LayoutUnit offset = em_height.descent;
    auto lines_before_base =
        base::span(ruby_lines_)
            .first(base::checked_cast<size_t>(
                std::distance(ruby_lines_.begin(), base_iterator)));
    for (auto& ruby_line : base::Reversed(lines_before_base)) {
      FontHeight metrics = ruby_line->UpdateMetrics();
      offset += metrics.ascent;
      ruby_line->MoveInBlockDirection(offset);
      ruby_line->SetOffset(offset);
      offset += metrics.descent;
    }
    annotation_metrics_.descent = offset;
  }

  // Place "over" annotations from the base level to the highest one.
  if (std::next(base_iterator) != ruby_lines_.end()) {
    auto first_over_iterator = std::ranges::find_if(
        base_iterator, ruby_lines_.end(),
        [](const Member<RubyLine>& line) { return line->IsFirstOverLevel(); });
    FontHeight em_height = ComputeLogicalLineEmHeight(
        base_line_items, (**first_over_iterator).BaseIndexList());
    if (!em_height.LineHeight()) {
      em_height = line_box_metrics;
    }
    LayoutUnit offset = -em_height.ascent;
    for (auto& ruby_line :
         base::span(ruby_lines_)
             .last(base::checked_cast<size_t>(
                 std::distance(base_iterator, ruby_lines_.end()) - 1))) {
      FontHeight metrics = ruby_line->UpdateMetrics();
      offset -= metrics.descent;
      ruby_line->MoveInBlockDirection(offset);
      ruby_line->SetOffset(offset);
      offset -= metrics.ascent;
    }
    annotation_metrics_.ascent = -offset;
  }
  return *this;
}

RubyBlockPositionCalculator& RubyBlockPositionCalculator::AddLinesTo(
    LogicalLineContainer& line_container) {
  DCHECK(!annotation_metrics_.IsEmpty())
      << "This must be called after PlaceLines().";
  for (const auto& ruby_line : ruby_lines_) {
    ruby_line->AddLinesTo(line_container);
  }
  return *this;
}

FontHeight RubyBlockPositionCalculator::AnnotationMetrics() const {
  DCHECK(!annotation_metrics_.IsEmpty())
      << "This must be called after PlaceLines().";
  return annotation_metrics_;
}

void RubyBlockPositionCalculator::UpdateColumnLayoutAnnotationMetrics(
    const HeapVector<Member<LogicalRubyColumn>>& column_list) const {
  for (const auto& column : column_list) {
    UpdateColumnLayoutAnnotationMetrics(*column, LayoutUnit());
  }
}

void RubyBlockPositionCalculator::UpdateColumnLayoutAnnotationMetrics(
    LogicalRubyColumn& column,
    LayoutUnit base_offset) const {
  const RubyLine* associated_line = nullptr;
  if (column.annotation_items) {
    for (const auto& line : ruby_lines_) {
      if (line->ContainsColumn(&column)) {
        associated_line = line.Get();
        break;
      }
    }
  }

  LayoutUnit child_base_offset = base_offset;
  if (associated_line) {
    child_base_offset = associated_line->Offset();
  }

  for (const auto& sub_column : column.RubyColumnList()) {
    UpdateColumnLayoutAnnotationMetrics(*sub_column, child_base_offset);
  }

  LayoutUnit min_offset = LayoutUnit::Max();
  LayoutUnit max_offset = LayoutUnit::Min();

  AccumulateColumnOffsets(column, min_offset, max_offset);

  FontHeight new_metrics;
  if (min_offset != LayoutUnit::Max()) {
    new_metrics.ascent = base_offset - min_offset;
  }
  if (max_offset != LayoutUnit::Min()) {
    new_metrics.descent = max_offset - base_offset;
  }
  column.layout_annotation_metrics = new_metrics;
}

void RubyBlockPositionCalculator::AccumulateColumnOffsets(
    const LogicalRubyColumn& column,
    LayoutUnit& min_offset,
    LayoutUnit& max_offset) const {
  if (column.annotation_items) {
    const RubyLine* associated_line = nullptr;
    for (const auto& line : ruby_lines_) {
      if (line->ContainsColumn(&column)) {
        associated_line = line.Get();
        break;
      }
    }
    if (associated_line) {
      const RubyLevel& level = associated_line->Level();
      FontHeight metrics = ComputeLogicalLineEmHeight(*column.annotation_items);
      FontHeight emphasis_metrics =
          ComputeEmphasisHeights(*column.annotation_items);
      metrics.ascent += emphasis_metrics.ascent;
      metrics.descent += emphasis_metrics.descent;

      if (!level.empty() && level[0] > 0) {
        LayoutUnit start = -metrics.ascent;
        min_offset = std::min(min_offset, start);
      } else if (!level.empty() && level[0] < 0) {
        LayoutUnit end = metrics.descent;
        max_offset = std::max(max_offset, end);
      }
    }
  }

  for (const auto& sub_column :
       const_cast<LogicalRubyColumn&>(column).RubyColumnList()) {
    AccumulateColumnOffsets(*sub_column, min_offset, max_offset);
  }
}

RubyBlockPositionCalculator::RubyLine*
RubyBlockPositionCalculator::BuildTree() {
  RubyLine* root = nullptr;
  for (auto& line : ruby_lines_) {
    if (line->IsBaseLevel()) {
      root = line.Get();
    }
  }

  for (auto& line : ruby_lines_) {
    if (line->IsBaseLevel()) {
      continue;
    }
    const RubyLevel& level = line->Level();
    DCHECK(!level.empty());
    RubyLevel parent_level;
    parent_level.append_range(base::span(level).first(level.size() - 1));

    auto parent_it = std::ranges::find_if(
        ruby_lines_, [&](const Member<RubyLine>& potential_parent) {
          return std::ranges::equal(potential_parent->Level(), parent_level);
        });
    if (parent_it != ruby_lines_.end()) {
      RubyLine* parent = parent_it->Get();
      if (level.back() > 0) {
        parent->AddOverChild(line.Get());
      } else {
        parent->AddUnderChild(line.Get());
      }
    }
  }

  for (auto& line : ruby_lines_) {
    line->SortChildren();
  }

  return root;
}

FontHeight RubyBlockPositionCalculator::ComputeRelativeOffsets(
    RubyLine& node,
    const LogicalLineItems& base_line_items,
    const FontHeight& line_box_metrics) {
  FontHeight node_metrics;
  if (node.IsBaseLevel()) {
    if (!node.OverChildren().empty()) {
      node_metrics = ComputeLogicalLineEmHeight(
          base_line_items, node.OverChildren().front()->BaseIndexList());
    } else if (!node.UnderChildren().empty()) {
      node_metrics = ComputeLogicalLineEmHeight(
          base_line_items, node.UnderChildren().front()->BaseIndexList());
    }
    if (!node_metrics.LineHeight()) {
      node_metrics = line_box_metrics;
    }
  } else {
    node_metrics = node.UpdateMetrics();
  }

  LayoutUnit subtree_ascent = node_metrics.ascent;
  LayoutUnit subtree_descent = node_metrics.descent;
  FontHeight node_emphasis = node.ComputeLevelEmphasisHeights(base_line_items);

  if (!node.OverChildren().empty()) {
    LayoutUnit current_offset = -node_metrics.ascent;
    wtf_size_t i = 0;

    // 1. Own annotations
    for (; i < node.OverChildren().size(); ++i) {
      auto& child = node.OverChildren()[i];
      if (child->Level().size() <= node.Level().size()) {
        break;
      }
      FontHeight child_subtree_metrics =
          ComputeRelativeOffsets(*child, base_line_items, line_box_metrics);
      LayoutUnit child_relative_offset =
          current_offset - child_subtree_metrics.descent;
      child->SetRelativeOffset(child_relative_offset);
      current_offset = child_relative_offset - child_subtree_metrics.ascent;
    }

    // 1.5. Include own annotations in subtree ascent
    subtree_ascent = std::max(subtree_ascent, -current_offset);

    // 2. Node's emphasis
    LayoutUnit emphasis_top_with_anno = LayoutUnit::Max();
    if (i > 0) {
      FontHeight emp_with_anno = node.ComputeParentEmphasisHeightsForChild(
          *node.OverChildren()[i - 1], base_line_items);
      emphasis_top_with_anno = current_offset - emp_with_anno.ascent;
    }
    LayoutUnit emphasis_top_without_anno =
        -node_metrics.ascent - node_emphasis.ascent;
    LayoutUnit emphasis_top =
        std::min(emphasis_top_with_anno, emphasis_top_without_anno);
    subtree_ascent = std::max(subtree_ascent, -emphasis_top);

    // 3. Higher-level annotations
    for (; i < node.OverChildren().size(); ++i) {
      auto& child = node.OverChildren()[i];
      FontHeight child_subtree_metrics =
          ComputeRelativeOffsets(*child, base_line_items, line_box_metrics);
      FontHeight parent_emphasis_for_child =
          node.ComputeParentEmphasisHeightsForChild(*child, base_line_items);

      LayoutUnit child_bottom =
          current_offset - parent_emphasis_for_child.ascent;
      LayoutUnit child_relative_offset =
          child_bottom - child_subtree_metrics.descent;
      child->SetRelativeOffset(child_relative_offset);
      current_offset = child_relative_offset - child_subtree_metrics.ascent;

      subtree_ascent = std::max(subtree_ascent, -current_offset);
    }
  } else {
    subtree_ascent += node_emphasis.ascent;
  }

  if (!node.UnderChildren().empty()) {
    LayoutUnit current_offset = node_metrics.descent;
    wtf_size_t i = 0;

    // 1. Own annotations
    for (; i < node.UnderChildren().size(); ++i) {
      auto& child = node.UnderChildren()[i];
      if (child->Level().size() <= node.Level().size()) {
        break;
      }
      FontHeight child_subtree_metrics =
          ComputeRelativeOffsets(*child, base_line_items, line_box_metrics);
      LayoutUnit child_relative_offset =
          current_offset + child_subtree_metrics.ascent;
      child->SetRelativeOffset(child_relative_offset);
      current_offset = child_relative_offset + child_subtree_metrics.descent;
    }

    // 1.5. Include own annotations in subtree descent
    subtree_descent = std::max(subtree_descent, current_offset);

    // 2. Node's emphasis
    LayoutUnit emphasis_bottom_with_anno = LayoutUnit::Min();
    if (i > 0) {
      FontHeight emp_with_anno = node.ComputeParentEmphasisHeightsForChild(
          *node.UnderChildren()[i - 1], base_line_items);
      emphasis_bottom_with_anno = current_offset + emp_with_anno.descent;
    }
    LayoutUnit emphasis_bottom_without_anno =
        node_metrics.descent + node_emphasis.descent;
    LayoutUnit emphasis_bottom =
        std::max(emphasis_bottom_with_anno, emphasis_bottom_without_anno);
    subtree_descent = std::max(subtree_descent, emphasis_bottom);

    // 3. Higher-level annotations
    for (; i < node.UnderChildren().size(); ++i) {
      auto& child = node.UnderChildren()[i];
      FontHeight child_subtree_metrics =
          ComputeRelativeOffsets(*child, base_line_items, line_box_metrics);
      FontHeight parent_emphasis_for_child =
          node.ComputeParentEmphasisHeightsForChild(*child, base_line_items);

      LayoutUnit child_top = current_offset + parent_emphasis_for_child.descent;
      LayoutUnit child_relative_offset =
          child_top + child_subtree_metrics.ascent;
      child->SetRelativeOffset(child_relative_offset);
      current_offset = child_relative_offset + child_subtree_metrics.descent;

      subtree_descent = std::max(subtree_descent, current_offset);
    }
  } else {
    subtree_descent += node_emphasis.descent;
  }

  return {subtree_ascent, subtree_descent};
}

void RubyBlockPositionCalculator::ComputeOffsetsFromBase(
    RubyLine& node,
    LayoutUnit parent_offset_from_base) {
  LayoutUnit offset_from_base = parent_offset_from_base + node.RelativeOffset();
  node.SetOffset(offset_from_base);
  node.MoveInBlockDirection(offset_from_base);

  for (auto& child : node.OverChildren()) {
    ComputeOffsetsFromBase(*child, offset_from_base);
  }
  for (auto& child : node.UnderChildren()) {
    ComputeOffsetsFromBase(*child, offset_from_base);
  }
}

// ================================================================

RubyBlockPositionCalculator::RubyLine::RubyLine(const RubyLevel& level)
    : level_(level) {}

void RubyBlockPositionCalculator::RubyLine::Trace(Visitor* visitor) const {
  visitor->Trace(column_list_);
  visitor->Trace(over_children_);
  visitor->Trace(under_children_);
}

void RubyBlockPositionCalculator::RubyLine::SortChildren() {
  auto compare_abs_level = [](const Member<RubyLine>& a,
                              const Member<RubyLine>& b) {
    return std::abs(a->Level().back()) < std::abs(b->Level().back());
  };
  std::ranges::sort(over_children_, compare_abs_level);
  std::ranges::sort(under_children_, compare_abs_level);
}

bool RubyBlockPositionCalculator::RubyLine::operator<(
    const RubyLine& another) const {
  const RubyLevel& level1 = Level();
  const RubyLevel& level2 = another.Level();
  wtf_size_t i = 0;
  while (i < level1.size() && i < level2.size() && level1[i] == level2[i]) {
    ++i;
  }
  RubyLevel::ValueType value1 = i < level1.size() ? level1[i] : 0;
  RubyLevel::ValueType value2 = i < level2.size() ? level2[i] : 0;
  return value1 < value2;
}

void RubyBlockPositionCalculator::RubyLine::Append(
    LogicalRubyColumn& logical_column) {
  column_list_.push_back(logical_column);
}

void RubyBlockPositionCalculator::RubyLine::MaybeRecordBaseIndexes(
    const LogicalRubyColumn& logical_column) {
  if (IsFirstOverLevel() || IsFirstUnderLevel()) {
    base_index_list_.reserve(base_index_list_.size() + logical_column.size);
    for (wtf_size_t item_index = logical_column.start_index;
         item_index < logical_column.EndIndex(); ++item_index) {
      base_index_list_.push_back(item_index);
    }
  }
}

FontHeight RubyBlockPositionCalculator::RubyLine::UpdateMetrics() {
  DCHECK(metrics_.IsEmpty());
  metrics_ = FontHeight();
  for (auto& column : column_list_) {
    const auto margins = column->state_stack.AnnotationBoxBlockAxisMargins();
    if (!margins.has_value()) {
      metrics_.Unite(ComputeLogicalLineEmHeight(*column->annotation_items));
    } else {
      // A placeholder item is at [0] in LTR, but it's not at [0] in RTL.
      for (const LogicalLineItem& item : *column->annotation_items) {
        if (item.IsPlaceholder()) {
          metrics_.Unite({-item.BlockOffset() + margins->first,
                          item.BlockEndOffset() + margins->second});
          break;
        }
      }
    }
  }
  return metrics_;
}

void RubyBlockPositionCalculator::RubyLine::MoveInBlockDirection(
    LayoutUnit offset) {
  for (auto& column : column_list_) {
    column->annotation_items->MoveInBlockDirection(offset);
    column->state_stack.MoveBoxDataInBlockDirection(offset);
  }
}

void RubyBlockPositionCalculator::RubyLine::AddLinesTo(
    LogicalLineContainer& line_container) const {
  if (IsBaseLevel()) {
    return;
  }
  for (const auto& column : column_list_) {
    line_container.AddAnnotation(metrics_, *column->annotation_items);
  }
}

FontHeight RubyBlockPositionCalculator::RubyLine::ComputeLevelEmphasisHeights(
    const LogicalLineItems& base_line_items) const {
  FontHeight heights;
  if (IsBaseLevel()) {
    return ComputeEmphasisHeights(base_line_items);
  }
  for (const auto& column : column_list_) {
    if (column->annotation_items) {
      heights.Unite(ComputeEmphasisHeights(*column->annotation_items));
    }
  }
  return heights;
}

FontHeight
RubyBlockPositionCalculator::RubyLine::ComputeParentEmphasisHeightsForChild(
    const RubyLine& child,
    const LogicalLineItems& base_line_items) const {
  FontHeight heights;
  if (IsBaseLevel()) {
    if (!child.BaseIndexList().empty()) {
      return ComputeEmphasisHeights(base_line_items, child.BaseIndexList());
    }
    for (const auto& column : child.column_list_) {
      for (wtf_size_t i = column->start_index; i < column->EndIndex(); ++i) {
        if (i < base_line_items.size()) {
          heights.Unite(ComputeEmphasisHeights(base_line_items, {i}));
        }
      }
    }
  } else {
    for (const auto& node_col : column_list_) {
      if (node_col->annotation_items) {
        heights.Unite(ComputeEmphasisHeights(*node_col->annotation_items));
      }
    }
  }
  return heights;
}

bool RubyBlockPositionCalculator::RubyLine::ContainsColumn(
    const LogicalRubyColumn* column) const {
  return std::ranges::find(column_list_, column) != column_list_.end();
}

// ================================================================

void RubyBlockPositionCalculator::AnnotationDepth::Trace(
    Visitor* visitor) const {
  visitor->Trace(column);
}

}  // namespace blink
