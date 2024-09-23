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
  const SimpleFontData* primary_font_data = style.GetFont().PrimaryFont();
  if (!primary_font_data)
    return std::make_pair(over, under);
  const auto font_baseline = style.GetFontBaseline();
  const LayoutUnit line_height = under - over;
  const LayoutUnit primary_ascent =
      primary_font_data->GetFontMetrics().FixedAscent(font_baseline);
  const LayoutUnit primary_descent = line_height - primary_ascent;

  // We don't use ShapeResultView::FallbackFonts() because we can't know if the
  // primary font is actually used with FallbackFonts().
  HeapVector<ShapeResult::RunFontData> run_fonts;
  ClearCollectionScope clear_scope(&run_fonts);
  shape_view.GetRunFontData(&run_fonts);
  const LayoutUnit kNoDiff = LayoutUnit::Max();
  LayoutUnit over_diff = kNoDiff;
  LayoutUnit under_diff = kNoDiff;
  for (const auto& run_font : run_fonts) {
    const SimpleFontData* font_data = run_font.font_data_;
    if (!font_data)
      continue;
    const FontHeight normalized_height =
        font_data->NormalizedTypoAscentAndDescent(font_baseline);
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
    const SimpleFontData* primary_font_data = style->GetFont().PrimaryFont();
    if (!primary_font_data) {
      return FontHeight();
    }
    const auto font_baseline = style->GetFontBaseline();
    const FontHeight primary_height =
        primary_font_data->GetFontMetrics().GetFloatFontHeight(font_baseline);
    FontHeight result_height;
    // We don't use ShapeResultView::FallbackFonts() because we can't know if
    // the primary font is actually used with FallbackFonts().
    HeapVector<ShapeResult::RunFontData> run_fonts;
    ClearCollectionScope clear_scope(&run_fonts);
    shape_result_view->GetRunFontData(&run_fonts);
    for (const auto& run_font : run_fonts) {
      const SimpleFontData* font_data = run_font.font_data_;
      if (!font_data) {
        continue;
      }
      result_height.Unite(
          font_data->NormalizedTypoAscentAndDescent(font_baseline));
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

RubyItemIndexes ParseRubyInInlineItems(const HeapVector<InlineItem>& items,
                                       wtf_size_t start_item_index) {
  CHECK_LT(start_item_index, items.size());
  CHECK_EQ(items[start_item_index].Type(), InlineItem::kOpenRubyColumn);
  RubyItemIndexes indexes = {start_item_index, WTF::kNotFound, WTF::kNotFound,
                             WTF::kNotFound};
  for (wtf_size_t i = start_item_index + 1; i < items.size(); ++i) {
    const InlineItem& item = items[i];
    if (item.Type() == InlineItem::kCloseRubyColumn) {
      if (indexes.base_end == WTF::kNotFound) {
        DCHECK_EQ(indexes.annotation_start, WTF::kNotFound);
        indexes.base_end = i;
      } else {
        DCHECK_NE(indexes.annotation_start, WTF::kNotFound);
      }
      indexes.column_end = i;
      return indexes;
    }
    if (item.Type() == InlineItem::kOpenTag &&
        item.GetLayoutObject()->IsInlineRubyText()) {
      DCHECK_EQ(indexes.base_end, WTF::kNotFound);
      DCHECK_EQ(indexes.annotation_start, WTF::kNotFound);
      indexes.base_end = i;
      indexes.annotation_start = i;
    } else if (item.Type() == InlineItem::kOpenRubyColumn) {
      RubyItemIndexes sub_indexes = ParseRubyInInlineItems(items, i);
      i = sub_indexes.column_end;
    }
  }
  NOTREACHED();
}

PhysicalRect AdjustTextRectForEmHeight(const PhysicalRect& rect,
                                       const ComputedStyle& style,
                                       const ShapeResultView* shape_view,
                                       WritingMode writing_mode) {
  if (!shape_view)
    return rect;
  const LayoutUnit line_height = IsHorizontalWritingMode(writing_mode)
                                     ? rect.size.height
                                     : rect.size.width;
  auto [over, under] = AdjustTextOverUnderOffsetsForEmHeight(
      LayoutUnit(), line_height, style, *shape_view);
  const LayoutUnit over_diff = over;
  const LayoutUnit under_diff = line_height - under;
  const LayoutUnit new_line_height = under - over;

  if (IsHorizontalWritingMode(writing_mode)) {
    return {{rect.offset.left, rect.offset.top + over_diff},
            PhysicalSize(rect.size.width, new_line_height)};
  }
  if (IsFlippedLinesWritingMode(writing_mode)) {
    return {{rect.offset.left + under_diff, rect.offset.top},
            PhysicalSize(new_line_height, rect.size.height)};
  }
  return {{rect.offset.left + over_diff, rect.offset.top},
          PhysicalSize(new_line_height, rect.size.height)};
}

AnnotationOverhang GetOverhang(
    LayoutUnit ruby_size,
    const LineInfo& base_line,
    const HeapVector<LineInfo, 1> annotation_line_list) {
  DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
  AnnotationOverhang overhang;
  ERubyAlign ruby_align = base_line.LineStyle().RubyAlign();
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
  AnnotationOverhang overhang;
  if (item.IsRubyColumn()) {
    DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
    const InlineItemResultRubyColumn& column = *item.ruby_column;
    return GetOverhang(item.inline_size, column.base_line,
                       column.annotation_line_list);
  }

  if (!item.layout_result)
    return overhang;

  const auto& column_fragment = item.layout_result->GetPhysicalFragment();

  const ComputedStyle* ruby_text_style = nullptr;
  for (const auto& child_link : column_fragment.PostLayoutChildren()) {
    const PhysicalFragment& child_fragment = *child_link.get();
    const LayoutObject* layout_object = child_fragment.GetLayoutObject();
    if (!layout_object)
      continue;
    if (layout_object->IsRubyText()) {
      ruby_text_style = layout_object->Style();
      break;
    }
  }
  if (!ruby_text_style)
    return overhang;

  // We allow overhang up to the half of ruby text font size.
  const LayoutUnit half_width_of_ruby_font =
      LayoutUnit(ruby_text_style->FontSize()) / 2;
  LayoutUnit start_overhang = half_width_of_ruby_font;
  LayoutUnit end_overhang = half_width_of_ruby_font;
  bool found_line = false;
  for (const auto& child_link : column_fragment.PostLayoutChildren()) {
    const PhysicalFragment& child_fragment = *child_link.get();
    const LayoutObject* layout_object = child_fragment.GetLayoutObject();
    if (!layout_object->IsRubyBase())
      continue;
    const ComputedStyle& base_style = child_fragment.Style();
    const auto writing_direction = base_style.GetWritingDirection();
    const LayoutUnit base_inline_size =
        LogicalFragment(writing_direction, child_fragment).InlineSize();
    // RubyBase's inline_size is always same as RubyColumn's inline_size.
    // Overhang values are offsets from RubyBase's inline edges to
    // the outmost text.
    for (const auto& base_child_link : child_fragment.PostLayoutChildren()) {
      const LayoutUnit line_inline_size =
          LogicalFragment(writing_direction, *base_child_link).InlineSize();
      if (line_inline_size == LayoutUnit())
        continue;
      found_line = true;
      const LayoutUnit start =
          base_child_link.offset
              .ConvertToLogical(writing_direction, child_fragment.Size(),
                                base_child_link.get()->Size())
              .inline_offset;
      const LayoutUnit end = base_inline_size - start - line_inline_size;
      start_overhang = std::min(start_overhang, start);
      end_overhang = std::min(end_overhang, end);
    }
  }
  if (!found_line)
    return overhang;
  overhang.start = start_overhang;
  overhang.end = end_overhang;
  return overhang;
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
  if (previous_item.item->Style()->FontSize() > ruby_style.FontSize()) {
    return false;
  }
  start_overhang = std::min(start_overhang, previous_item.inline_size);
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
  if (RuntimeEnabledFeatures::RubyLineBreakableEnabled()) {
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
    if (column_item.ruby_column->base_line.LineStyle().FontSize() <
        text_item.Style()->FontSize()) {
      return LayoutUnit();
    }
    // Ideally we should refer to inline_size of |text_item| instead of the
    // width of the InlineItem's ShapeResult. However it's impossible to compute
    // inline_size of |text_item| before calling BreakText(), and BreakText()
    // requires precise |position_| which takes |end_overhang| into account.
    LayoutUnit end_overhang =
        std::min(column_item.pending_end_overhang,
                 LayoutUnit(text_item.TextShapeResult()->Width()));
    InlineItemResult& end_item =
        column_item.ruby_column->base_line.MutableResults()->back();
    DCHECK_EQ(end_item.item->Type(), InlineItem::kRubyLinePlaceholder);
    DCHECK_EQ(end_item.margins.inline_end, LayoutUnit());
    end_item.margins.inline_end = -end_overhang;
    column_item.pending_end_overhang = LayoutUnit();
    return end_overhang;
  }
  while ((*items)[i].item->Type() != InlineItem::kAtomicInline) {
    const auto type = (*items)[i].item->Type();
    if (type != InlineItem::kOpenTag && type != InlineItem::kCloseTag) {
      return LayoutUnit();
    }
    if (i-- == 0)
      return LayoutUnit();
  }
  InlineItemResult& atomic_inline_item = (*items)[i];
  if (!atomic_inline_item.layout_result->GetPhysicalFragment().IsRubyColumn()) {
    return LayoutUnit();
  }
  if (atomic_inline_item.pending_end_overhang <= LayoutUnit())
    return LayoutUnit();
  if (atomic_inline_item.item->Style()->FontSize() <
      text_item.Style()->FontSize()) {
    return LayoutUnit();
  }
  // Ideally we should refer to inline_size of |text_item| instead of the width
  // of the InlineItem's ShapeResult. However it's impossible to compute
  // inline_size of |text_item| before calling BreakText(), and BreakText()
  // requires precise |position_| which takes |end_overhang| into account.
  LayoutUnit end_overhang =
      std::min(atomic_inline_item.pending_end_overhang,
               LayoutUnit(text_item.TextShapeResult()->Width()));
  DCHECK_EQ(atomic_inline_item.margins.inline_end, LayoutUnit());
  atomic_inline_item.margins.inline_end = -end_overhang;
  atomic_inline_item.inline_size -= end_overhang;
  atomic_inline_item.pending_end_overhang = LayoutUnit();
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
      NOTREACHED_IN_MIGRATION();
      break;
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
  bool has_over_emphasis = false;
  bool has_under_emphasis = false;
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
      if (fragment && fragment->IsRubyColumn()) {
        DCHECK(!RuntimeEnabledFeatures::RubyLineBreakableEnabled());
        PhysicalRect rect =
            ComputeRubyEmHeightBox(*To<PhysicalBoxFragment>(fragment));
        LayoutUnit block_size;
        if (line_style.IsHorizontalWritingMode()) {
          item_under = item_over + rect.Bottom();
          item_over += rect.offset.top;
          block_size = fragment->Size().height;
        } else {
          block_size = fragment->Size().width;
          // We assume 'over' is always on right in vertical writing modes.
          // TODO(layout-dev): sideways-lr support.
          DCHECK(line_style.IsFlippedBlocksWritingMode() ||
                 line_style.IsFlippedLinesWritingMode());
          item_under = item_over + block_size;
          item_over = item_under - rect.Right();
          item_under -= rect.offset.left;
        }

        // Check if we really have an annotation.
        if (const auto& layout_result = item.layout_result) {
          LayoutUnit overflow = layout_result->AnnotationOverflow();
          if (IsFlippedLinesWritingMode(line_style.GetWritingMode()))
            overflow = -overflow;
          if (overflow < LayoutUnit())
            has_over_annotation = true;
          else if (overflow > LayoutUnit())
            has_under_annotation = true;
        }
      } else if (fragment && box && box->IsAtomicInlineLevel() &&
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
        if (style->GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver)
          has_over_emphasis = true;
        else
          has_under_emphasis = true;
      }
    }
  }

  if (annotation_metrics) {
    DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
    if (annotation_metrics->ascent) {
      LayoutUnit item_over =
          line_box_metrics.ascent - annotation_metrics->ascent;
      content_over = std::min(content_over, item_over);
      has_over_annotation = true;
    }
    if (annotation_metrics->descent) {
      LayoutUnit item_under =
          line_box_metrics.ascent + annotation_metrics->descent;
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
  if (has_over_emphasis)
    content_over = std::min(content_over, line_over);
  if (has_under_emphasis)
    content_under = std::max(content_under, line_under);

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

namespace {

// Em height box. including contents, in the local coordinate.
PhysicalRect ComputeRubyEmHeightBox(const PhysicalFragment& fragment,
                                    const PhysicalBoxFragment& container) {
  switch (fragment.Type()) {
    case PhysicalFragment::kFragmentBox:
      return ComputeRubyEmHeightBox(To<PhysicalBoxFragment>(fragment));
    case PhysicalFragment::kFragmentLineBox:
      NOTREACHED_IN_MIGRATION()
          << "You must call LineBoxFragment::ComputeRubyEmHeightBox "
             "explicitly.";
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return {{}, fragment.Size()};
}

void AdjustRubyEmHeightBoxForPropagation(const PhysicalFragment& fragment,
                                         const PhysicalBoxFragment& container,
                                         PhysicalRect* overflow) {
  DCHECK(!fragment.IsLineBox());
  if (!fragment.IsCSSBox()) {
    return;
  }
  if (fragment.IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  const LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object);
  const LayoutObject* container_layout_object = container.GetLayoutObject();
  DCHECK(container_layout_object);
  if (layout_object->ShouldUseTransformFromContainer(container_layout_object)) {
    gfx::Transform transform;
    layout_object->GetTransformFromContainer(container_layout_object,
                                             PhysicalOffset(), transform);
    *overflow =
        PhysicalRect::EnclosingRect(transform.MapRect(gfx::RectF(*overflow)));
  }
}

// ComputeRubyEmHeightBox(), with transforms applied wrt container if needed.
// This does not include any offsets from the parent (including relpos).
PhysicalRect ComputeRubyEmHeightBoxForPropagation(
    const PhysicalFragment& fragment,
    const PhysicalBoxFragment& container) {
  PhysicalRect overflow = ComputeRubyEmHeightBox(fragment, container);
  AdjustRubyEmHeightBoxForPropagation(fragment, container, &overflow);
  return overflow;
}

// Chop the hanging part from scrollable overflow. Children overflow in inline
// direction should hang, which should not cause scroll.
// TODO(kojii): Should move to text fragment to make this more accurate.
void AdjustRubyEmHeightBoxForHanging(const PhysicalRect& rect,
                                     const WritingMode container_writing_mode,
                                     PhysicalRect* overflow) {
  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (overflow->offset.left < rect.offset.left) {
      overflow->offset.left = rect.offset.left;
    }
    if (overflow->Right() > rect.Right()) {
      overflow->ShiftRightEdgeTo(rect.Right());
    }
  } else {
    if (overflow->offset.top < rect.offset.top) {
      overflow->offset.top = rect.offset.top;
    }
    if (overflow->Bottom() > rect.Bottom()) {
      overflow->ShiftBottomEdgeTo(rect.Bottom());
    }
  }
}

void AddRubyEmHeightBoxForInlineChild(const PhysicalFragment& child,
                                      const PhysicalBoxFragment& container,
                                      const ComputedStyle& container_style,
                                      const FragmentItem& line,
                                      bool has_hanging,
                                      const InlineCursor& cursor,
                                      PhysicalRect* overflow) {
  DCHECK(child.IsLineBox() || child.IsInlineBox());
  DCHECK(cursor.Current().Item());
  DCHECK(cursor.Current().Item()->BoxFragment() == &child ||
         cursor.Current().Item()->LineBoxFragment() == &child);
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  for (InlineCursor descendants = cursor.CursorForDescendants(); descendants;) {
    const FragmentItem* item = descendants.CurrentItem();
    DCHECK(item);
    if (item->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      NOTREACHED_IN_MIGRATION();
      descendants.MoveToNextSkippingChildren();
      continue;
    }
    if (item->IsText()) {
      PhysicalRect child_scroll_overflow = item->RectInContainerFragment();
      child_scroll_overflow = AdjustTextRectForEmHeight(
          child_scroll_overflow, item->Style(), item->TextShapeResult(),
          container_writing_mode);
      if (has_hanging) [[unlikely]] {
        AdjustRubyEmHeightBoxForHanging(line.RectInContainerFragment(),
                                        container_writing_mode,
                                        &child_scroll_overflow);
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    if (const PhysicalBoxFragment* child_box = item->PostLayoutBoxFragment()) {
      PhysicalRect child_scroll_overflow;
      if (child_box->GetBoxType() != PhysicalFragment::kInlineBox &&
          !child.IsRubyBox()) {
        child_scroll_overflow = item->RectInContainerFragment();
      }
      if (child_box->IsInlineBox()) {
        AddRubyEmHeightBoxForInlineChild(*child_box, container, container_style,
                                         line, has_hanging, descendants,
                                         &child_scroll_overflow);
        AdjustRubyEmHeightBoxForPropagation(*child_box, container,
                                            &child_scroll_overflow);
        if (has_hanging) [[unlikely]] {
          AdjustRubyEmHeightBoxForHanging(line.RectInContainerFragment(),
                                          container_writing_mode,
                                          &child_scroll_overflow);
        }
      } else {
        child_scroll_overflow =
            ComputeRubyEmHeightBoxForPropagation(*child_box, container);
        child_scroll_overflow.offset += item->OffsetInContainerFragment();
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    // Add all children of a culled inline box; i.e., an inline box without
    // margin/border/padding etc.
    DCHECK_EQ(item->Type(), FragmentItem::kBox);
    descendants.MoveToNext();
  }
}

// Include the inline-size of the line-box in the overflow.
// Do not update block offset and block size of |overflow|.
inline void AddInlineSizeToRubyEmHeightBox(
    const PhysicalRect& rect,
    const WritingMode container_writing_mode,
    PhysicalRect* overflow) {
  PhysicalRect inline_rect;
  inline_rect.offset = rect.offset;
  if (IsHorizontalWritingMode(container_writing_mode)) {
    inline_rect.size.width = rect.size.width;
    inline_rect.offset.top = overflow->offset.top;
    inline_rect.size.height = overflow->size.height;
  } else {
    inline_rect.size.height = rect.size.height;
    inline_rect.offset.left = overflow->offset.left;
    inline_rect.size.width = overflow->size.width;
  }
  overflow->UniteEvenIfEmpty(inline_rect);
}

// Em height box. including contents, in the local coordinate.
// |ComputeRubyEmHeightBoxForLine| is not precomputed/cached because it cannot
// be computed when LineBox is generated because it needs container dimensions
// to resolve relative position of its children.
PhysicalRect ComputeRubyEmHeightBoxForLine(
    const PhysicalLineBoxFragment& line_fragment,
    const PhysicalBoxFragment& container,
    const ComputedStyle& container_style) {
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  PhysicalRect overflow;
  // Make sure we include the inline-size of the line-box in the overflow.
  AddInlineSizeToRubyEmHeightBox(line_fragment.LocalRect(),
                                 container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect ComputeRubyEmHeightBoxForLine(
    const PhysicalLineBoxFragment& line_fragment,
    const PhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const FragmentItem& line,
    const InlineCursor& cursor) {
  DCHECK_EQ(&line, cursor.CurrentItem());
  DCHECK_EQ(line.LineBoxFragment(), &line_fragment);

  PhysicalRect overflow;
  AddRubyEmHeightBoxForInlineChild(line_fragment, container, container_style,
                                   line, line_fragment.HasHanging(), cursor,
                                   &overflow);

  // Make sure we include the inline-size of the line-box in the overflow.
  // Note, the bottom half-leading should not be included. crbug.com/996847
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  AddInlineSizeToRubyEmHeightBox(line.RectInContainerFragment(),
                                 container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect ComputeRubyEmHeightBoxFromChildren(
    const PhysicalBoxFragment& fragment) {
  // TODO(kojii): See |ComputeRubyEmHeightBox|.
  const FragmentItems* items = fragment.Items();
  if (fragment.Children().empty() && !items) {
    return PhysicalRect();
  }

  // Internal struct to share logic between child fragments and child items.
  // - Inline children's overflow expands by padding end/after.
  // - Float / OOF overflow is added as is.
  // - Children not reachable by scroll overflow do not contribute to it.
  struct ComputeOverflowContext {
    STACK_ALLOCATED();

   public:
    explicit ComputeOverflowContext(const PhysicalBoxFragment& container)
        : container(container),
          style(container.Style()),
          writing_direction(style.GetWritingDirection()),
          border_inline_start(LayoutUnit(style.BorderInlineStartWidth())),
          border_block_start(LayoutUnit(style.BorderBlockStartWidth())) {
      DCHECK_EQ(&style, container.GetLayoutObject()->Style(
                            container.UsesFirstLineStyle()));

      // End and under padding are added to scroll overflow of inline children.
      // https://github.com/w3c/csswg-drafts/issues/129
      DCHECK_EQ(container.HasNonVisibleOverflow(),
                container.GetLayoutObject()->HasNonVisibleOverflow());
      if (container.HasNonVisibleOverflow()) {
        const auto* layout_object = To<LayoutBox>(container.GetLayoutObject());
        padding_strut =
            BoxStrut(LayoutUnit(), layout_object->PaddingInlineEnd(),
                     LayoutUnit(), layout_object->PaddingBlockEnd())
                .ConvertToPhysical(writing_direction);
      }
    }

    void AddChild(const PhysicalRect& child_scrollable_overflow) {
      // Do not add overflow if fragment is not reachable by scrolling.
      children_overflow.Unite(child_scrollable_overflow);
    }

    void AddFloatingOrOutOfFlowPositionedChild(
        const PhysicalFragment& child,
        const PhysicalOffset& child_offset) {
      DCHECK(child.IsFloatingOrOutOfFlowPositioned());
      PhysicalRect child_scrollable_overflow =
          ComputeRubyEmHeightBoxForPropagation(child, container);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const PhysicalLineBoxFragment& child,
                         const PhysicalOffset& child_offset) {
      if (padding_strut) {
        AddLineBoxRect({child_offset, child.Size()});
      }
      PhysicalRect child_scrollable_overflow =
          ComputeRubyEmHeightBoxForLine(child, container, style);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const FragmentItem& child,
                         const InlineCursor& cursor) {
      DCHECK_EQ(&child, cursor.CurrentItem());
      DCHECK_EQ(child.Type(), FragmentItem::kLine);
      if (padding_strut) {
        AddLineBoxRect(child.RectInContainerFragment());
      }
      const PhysicalLineBoxFragment* line_box = child.LineBoxFragment();
      DCHECK(line_box);
      PhysicalRect child_scrollable_overflow = ComputeRubyEmHeightBoxForLine(
          *line_box, container, style, child, cursor);
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxRect(const PhysicalRect& linebox_rect) {
      DCHECK(padding_strut);
      if (lineboxes_enclosing_rect) {
        lineboxes_enclosing_rect->Unite(linebox_rect);
      } else {
        lineboxes_enclosing_rect = linebox_rect;
      }
    }

    void AddPaddingToLineBoxChildren() {
      if (lineboxes_enclosing_rect) {
        DCHECK(padding_strut);
        lineboxes_enclosing_rect->Expand(*padding_strut);
        AddChild(*lineboxes_enclosing_rect);
      }
    }

    const PhysicalBoxFragment& container;
    const ComputedStyle& style;
    const WritingDirectionMode writing_direction;
    const LayoutUnit border_inline_start;
    const LayoutUnit border_block_start;
    std::optional<PhysicalBoxStrut> padding_strut;
    std::optional<PhysicalRect> lineboxes_enclosing_rect;
    PhysicalRect children_overflow;
  } context(fragment);

  // Traverse child items.
  if (items) {
    for (InlineCursor cursor(fragment, *items); cursor;
         cursor.MoveToNextSkippingChildren()) {
      const FragmentItem* item = cursor.CurrentItem();
      if (item->Type() == FragmentItem::kLine) {
        context.AddLineBoxChild(*item, cursor);
        continue;
      }

      if (const PhysicalBoxFragment* child_box =
              item->PostLayoutBoxFragment()) {
        if (child_box->IsFloatingOrOutOfFlowPositioned()) {
          context.AddFloatingOrOutOfFlowPositionedChild(
              *child_box, item->OffsetInContainerFragment());
        }
      }
    }
  }

  // Traverse child fragments.
  const bool add_inline_children =
      !items && fragment.IsInlineFormattingContext();
  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transforms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  for (const auto& child : fragment.PostLayoutChildren()) {
    if (child->IsFloatingOrOutOfFlowPositioned()) {
      context.AddFloatingOrOutOfFlowPositionedChild(*child, child.Offset());
    } else if (add_inline_children && child->IsLineBox()) {
      context.AddLineBoxChild(To<PhysicalLineBoxFragment>(*child),
                              child.Offset());
    } else if (fragment.IsRubyColumn()) {
      PhysicalRect r = ComputeRubyEmHeightBox(*child, fragment);
      r.offset += child.offset;
      context.AddChild(r);
    }
  }

  context.AddPaddingToLineBoxChildren();

  return context.children_overflow;
}

}  // namespace

PhysicalRect ComputeRubyEmHeightBox(const PhysicalBoxFragment& box_fragment) {
  DCHECK(box_fragment.GetLayoutObject());
  // TODO(kojii): It might be that |ComputeAnnotationOverflow| should move to
  // scrollable overflow recalc, but it is to be thought out.
  if (box_fragment.IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
    NOTREACHED_IN_MIGRATION();
    return PhysicalRect();
  }
  const LayoutObject* layout_object = box_fragment.GetLayoutObject();
  if (box_fragment.IsRubyBox()) {
    return ComputeRubyEmHeightBoxFromChildren(box_fragment);
  }
  if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    if (box_fragment.HasNonVisibleOverflow()) {
      return PhysicalRect({}, box_fragment.Size());
    }
    // Legacy is the source of truth for overflow
    return layout_box->ScrollableOverflowRect();
  } else if (layout_object->IsLayoutInline()) {
    // Inline overflow is a union of child overflows.
    PhysicalRect overflow;
    if (box_fragment.GetBoxType() != PhysicalBoxFragment::kInlineBox) {
      overflow = PhysicalRect({}, box_fragment.Size());
    }
    for (const auto& child_fragment : box_fragment.PostLayoutChildren()) {
      PhysicalRect child_overflow =
          ComputeRubyEmHeightBoxForPropagation(*child_fragment, box_fragment);
      child_overflow.offset += child_fragment.Offset();
      overflow.Unite(child_overflow);
    }
    return overflow;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  return PhysicalRect({}, box_fragment.Size());
}

// ================================================================

void UpdateRubyColumnInlinePositions(
    const LogicalLineItems& line_items,
    LayoutUnit inline_size,
    HeapVector<Member<LogicalRubyColumn>>& column_list) {
  DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
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
      NOTREACHED_IN_MIGRATION()
          << " LogicalLineItems::size()=" << line_items.size()
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

void RubyBlockPositionCalculator::HandleRubyLine(
    const RubyLine& current_ruby_line,
    const HeapVector<Member<LogicalRubyColumn>>& column_list) {
  if (column_list.empty()) {
    return;
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
    while (!depth_stack.empty() && should_close_column()) {
      const auto [annotation_level, closing_depth] =
          create_level_and_update_depth(current_level, depth_stack.back());
      RubyLine& annotation_line = EnsureRubyLine(annotation_level);
      annotation_line.Append(*closing_depth.column);
      HandleRubyLine(annotation_line, closing_depth.column->RubyColumnList());
      annotation_line.MaybeRecordBaseIndexes(*closing_depth.column);

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
}

RubyBlockPositionCalculator::RubyLine&
RubyBlockPositionCalculator::EnsureRubyLine(const RubyLevel& level) {
  // We do linear search because ruby_lines_ typically has only two items.
  auto it =
      base::ranges::find_if(ruby_lines_, [&](const Member<RubyLine>& line) {
        return base::ranges::equal(line->Level(), level);
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
  base::ranges::sort(ruby_lines_, [](const Member<RubyLine>& line1,
                                     const Member<RubyLine>& line2) {
    return *line1 < *line2;
  });

  auto base_iterator = base::ranges::find_if(
      ruby_lines_,
      [](const Member<RubyLine>& line) { return line->Level().empty(); });
  CHECK_NE(base_iterator, ruby_lines_.end());

  // Place "under" annotations from the base level to the lowest one.
  if (base_iterator != ruby_lines_.begin()) {
    auto first_under_iterator = base::ranges::find_if(
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
    auto first_over_iterator = base::ranges::find_if(
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
      DCHECK_GT(column->annotation_items->size(), 0u);
      const LogicalLineItem& item = (*column->annotation_items)[0];
      DCHECK(item.IsPlaceholder());
      metrics_.Unite({-item.BlockOffset() + margins->first,
                      item.BlockEndOffset() + margins->second});
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
