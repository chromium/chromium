// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_widths.h"

#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

bool LineWidths::Set(const InlineNode& node,
                     base::span<const LayoutOpportunity> opportunities,
                     const InlineBreakToken* break_token) {
  // Set the default width if no exclusions.
  DCHECK_GE(opportunities.size(), 1u);
  const LayoutOpportunity& first_opportunity = opportunities.front();
  if (opportunities.size() == 1 && !node.HasFloats()) {
    DCHECK(!first_opportunity.HasShapeExclusions());
    default_width_ = first_opportunity.rect.InlineSize();
    DCHECK(!num_excluded_lines_);
    return true;
  }

  // This class supports only single simple exclusion.
  if (opportunities.size() > 2 || first_opportunity.HasShapeExclusions()) {
    return false;
  }

  // Compute the metrics when only one font is used in the block. This is the
  // same as "strut". https://drafts.csswg.org/css2/visudet.html#strut
  const ComputedStyle& block_style = node.Style();
  const Font& block_font = block_style.GetFont();
  const FontBaseline baseline_type = block_style.GetFontBaseline();
  InlineBoxState line_box;
  line_box.ComputeTextMetrics(block_style, block_font, baseline_type);

  // Check if all lines have the same line heights.
  const SimpleFontData* primary_font = block_font.PrimaryFont();
  DCHECK(primary_font);
  const InlineItemsData& items_data = node.ItemsData(/*is_first_line*/ false);
  // `::first-line` is not supported.
  DCHECK_EQ(&items_data, &node.ItemsData(true));
  base::span<const InlineItem> items(items_data.items);
  bool is_empty_so_far = true;
  if (break_token) {
    DCHECK(break_token->Start());
    items = items.subspan(break_token->StartItemIndex());
    is_empty_so_far = false;
  }
  for (const InlineItem& item : items) {
    switch (item.Type()) {
      case InlineItem::kText: {
        if (!item.Length()) [[unlikely]] {
          break;
        }
        const ShapeResult* shape_result = item.TextShapeResult();
        DCHECK(shape_result);
        if (shape_result->HasFallbackFonts(primary_font)) {
          // Compute the metrics. It may have different metrics if fonts are
          // different.
          DCHECK(item.Style());
          const ComputedStyle& item_style = *item.Style();
          InlineBoxState text_box;
          text_box.ComputeTextMetrics(item_style, item_style.GetFont(),
                                      baseline_type);
          if (text_box.include_used_fonts) {
            text_box.style = &item_style;
            const ShapeResultView* shape_result_view =
                ShapeResultView::Create(shape_result);
            text_box.AccumulateUsedFonts(shape_result_view);
          }
          // If it doesn't fit to the default line box, fail.
          if (!line_box.metrics.Contains(text_box.metrics)) {
            return false;
          }
        }
        break;
      }
      case InlineItem::kOpenTag: {
        DCHECK(item.Style());
        const ComputedStyle& style = *item.Style();
        if (style.VerticalAlign() != EVerticalAlign::kBaseline) [[unlikely]] {
          return false;
        }
        break;
      }
      case InlineItem::kCloseTag:
      case InlineItem::kControl:
      case InlineItem::kOutOfFlowPositioned:
      case InlineItem::kBidiControl:
      case InlineItem::kOpenRubyColumn:
      case InlineItem::kCloseRubyColumn:
      case InlineItem::kRubyLinePlaceholder:
        // These items don't affect line heights.
        break;
      case InlineItem::kFloating:
        // Only leading floats are computable without layout.
        if (is_empty_so_far) {
          break;
        }
        return false;
      case InlineItem::kAtomicInline:
      case InlineItem::kBlockInInline:
      case InlineItem::kInitialLetterBox:
      case InlineItem::kListMarker:
        // These items need layout to determine the height.
        return false;
    }
    if (is_empty_so_far && !item.IsEmptyItem()) {
      is_empty_so_far = false;
    }
  }

  if (opportunities.size() == 1) {
    // There are two conditions to come here:
    // * The `node` has floats, but only before `break_token`; i.e., no floats
    //   after `break_token`.
    // * The `node` has leading floats, but their size is 0, so they don't
    //   create exclusions.
    // Either way, there are no exclusions.
    default_width_ = first_opportunity.rect.InlineSize();
    return true;
  }

  // All lines have the same line height.
  // Compute the number of lines that have the exclusion.
  const LayoutUnit line_height = line_box.metrics.LineHeight();
  if (line_height <= LayoutUnit()) [[unlikely]] {
    return false;
  }
  DCHECK_GE(opportunities.size(), 2u);
  const LayoutOpportunity& last_opportunity = opportunities.back();
  DCHECK(!last_opportunity.HasShapeExclusions());
  default_width_ = last_opportunity.rect.InlineSize();
  const LayoutUnit exclusion_block_size =
      last_opportunity.rect.BlockStartOffset() -
      first_opportunity.rect.BlockStartOffset();
  DCHECK_GT(exclusion_block_size, LayoutUnit());
  // Use the float division because `LayoutUnit::operator/` doesn't have enough
  // precision; e.g., `LayoutUnit` computes "46.25 / 23" to 2.
  const float num_excluded_lines =
      ceil(exclusion_block_size.ToFloat() / line_height.ToFloat());
  DCHECK_GE(num_excluded_lines, 1);
  num_excluded_lines_ = base::saturated_cast<wtf_size_t>(num_excluded_lines);
  excluded_width_ = first_opportunity.rect.InlineSize();
  return true;
}

}  // namespace blink
