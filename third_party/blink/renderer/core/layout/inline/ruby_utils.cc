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
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_height.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {

namespace {

std::tuple<LayoutUnit, LayoutUnit> AdjustTextOverUnderOffsetsForEmHeight(
    LayoutUnit over,
    LayoutUnit under,
    const ComputedStyle& style,
    const ShapeResultView& shape_view) {
  DCHECK_LE(over, under);
  const SimpleFontData* primary_font_data = style.GetFont()->PrimaryFont();
  if (!primary_font_data)
    return std::make_pair(over, under);
  const auto font_baseline = style.GetFontBaseline();
  const LayoutUnit line_height = under - over;
  const LayoutUnit primary_ascent =
      primary_font_data->GetFontMetrics().FixedAscent(font_baseline);
  const LayoutUnit primary_descent = line_height - primary_ascent;

  HeapHashSet<Member<const SimpleFontData>> run_fonts = shape_view.UsedFonts();
  ClearCollectionScope clear_scope(&run_fonts);

  const LayoutUnit kNoDiff = LayoutUnit::Max();
  LayoutUnit over_diff = kNoDiff;
  LayoutUnit under_diff = kNoDiff;
  for (const auto& run_font : run_fonts) {
    const FontHeight normalized_height =
        run_font->NormalizedTypoAscentAndDescent(font_baseline);
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
    LogicalSize logical_size =
        LogicalFragment(style.GetWritingDirection(), fragment).Size();
    const LayoutBox* box = DynamicTo<LayoutBox>(line_item.GetLayoutObject());
    if (logical_size.inline_size && box && box->IsAtomicInlineLevel()) {
      LogicalRect overflow =
          WritingModeConverter(
              {ToLineWritingMode(style.GetWritingMode()), style.Direction()},
              fragment.Size())
              .ToLogical(box->ScrollableOverflowRect());
      // Assume 0 is the baseline.  BlockOffset() is always negative.
      return FontHeight(-overflow.offset.block_offset - line_item.BlockOffset(),
                        overflow.BlockEndOffset() + line_item.BlockOffset());
    }
  }
  return FontHeight();
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
    const HeapVector<LineInfo, 1> annotation_line_list) {
  AnnotationOverhang overhang;
  const ComputedStyle& base_line_style = base_line.LineStyle();

  if (base_line_style.RubyOverhang() == ERubyOverhang::kNone) {
    return overhang;
  }

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
  if (ruby_align == ERubyAlign::kStart) {
    overhang.end = std::min(space, half_width_of_annotation_font);
    return overhang;
  }
  std::optional<LayoutUnit> inset = ComputeRubyBaseInset(space, base_line);
  if (!inset) {
    return overhang;
  }
  overhang.start = std::min(*inset, half_width_of_annotation_font);
  overhang.end = overhang.start;
  return overhang;
}

AnnotationOverhang GetOverhang(const InlineItemResult& item) {
  DCHECK(item.IsRubyColumn());
  const InlineItemResultRubyColumn& column = *item.ruby_column;
  return GetOverhang(item.inline_size, column.base_line,
                     column.annotation_line_list);
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
  start_overhang = std::min(start_overhang, previous_item.inline_size / 2);
  return true;
}

LayoutUnit CommitPendingEndOverhang(const InlineItem& text_item,
                                    LineInfo* line_info) {
  DCHECK(line_info);
  InlineItemResults* items = line_info->MutableResults();
  if (items->size() < 1U) {
    return LayoutUnit();
  }
  if (text_item.Type() == InlineItem::kControl) {
    return LayoutUnit();
  }
  DCHECK_EQ(text_item.Type(), InlineItem::kText);
  wtf_size_t i = items->size() - 1;
  while (!(*items)[i].IsRubyColumn()) {
    const auto type = (*items)[i].item->Type();
    if (type != InlineItem::kOpenTag && type != InlineItem::kCloseTag &&
        type != InlineItem::kCloseRubyColumn &&
        type != InlineItem::kOpenRubyColumn &&
        type != InlineItem::kRubyLinePlaceholder) {
      return LayoutUnit();
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
  if (column_base_line_style.FontSize() < text_item.Style()->FontSize()) {
    return LayoutUnit();
  }
  if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled() &&
      text_item.Style()->GetTextEmphasisMark() != TextEmphasisMark::kNone &&
      column_base_line_style.GetRubyPosition() ==
          text_item.Style()->GetTextEmphasisLineLogicalSide()) {
    return LayoutUnit();
  }
  // Ideally we should refer to inline_size of |text_item| instead of the
  // width of the InlineItem's ShapeResult. However it's impossible to compute
  // inline_size of |text_item| before calling BreakText(), and BreakText()
  // requires precise |position_| which takes |end_overhang| into account.
  LayoutUnit text_inline_size =
      LayoutUnit(text_item.TextShapeResult()->Width());
  LayoutUnit end_overhang =
      std::min(column_item.pending_end_overhang, text_inline_size / 2);
  InlineItemResult& end_item =
      column_item.ruby_column->base_line.MutableResults()->back();
  end_item.margins.inline_end -= end_overhang;
  column_item.pending_end_overhang = LayoutUnit();
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
    const ComputedStyle& line_style,
    std::optional<FontHeight> annotation_metrics) {
  // Min/max position of content and annotations, ignoring line-height.
  // They are distance from the line box top.
  const LayoutUnit line_over;
  LayoutUnit content_over = line_over + line_box_metrics.ascent;
  LayoutUnit content_under = content_over;

  bool has_over_annotation = false;
  bool has_under_annotation = false;

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
    LayoutUnit item_over = line_box_metrics.ascent + item.BlockOffset();
    LayoutUnit item_under = line_box_metrics.ascent + item.BlockEndOffset();
    if (item.shape_result) {
      if (const auto* style = item.Style()) {
        std::tie(item_over, item_under) = AdjustTextOverUnderOffsetsForEmHeight(
            item_over, item_under, *style, *item.shape_result);
      }
    } else {
      const LayoutBox* box = DynamicTo<LayoutBox>(item.GetLayoutObject());
      const auto* fragment = item.GetPhysicalFragment();
      if (fragment && box && box->IsAtomicInlineLevel() &&
          !box->IsInitialLetterBox()) {
        item_under = ComputeEmHeight(item).LineHeight();
      } else if (item.IsInlineBox()) {
        continue;
      }
    }
    content_over = std::min(content_over, item_over);
    content_under = std::max(content_under, item_under);

    if (const auto* style = item.Style()) {
      if (style->GetTextEmphasisMark() != TextEmphasisMark::kNone) {
        if (RuntimeEnabledFeatures::TextEmphasisWithRubyEnabled()) {
          const auto& emphasis_mark_outsets =
              InlineBoxState::ComputeEmphasisMarkOutsets(*style,
                                                         *style->GetFont());
          if (style->GetTextEmphasisLineLogicalSide() ==
              LineLogicalSide::kOver) {
            over_emphasis =
                std::max(emphasis_mark_outsets.LineHeight(), over_emphasis);
          } else {
            under_emphasis =
                std::max(emphasis_mark_outsets.LineHeight(), under_emphasis);
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
  const LayoutUnit font_size(line_style.ComputedFontSize());
  if (content_under - content_over < font_size) {
    LayoutUnit half_leading = (line_box_metrics.LineHeight() - font_size) / 2;
    half_leading = half_leading.ClampNegativeToZero();
    content_over = line_over + half_leading;
    content_under = line_under - half_leading;
  }

  // Don't provide annotation space if text-emphasis exists.
  // TODO(layout-dev): If the text-emphasis is in [line_over, line_under],
  // this line can provide annotation space.
  if (over_emphasis > LayoutUnit()) {
    content_over = std::min(content_over, line_over);
  }
  if (under_emphasis > LayoutUnit()) {
    content_under = std::max(content_under, line_under);
  }

  // With some fonts, text fragment sizes can exceed line-height.
  // We'd like to set overflow only if we have annotations.
  // This affects fast/ruby/line-height.html on macOS.
  if (content_over < line_over && !has_over_annotation)
    content_over = line_over;
  if (content_under > line_under && !has_under_annotation)
    content_under = line_under;

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
        new_level.AppendVector(current);
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
      annotation_metrics += HandleRubyLine(
          annotation_line, closing_depth.column->RubyColumnList());
      annotation_line.MaybeRecordBaseIndexes(*closing_depth.column);

      LayoutUnit annotation_height =
          closing_depth.column->annotation_items
              ? ComputeLogicalLineEmHeight(
                    *closing_depth.column->annotation_items)
                    .LineHeight()
              : LayoutUnit();
      if (closing_depth.column->ruby_position == RubyPosition::kOver) {
        annotation_metrics.ascent += annotation_height;
      } else {
        annotation_metrics.descent += annotation_height;
      }

      depth_stack.pop_back();
      if (!depth_stack.empty()) {
        AnnotationDepth& parent_depth = depth_stack.back();
        parent_depth.over_depth =
            std::max(parent_depth.over_depth, closing_depth.over_depth);
        parent_depth.under_depth =
            std::min(parent_depth.under_depth, closing_depth.under_depth);
      }
    }
    column_list[i]->annotation_metrics = annotation_metrics;
    max_annotation_metrics.Unite(annotation_metrics);
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

// ================================================================

RubyBlockPositionCalculator::RubyLine::RubyLine(const RubyLevel& level)
    : level_(level) {}

void RubyBlockPositionCalculator::RubyLine::Trace(Visitor* visitor) const {
  visitor->Trace(column_list_);
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

// ================================================================

void RubyBlockPositionCalculator::AnnotationDepth::Trace(
    Visitor* visitor) const {
  visitor->Trace(column);
}

}  // namespace blink
