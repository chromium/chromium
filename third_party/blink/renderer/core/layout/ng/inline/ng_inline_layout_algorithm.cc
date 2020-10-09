// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_truncator.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

inline void ClearNeedsLayoutIfNeeded(LayoutObject* layout_object) {
  DCHECK(layout_object);
  if (layout_object->NeedsLayout())
    layout_object->ClearNeedsLayout();
}

}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode inline_node,
    const NGConstraintSpace& space,
    const NGInlineBreakToken* break_token,
    NGInlineChildLayoutContext* context)
    : NGLayoutAlgorithm(
          inline_node,
          ComputedStyle::CreateAnonymousStyleWithDisplay(inline_node.Style(),
                                                         EDisplay::kBlock),
          space,
          // Use LTR direction since inline layout handles bidi by itself and
          // lays out in visual order.
          TextDirection::kLtr,
          break_token),
      box_states_(nullptr),
      context_(context),
      baseline_type_(container_builder_.Style().GetFontBaseline()),
      quirks_mode_(inline_node.GetDocument().InLineHeightQuirksMode()) {
  DCHECK(context);
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGInlineLayoutAlgorithm::~NGInlineLayoutAlgorithm() = default;

NGInlineBoxState* NGInlineLayoutAlgorithm::HandleOpenTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    NGLogicalLineItems* line_box,
    NGInlineLayoutStateStack* box_states) const {
  NGInlineBoxState* box =
      box_states->OnOpenTag(item, item_result, baseline_type_, line_box);
  // Compute text metrics for all inline boxes since even empty inlines
  // influence the line height, except when quirks mode and the box is empty
  // for the purpose of empty block calculation.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  if (!quirks_mode_ || !item.IsEmptyItem())
    box->ComputeTextMetrics(*item.Style(), baseline_type_);

  if (item.Style()->HasMask()) {
    // Layout may change the bounding box, which affects MaskClip.
    if (LayoutObject* object = item.GetLayoutObject())
      object->SetNeedsPaintPropertyUpdate();
  }

  return box;
}

NGInlineBoxState* NGInlineLayoutAlgorithm::HandleCloseTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    NGLogicalLineItems* line_box,
    NGInlineBoxState* box) {
  if (UNLIKELY(quirks_mode_ && !item.IsEmptyItem()))
    box->EnsureTextMetrics(*item.Style(), baseline_type_);
  box = box_states_->OnCloseTag(ConstraintSpace(), line_box, box,
                                baseline_type_, item.HasEndEdge());
  // Just clear |NeedsLayout| flags. Culled inline boxes do not need paint
  // invalidations. If this object produces box fragments,
  // |NGInlineBoxStateStack| takes care of invalidations.
  item.GetLayoutObject()->ClearNeedsLayoutWithoutPaintInvalidation();
  return box;
}

// Prepare NGInlineLayoutStateStack for a new line.
void NGInlineLayoutAlgorithm::PrepareBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token) {
#if DCHECK_IS_ON()
  is_box_states_from_context_ = false;
#endif

  // Use the initial box states if no break token; i.e., a line from the start.
  if (!break_token) {
    box_states_ = context_->ResetBoxStates();
    return;
  }

  // Check if the box states in NGChildLayoutContext is valid for this line.
  // If the previous line was ::first-line, always rebuild because box states
  // have ::first-line styles.
  const Vector<NGInlineItem>& items = line_info.ItemsData().items;
  if (!break_token->UseFirstLineStyle()) {
    box_states_ =
        context_->BoxStatesIfValidForItemIndex(items, break_token->ItemIndex());
    if (box_states_) {
#if DCHECK_IS_ON()
      is_box_states_from_context_ = true;
#endif
      return;
    }
  }

  // If not, rebuild the box states for the break token.
  box_states_ = context_->ResetBoxStates();
  RebuildBoxStates(line_info, break_token, box_states_);
}

void NGInlineLayoutAlgorithm::RebuildBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token,
    NGInlineLayoutStateStack* box_states) const {
  // Compute which tags are not closed at the beginning of this line.
  NGInlineItemsData::OpenTagItems open_items;
  line_info.ItemsData().GetOpenTagItems(break_token->ItemIndex(), &open_items);

  // Create box states for tags that are not closed yet.
  NGLogicalLineItems line_box;
  box_states->OnBeginPlaceItems(line_info.LineStyle(), baseline_type_,
                                quirks_mode_, &line_box);
  for (const NGInlineItem* item : open_items) {
    NGInlineItemResult item_result;
    NGLineBreaker::ComputeOpenTagResult(*item, ConstraintSpace(), &item_result);
    HandleOpenTag(*item, item_result, &line_box, box_states);
  }
}

#if DCHECK_IS_ON()
void NGInlineLayoutAlgorithm::CheckBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token) const {
  NGInlineLayoutStateStack rebuilt;
  RebuildBoxStates(line_info, break_token, &rebuilt);
  NGLogicalLineItems line_box;
  rebuilt.OnBeginPlaceItems(line_info.LineStyle(), baseline_type_, quirks_mode_,
                            &line_box);
  DCHECK(box_states_);
  box_states_->CheckSame(rebuilt);
}
#endif

void NGInlineLayoutAlgorithm::CreateLine(
    const NGLineLayoutOpportunity& opportunity,
    NGLineInfo* line_info,
    NGLogicalLineItems* line_box,
    NGExclusionSpace* exclusion_space) {
  // Needs MutableResults to move ShapeResult out of the NGLineInfo.
  NGInlineItemResults* line_items = line_info->MutableResults();
  // Clear the current line without releasing the buffer.
  line_box->Shrink(0);

  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  LayoutUnit line_offset_for_text_align = ApplyTextAlign(line_info);

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  const ComputedStyle& line_style = line_info->LineStyle();
  box_states_->SetIsEmptyLine(line_info->IsEmptyLine());
  NGInlineBoxState* box = box_states_->OnBeginPlaceItems(
      line_style, baseline_type_, quirks_mode_, line_box);
#if DCHECK_IS_ON()
  if (is_box_states_from_context_)
    CheckBoxStates(*line_info, BreakToken());
#endif

  bool has_out_of_flow_positioned_items = false;
  bool has_floating_items = false;
  bool has_relative_positioned_items = false;

  // List items trigger strict line height, i.e. we make room for the line box
  // strut, for *every* line. This matches other browsers. The intention may
  // have been to make sure that there's always room for the list item marker,
  // but that doesn't explain why it's done for every line...
  if (quirks_mode_ && line_style.Display() == EDisplay::kListItem)
    box->ComputeTextMetrics(line_style, baseline_type_);

  bool has_logical_text_items = false;
  for (NGInlineItemResult& item_result : *line_items) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item.GetLayoutObject());
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutNGListItem());
      DCHECK(item_result.shape_result);

      if (UNLIKELY(quirks_mode_))
        box->EnsureTextMetrics(*item.Style(), baseline_type_);

      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts) {
        box->AccumulateUsedFonts(item_result.shape_result.get(),
                                 baseline_type_);
      }

      DCHECK(item.TextType() == NGTextType::kNormal ||
             item.TextType() == NGTextType::kSymbolMarker);
      if (UNLIKELY(item_result.hyphen_shape_result)) {
        LayoutUnit hyphen_inline_size = item_result.HyphenInlineSize();
        line_box->AddChild(item, std::move(item_result.shape_result),
                           item_result.TextOffset(), box->text_top,
                           item_result.inline_size - hyphen_inline_size,
                           box->text_height, item.BidiLevel());
        PlaceHyphen(item_result, hyphen_inline_size, line_box, box);
      } else {
        line_box->AddChild(item, std::move(item_result.shape_result),
                           item_result.TextOffset(), box->text_top,
                           item_result.inline_size, box->text_height,
                           item.BidiLevel());
      }
      has_logical_text_items = true;

      // Text boxes always need full paint invalidations.
      item.GetLayoutObject()->ClearNeedsLayoutWithFullPaintInvalidation();

    } else if (item.Type() == NGInlineItem::kControl) {
      PlaceControlItem(item, *line_info, &item_result, line_box, box);
      has_logical_text_items = true;
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = HandleOpenTag(item, item_result, line_box, box_states_);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      box = HandleCloseTag(item, item_result, line_box, box);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, *line_info, &item_result, line_box);
      has_relative_positioned_items |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == NGInlineItem::kListMarker) {
      PlaceListMarker(item, &item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      // An inline-level OOF child positions itself based on its direction, a
      // block-level OOF child positions itself based on the direction of its
      // block-level container.
      TextDirection direction =
          item.GetLayoutObject()->StyleRef().IsOriginalDisplayInlineType()
              ? item.Direction()
              : ConstraintSpace().Direction();

      line_box->AddChild(item.GetLayoutObject(), item.BidiLevel(), direction);
      has_out_of_flow_positioned_items = true;
    } else if (item.Type() == NGInlineItem::kFloating) {
      if (item_result.positioned_float) {
        if (scoped_refptr<const NGLayoutResult> layout_result =
                item_result.positioned_float->layout_result) {
          line_box->AddChild(std::move(layout_result),
                             item_result.positioned_float->bfc_offset,
                             item.BidiLevel());
        } else {
          // If we didn't produce a result, it means that we decided to push the
          // float to the next fragmentainer.
          DCHECK(ConstraintSpace().HasBlockFragmentation());
        }
      } else {
        line_box->AddChild(item.GetLayoutObject(), item.BidiLevel());
      }
      has_floating_items = true;
      has_relative_positioned_items |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == NGInlineItem::kBidiControl) {
      line_box->AddChild(item.BidiLevel());
    }
  }

  box_states_->OnEndPlaceItems(ConstraintSpace(), line_box, baseline_type_);

  if (UNLIKELY(Node().IsBidiEnabled())) {
    box_states_->PrepareForReorder(line_box);
    BidiReorder(line_info->BaseDirection(), line_box);
    box_states_->UpdateAfterReorder(line_box);
  } else {
    DCHECK(IsLtr(line_info->BaseDirection()));
  }
  const LayoutUnit hang_width = line_info->HangWidth();
  LayoutUnit inline_size;
  if (IsLtr(line_info->BaseDirection())) {
    inline_size = box_states_->ComputeInlinePositions(line_box, LayoutUnit());
  } else {
    inline_size = box_states_->ComputeInlinePositions(line_box, -hang_width);
    inline_size += hang_width;
  }
  if (UNLIKELY(hang_width)) {
    inline_size -= hang_width;
    container_builder_.SetHangInlineSize(hang_width);
  }

  // Truncate the line if 'text-overflow: ellipsis' is set, or for line-clamp.
  if (UNLIKELY((line_info->HasOverflow() &&
                node_.GetLayoutBlockFlow()->ShouldTruncateOverflowingText()) ||
               ConstraintSpace().LinesUntilClamp() == 1)) {
    NGLineTruncator truncator(*line_info);
    auto* input =
        DynamicTo<HTMLInputElement>(node_.GetLayoutBlockFlow()->GetNode());
    if (input && input->ShouldApplyMiddleEllipsis()) {
      inline_size =
          truncator.TruncateLineInTheMiddle(inline_size, line_box, box_states_);
    } else {
      inline_size = truncator.TruncateLine(inline_size, line_box, box_states_);
    }
  }

  if (has_logical_text_items &&
      !RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    line_box->CreateTextFragments(ConstraintSpace().GetWritingMode(),
                                  line_info->ItemsData().text_content);
  }

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  inline_size = inline_size.ClampNegativeToZero();

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  LayoutUnit bfc_line_offset =
      line_info->BfcOffset().line_offset + line_offset_for_text_align;

  if (IsLtr(line_info->BaseDirection()))
    bfc_line_offset += line_info->TextIndent();

  container_builder_.SetBfcLineOffset(bfc_line_offset);

  const FontHeight& line_box_metrics =
      UNLIKELY(Node().HasLineEvenIfEmpty())
          ? line_info->LineStyle().GetFontHeight()
          : box_states_->LineBoxState().metrics;

  // Place out-of-flow positioned objects.
  // This adjusts the NGLogicalLineItem::offset member to contain
  // the static position of the OOF positioned children relative to the linebox.
  if (has_out_of_flow_positioned_items)
    PlaceOutOfFlowObjects(*line_info, line_box_metrics, line_box);

  // Place floating objects.
  // This adjusts the  NGLogicalLineItem::offset member to
  // contain the position of the float relative to the linebox.
  // Additionally it will perform layout on any unpositioned floats which
  // needed the line height to correctly determine their final position.
  if (has_floating_items) {
    PlaceFloatingObjects(*line_info, line_box_metrics, opportunity, line_box,
                         exclusion_space);
  }

  // Apply any relative positioned offsets to *items* which have relative
  // positioning, (atomic-inlines, and floats). This will only move the
  // individual item.
  if (has_relative_positioned_items)
    PlaceRelativePositionedItems(line_box);

  // Apply any relative positioned offsets to any boxes (and their children).
  box_states_->ApplyRelativePositioning(ConstraintSpace(), line_box);

  NGAnnotationMetrics annotation_metrics;
  if (Node().HasRuby() &&
      !RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    annotation_metrics = ComputeAnnotationOverflow(*line_box, line_box_metrics,
                                                   -line_box_metrics.ascent,
                                                   line_info->LineStyle());
  }

  // Create box fragments if needed. After this point forward, |line_box| is a
  // tree structure.
  // The individual children don't move position within the |line_box|, rather
  // the children have their layout_result, fragment, (or similar) set to null,
  // creating a "hole" in the array.
  if (box_states_->HasBoxFragments())
    box_states_->CreateBoxFragments(line_box);

  // Update item index of the box states in the context.
  context_->SetItemIndex(line_info->ItemsData().items,
                         line_info->EndItemIndex());

  // Even if we have something in-flow, it may just be empty items that
  // shouldn't trigger creation of a line. Exit now if that's the case.
  if (line_info->IsEmptyLine()) {
    container_builder_.SetIsSelfCollapsing();
    container_builder_.SetIsEmptyLineBox();
    container_builder_.SetBaseDirection(line_info->BaseDirection());
    return;
  }

  DCHECK(!line_box_metrics.IsEmpty());

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box->MoveInBlockDirection(line_box_metrics.ascent);

  if (Node().HasRuby() &&
      RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    annotation_metrics = ComputeAnnotationOverflow(
        *line_box, line_box_metrics, LayoutUnit(), line_info->LineStyle());
  }
  LayoutUnit annotation_overflow_block_start;
  LayoutUnit annotation_overflow_block_end;
  LayoutUnit annotation_space_block_start;
  LayoutUnit annotation_space_block_end;
  if (!IsFlippedLinesWritingMode(line_info->LineStyle().GetWritingMode())) {
    annotation_overflow_block_start = annotation_metrics.overflow_over;
    annotation_overflow_block_end = annotation_metrics.overflow_under;
    annotation_space_block_start = annotation_metrics.space_over;
    annotation_space_block_end = annotation_metrics.space_under;
  } else {
    annotation_overflow_block_start = annotation_metrics.overflow_under;
    annotation_overflow_block_end = annotation_metrics.overflow_over;
    annotation_space_block_start = annotation_metrics.space_under;
    annotation_space_block_end = annotation_metrics.space_over;
  }

  LayoutUnit block_offset_shift = annotation_overflow_block_start;
  // If the previous line has block-end annotation overflow and this line has
  // block-start annotation space, shift up the block offset of this line.
  if (ConstraintSpace().BlockStartAnnotationSpace() < LayoutUnit() &&
      annotation_space_block_start) {
    const LayoutUnit overflow = -ConstraintSpace().BlockStartAnnotationSpace();
    block_offset_shift = -std::min(annotation_space_block_start, overflow);
  }

  // If this line has block-start annotation overflow and the previous line has
  // block-end annotation space, borrow the block-end space of the previous line
  // and shift down the block offset by |overflow - space|.
  if (annotation_overflow_block_start &&
      ConstraintSpace().BlockStartAnnotationSpace() > LayoutUnit()) {
    block_offset_shift = (annotation_overflow_block_start -
                          ConstraintSpace().BlockStartAnnotationSpace())
                             .ClampNegativeToZero();
  }

  if (line_info->UseFirstLineStyle())
    container_builder_.SetStyleVariant(NGStyleVariant::kFirstLine);
  container_builder_.SetBaseDirection(line_info->BaseDirection());
  container_builder_.SetInlineSize(inline_size);
  container_builder_.SetMetrics(line_box_metrics);
  container_builder_.SetBfcBlockOffset(line_info->BfcOffset().block_offset +
                                       block_offset_shift);
  if (annotation_overflow_block_end)
    container_builder_.SetAnnotationOverflow(annotation_overflow_block_end);
  else if (annotation_space_block_end)
    container_builder_.SetBlockEndAnnotationSpace(annotation_space_block_end);
}

void NGInlineLayoutAlgorithm::PlaceControlItem(const NGInlineItem& item,
                                               const NGLineInfo& line_info,
                                               NGInlineItemResult* item_result,
                                               NGLogicalLineItems* line_box,
                                               NGInlineBoxState* box) {
  DCHECK_EQ(item.Type(), NGInlineItem::kControl);
  DCHECK_GE(item.Length(), 1u);
  DCHECK(!item.TextShapeResult());
#if DCHECK_IS_ON()
  UChar character = line_info.ItemsData().text_content[item.StartOffset()];
  NGTextType text_type;
  switch (character) {
    case kNewlineCharacter:
      text_type = NGTextType::kForcedLineBreak;
      break;
    case kTabulationCharacter:
      text_type = NGTextType::kFlowControl;
      break;
    case kZeroWidthSpaceCharacter:
      text_type = NGTextType::kFlowControl;
      break;
    default:
      NOTREACHED();
      return;
  }
  DCHECK_EQ(item.TextType(), text_type);
#endif

  // Don't generate fragments if this is a generated (not in DOM) break
  // opportunity during the white space collapsing in NGInlineItemBuilder.
  if (UNLIKELY(item.IsGeneratedForLineBreak()))
    return;

  DCHECK(item.GetLayoutObject());
  DCHECK(item.GetLayoutObject()->IsText());
  ClearNeedsLayoutIfNeeded(item.GetLayoutObject());

  if (UNLIKELY(quirks_mode_ && !box->HasMetrics()))
    box->EnsureTextMetrics(*item.Style(), baseline_type_);

  line_box->AddChild(item, std::move(item_result->shape_result),
                     item_result->TextOffset(), box->text_top,
                     item_result->inline_size, box->text_height,
                     item.BidiLevel());
}

void NGInlineLayoutAlgorithm::PlaceHyphen(const NGInlineItemResult& item_result,
                                          LayoutUnit hyphen_inline_size,
                                          NGLogicalLineItems* line_box,
                                          NGInlineBoxState* box) {
  DCHECK(item_result.item);
  DCHECK(item_result.hyphen_string);
  DCHECK(item_result.hyphen_shape_result);
  DCHECK_EQ(hyphen_inline_size, item_result.HyphenInlineSize());
  const NGInlineItem& item = *item_result.item;
  line_box->AddChild(
      item, ShapeResultView::Create(item_result.hyphen_shape_result.get()),
      item_result.hyphen_string, box->text_top, hyphen_inline_size,
      box->text_height, item.BidiLevel());
}

NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    const NGLineInfo& line_info,
    NGInlineItemResult* item_result,
    NGLogicalLineItems* line_box) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  // const ComputedStyle& style = *item.Style();
  // position += item_result->margins.LineLeft(style.Direction());

  item_result->has_edge = true;
  NGInlineBoxState* box =
      box_states_->OnOpenTag(item, *item_result, baseline_type_, *line_box);
  PlaceLayoutResult(item_result, line_box, box, box->margin_inline_start);
  return box_states_->OnCloseTag(ConstraintSpace(), line_box, box,
                                 baseline_type_);
}

// Place a NGLayoutResult into the line box.
void NGInlineLayoutAlgorithm::PlaceLayoutResult(NGInlineItemResult* item_result,
                                                NGLogicalLineItems* line_box,
                                                NGInlineBoxState* box,
                                                LayoutUnit inline_offset) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->item);
  const NGInlineItem& item = *item_result->item;
  DCHECK(item.Style());
  FontHeight metrics =
      NGBoxFragment(ConstraintSpace().GetWritingDirection(),
                    To<NGPhysicalBoxFragment>(
                        item_result->layout_result->PhysicalFragment()))
          .BaselineMetrics(item_result->margins, baseline_type_);
  if (box)
    box->metrics.Unite(metrics);

  LayoutUnit line_top = item_result->margins.line_over - metrics.ascent;
  line_box->AddChild(std::move(item_result->layout_result),
                     LogicalOffset{inline_offset, line_top},
                     item_result->inline_size, /* children_count */ 0,
                     item.BidiLevel());
}

// Place all out-of-flow objects in |line_box_|.
void NGInlineLayoutAlgorithm::PlaceOutOfFlowObjects(
    const NGLineInfo& line_info,
    const FontHeight& line_box_metrics,
    NGLogicalLineItems* line_box) {
  DCHECK(line_info.IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  bool is_empty_inline = Node().IsEmptyInline();

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using NGLineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info.IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info.IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // The location of the "next" line.
  //
  // This uses NGConstraintSpace::Direction rather than
  // NGLineInfo::BaseDirection as this is for a block-level object rather than
  // an inline-level object.
  //
  // Similarly this uses the available size to determine which edge to align
  // to, and *does not* avoid floats.
  LayoutUnit block_level_line_location =
      IsLtr(ConstraintSpace().Direction())
          ? LayoutUnit()
          : ConstraintSpace().AvailableSize().inline_size;

  // This offset represents the position of the "next" line, relative to the
  // line we are currently creating, (this takes into account text-indent, etc).
  LayoutUnit block_level_inline_offset =
      block_level_line_location - (container_builder_.BfcLineOffset() -
                                   ConstraintSpace().BfcOffset().line_offset);

  // To correctly determine which "line" block-level out-of-flow positioned
  // object is placed on, we need to keep track of if there is any inline-level
  // content preceeding it.
  bool has_preceding_inline_level_content = false;
  bool has_rtl_block_level_out_of_flow_objects = false;
  bool is_ltr = IsLtr(line_info.BaseDirection());

  for (NGLogicalLineItem& child : *line_box) {
    has_preceding_inline_level_content |= child.HasInFlowFragment();

    const LayoutObject* box = child.out_of_flow_positioned_box;
    if (!box)
      continue;

    LogicalOffset static_offset(LayoutUnit(), baseline_adjustment);
    if (box->StyleRef().IsOriginalDisplayInlineType()) {
      // An inline-level OOF element positions itself within the line, at the
      // position it would have been if it was in-flow.
      static_offset.inline_offset = child.rect.offset.inline_offset;

      // The static-position of inline-level OOF-positioned nodes depends on
      // previous floats (if any).
      //
      // If we are an empty-inline we may not have the correct BFC block-offset
      // yet. Due to this we need to mark this node as having adjoining
      // objects, and perform a re-layout if our position shifts.
      if (is_empty_inline)
        container_builder_.AddAdjoiningObjectTypes(kAdjoiningInlineOutOfFlow);
    } else {
      // A block-level OOF element positions itself on the "next" line. However
      // only shifts down if there is preceding inline-level content.
      static_offset.inline_offset = block_level_inline_offset;
      if (is_ltr) {
        if (has_preceding_inline_level_content)
          static_offset.block_offset += line_height;
      } else {
        // "Preceding" is in logical order, but this loop is in visual order. In
        // RTL, move objects down in the reverse-order loop below.
        has_rtl_block_level_out_of_flow_objects = true;
      }
    }

    child.rect.offset = static_offset;
  }

  if (UNLIKELY(has_rtl_block_level_out_of_flow_objects)) {
    has_preceding_inline_level_content = false;
    for (NGLogicalLineItem& child : base::Reversed(*line_box)) {
      const LayoutObject* box = child.out_of_flow_positioned_box;
      if (!box) {
        has_preceding_inline_level_content |= child.HasInFlowFragment();
        continue;
      }
      if (has_preceding_inline_level_content &&
          !box->StyleRef().IsOriginalDisplayInlineType()) {
        child.rect.offset.block_offset += line_height;
      }
    }
  }
}

void NGInlineLayoutAlgorithm::PlaceFloatingObjects(
    const NGLineInfo& line_info,
    const FontHeight& line_box_metrics,
    const NGLineLayoutOpportunity& opportunity,
    NGLogicalLineItems* line_box,
    NGExclusionSpace* exclusion_space) {
  DCHECK(line_info.IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using NGLineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info.IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info.IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // Any unpositioned floats we encounter need to be placed on the "next" line.
  // This BFC block-offset represents the start of the "next" line.
  LayoutUnit origin_bfc_block_offset =
      opportunity.bfc_block_offset + line_height;

  LayoutUnit bfc_line_offset = container_builder_.BfcLineOffset();
  LayoutUnit bfc_block_offset = Node().IsEmptyInline()
                                    ? ConstraintSpace().ExpectedBfcBlockOffset()
                                    : line_info.BfcOffset().block_offset;

  for (NGLogicalLineItem& child : *line_box) {
    // We need to position any floats which should be on the "next" line now.
    // If this is an empty inline, all floats are positioned during the
    // PositionLeadingFloats step.
    if (child.unpositioned_float) {
      NGPositionedFloat positioned_float = PositionFloat(
          origin_bfc_block_offset, child.unpositioned_float, exclusion_space);
      if (positioned_float.need_break_before) {
        // We decided to break before the float. No fragment here. Create a
        // break token and propagate it to the block container.
        NGBlockNode float_node(ToLayoutBox(child.unpositioned_float));
        auto break_before = NGBlockBreakToken::CreateBreakBefore(
            float_node, /* is_forced_break */ false);
        context_->PropagateBreakToken(std::move(break_before));
        continue;
      } else {
        // If the float broke inside, we need to propagate the break token to
        // the block container, so that we'll resume in the next fragmentainer.
        if (scoped_refptr<const NGBreakToken> token =
                positioned_float.layout_result->PhysicalFragment().BreakToken())
          context_->PropagateBreakToken(To<NGBlockBreakToken>(token.get()));
        child.layout_result = std::move(positioned_float.layout_result);
        child.bfc_offset = positioned_float.bfc_offset;
        child.unpositioned_float = nullptr;
      }
    }

    // Skip any children which aren't positioned floats.
    if (!child.layout_result ||
        !child.layout_result->PhysicalFragment().IsFloating())
      continue;

    LayoutUnit block_offset =
        child.bfc_offset.block_offset - bfc_block_offset + baseline_adjustment;

    // We need to manually account for the flipped-lines writing mode here :(.
    if (IsFlippedLinesWritingMode(ConstraintSpace().GetWritingMode())) {
      NGFragment fragment(ConstraintSpace().GetWritingDirection(),
                          child.layout_result->PhysicalFragment());

      block_offset = -fragment.BlockSize() - block_offset;
    }

    child.rect.offset = {child.bfc_offset.line_offset - bfc_line_offset,
                         block_offset};
  }
}

void NGInlineLayoutAlgorithm::PlaceRelativePositionedItems(
    NGLogicalLineItems* line_box) {
  for (auto& child : *line_box) {
    const auto* physical_fragment = child.PhysicalFragment();
    if (!physical_fragment)
      continue;
    if (physical_fragment->IsText())
      continue;

    child.rect.offset += ComputeRelativeOffsetForInline(
        ConstraintSpace(), physical_fragment->Style());
  }
}

// Place a list marker.
void NGInlineLayoutAlgorithm::PlaceListMarker(const NGInlineItem& item,
                                              NGInlineItemResult* item_result,
                                              const NGLineInfo& line_info) {
  if (UNLIKELY(quirks_mode_)) {
    box_states_->LineBoxState().EnsureTextMetrics(*item.Style(),
                                                  baseline_type_);
  }

  container_builder_.SetUnpositionedListMarker(NGUnpositionedListMarker(
      ToLayoutNGOutsideListMarker(item.GetLayoutObject())));
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
base::Optional<LayoutUnit> NGInlineLayoutAlgorithm::ApplyJustify(
    LayoutUnit space,
    NGLineInfo* line_info) {
  // Empty lines should align to start.
  if (line_info->IsEmptyLine())
    return base::nullopt;

  // Justify the end of visible text, ignoring preserved trailing spaces.
  unsigned end_offset = line_info->EndOffsetForJustify();

  // If this line overflows, fallback to 'text-align: start'.
  if (space <= 0)
    return base::nullopt;

  // Can't justify an empty string.
  if (end_offset == line_info->StartOffset())
    return base::nullopt;

  // Construct the line text to compute spacing for.
  StringBuilder line_text_builder;
  line_text_builder.Append(StringView(line_info->ItemsData().text_content,
                                      line_info->StartOffset(),
                                      end_offset - line_info->StartOffset()));

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  DCHECK(!line_info->Results().IsEmpty());
  const NGInlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.hyphen_string)
    line_text_builder.Append(last_item_result.hyphen_string);

  // Compute the spacing to justify.
  String line_text = line_text_builder.ToString();
  DCHECK_GT(line_text.length(), 0u);

  ShapeResultSpacing<String> spacing(line_text);
  spacing.SetExpansion(space, line_info->BaseDirection(),
                       line_info->LineStyle().GetTextJustify());
  const LayoutObject* box = Node().GetLayoutBox();
  if (!spacing.HasExpansion()) {
    // See AdjustInlineDirectionLineBounds() of LayoutRubyBase and
    // LayoutRubyText.
    if (box && (box->IsRubyText() || box->IsRubyBase()))
      return space / 2;
    return base::nullopt;
  }

  LayoutUnit inset;
  // See AdjustInlineDirectionLineBounds() of LayoutRubyBase and
  // LayoutRubyText.
  if (box && (box->IsRubyText() || box->IsRubyBase())) {
    unsigned count = std::min(spacing.ExpansionOppotunityCount(),
                              static_cast<unsigned>(LayoutUnit::Max().Floor()));
    // Inset the ruby base/text by half the inter-ideograph expansion amount.
    inset = space / (count + 1);
    // For ruby text,  inset it by no more than a full-width ruby character on
    // each side.
    if (box->IsRubyText()) {
      inset =
          std::min(LayoutUnit(2 * line_info->LineStyle().FontSize()), inset);
    }
    spacing.SetExpansion(space - inset, line_info->BaseDirection(),
                         line_info->LineStyle().GetTextJustify());
  }

  for (NGInlineItemResult& item_result : *line_info->MutableResults()) {
    if (item_result.has_only_trailing_spaces)
      break;
    if (item_result.shape_result) {
      scoped_refptr<ShapeResult> shape_result =
          item_result.shape_result->CreateShapeResult();
      DCHECK_GE(item_result.StartOffset(), line_info->StartOffset());
      DCHECK_EQ(shape_result->NumCharacters(), item_result.Length());
      shape_result->ApplySpacing(spacing, item_result.StartOffset() -
                                              line_info->StartOffset() -
                                              shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      if (UNLIKELY(item_result.hyphen_shape_result))
        item_result.inline_size += item_result.HyphenInlineSize();
      item_result.shape_result = ShapeResultView::Create(shape_result.get());
    } else if (item_result.item->Type() == NGInlineItem::kAtomicInline) {
      float offset = 0.f;
      DCHECK_LE(line_info->StartOffset(), item_result.StartOffset());
      unsigned line_text_offset =
          item_result.StartOffset() - line_info->StartOffset();
      DCHECK_EQ(kObjectReplacementCharacter, line_text[line_text_offset]);
      item_result.inline_size +=
          spacing.ComputeSpacing(line_text_offset, 0.0, offset);
      // |offset| is non-zero only before CJK characters.
      DCHECK_EQ(offset, 0.f);
    }
  }
  return inset / 2;
}

// Apply the 'text-align' property to |line_info|. Returns the amount to move
// the line in the inline direction.
LayoutUnit NGInlineLayoutAlgorithm::ApplyTextAlign(NGLineInfo* line_info) {
  // NGLineInfo::WidthForAlignment may return a negative value, as text-indent
  // can accept negative values. We need to use this un-clamped value for
  // alginment, instead of just NGLineInfo::Width.
  LayoutUnit space =
      line_info->AvailableWidth() - line_info->WidthForAlignment();

  ETextAlign text_align = line_info->TextAlign();
  if (text_align == ETextAlign::kJustify) {
    base::Optional<LayoutUnit> offset = ApplyJustify(space, line_info);
    if (offset)
      return *offset;

    // If justification fails, fallback to 'text-align: start'.
    text_align = ETextAlign::kStart;
  }

  return LineOffsetForTextAlign(text_align, line_info->BaseDirection(), space);
}

LayoutUnit NGInlineLayoutAlgorithm::ComputeContentSize(
    const NGLineInfo& line_info,
    const NGExclusionSpace& exclusion_space,
    LayoutUnit line_height) {
  LayoutUnit content_size = line_height;

  const NGInlineItemResults& line_items = line_info.Results();
  if (line_items.IsEmpty())
    return content_size;

  // If the last item was a <br> we need to adjust the content_size to clear
  // floats if specified. The <br> element must be at the back of the item
  // result list as it forces a line to break.
  const NGInlineItemResult& item_result = line_items.back();
  DCHECK(item_result.item);
  const NGInlineItem& item = *item_result.item;
  const LayoutObject* layout_object = item.GetLayoutObject();

  // layout_object may be null in certain cases, e.g. if it's a kBidiControl.
  if (layout_object && layout_object->IsBR()) {
    NGBfcOffset bfc_offset = {ContainerBfcOffset().line_offset,
                              ContainerBfcOffset().block_offset + content_size};
    AdjustToClearance(
        exclusion_space.ClearanceOffset(item.Style()->Clear(Style())),
        &bfc_offset);
    content_size = bfc_offset.block_offset - ContainerBfcOffset().block_offset;
  }

  return content_size;
}

scoped_refptr<const NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  NGExclusionSpace initial_exclusion_space(ConstraintSpace().ExclusionSpace());

  const bool is_empty_inline = Node().IsEmptyInline();

  if (is_empty_inline) {
    // Margins should collapse across "certain zero-height line boxes".
    // https://drafts.csswg.org/css2/box.html#collapsing-margins
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());

    // We're just going to collapse through this one, so whatever went in on one
    // side will go out on the other side. The position of the adjoining objects
    // will be affected by any subsequent block, until the BFC block offset is
    // resolved.
    container_builder_.AddAdjoiningObjectTypes(
        ConstraintSpace().AdjoiningObjectTypes());

    // For the empty lines, most of the logic here are not necessary, but in
    // some edge cases we still need to create box fragments, such as when it
    // has a containing block for out of flow objects. For now, use the code
    // path than to create a fast code path for the stability.
  } else {
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());

    // The BFC block-offset was determined before entering this algorithm. This
    // means that there should be no adjoining objects.
    DCHECK(!ConstraintSpace().AdjoiningObjectTypes());
  }

  // In order to get the correct list of layout opportunities, we need to
  // position any "leading" floats within the exclusion space first.
  STACK_UNINITIALIZED NGPositionedFloatVector leading_floats;
  unsigned handled_leading_floats_index =
      PositionLeadingFloats(&initial_exclusion_space, &leading_floats);

  // Only empty-inlines should have the "forced" BFC block-offset set.
  DCHECK(is_empty_inline || !ConstraintSpace().ForcedBfcBlockOffset());

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  const LayoutOpportunityVector& opportunities =
      initial_exclusion_space.AllLayoutOpportunities(
          {ConstraintSpace().BfcOffset().line_offset,
           is_empty_inline ? ConstraintSpace().ExpectedBfcBlockOffset()
                           : ConstraintSpace().BfcOffset().block_offset},
          ConstraintSpace().AvailableSize().inline_size);

  NGExclusionSpace exclusion_space;
  const NGInlineBreakToken* break_token = BreakToken();

  NGFragmentItemsBuilder* items_builder = context_->ItemsBuilder();
  NGLogicalLineItems* line_box = items_builder
                                     ? items_builder->AcquireLogicalLineItems()
                                     : context_->LogicalLineItems();

  bool is_line_created = false;
  LayoutUnit line_block_size;
  LayoutUnit block_delta;
  const auto* opportunities_it = opportunities.begin();
  while (opportunities_it != opportunities.end()) {
    const NGLayoutOpportunity& opportunity = *opportunities_it;

#if DCHECK_IS_ON()
    // Make sure the last opportunity has the correct properties.
    if (opportunities_it + 1 == opportunities.end()) {
      // We shouldn't have any shapes affecting the last opportunity.
      DCHECK(!opportunity.HasShapeExclusions());
      DCHECK_EQ(line_block_size, LayoutUnit());
      DCHECK_EQ(block_delta, LayoutUnit());

      // The opportunity should match the given available size, (however need
      // to check if the inline-size got saturated first).
      if (opportunity.rect.InlineSize() != LayoutUnit::Max()) {
        DCHECK_EQ(opportunity.rect.InlineSize(),
                  ConstraintSpace().AvailableSize().inline_size);
      }
      DCHECK_EQ(opportunity.rect.BlockSize(), LayoutUnit::Max());
    }
#endif

    // Reset any state that may have been modified in a previous pass.
    container_builder_.Reset();
    exclusion_space = initial_exclusion_space;
    is_line_created = false;

    NGLineLayoutOpportunity line_opportunity =
        opportunity.ComputeLineLayoutOpportunity(ConstraintSpace(),
                                                 line_block_size, block_delta);

    STACK_UNINITIALIZED NGLineInfo line_info;
    NGLineBreaker line_breaker(Node(), NGLineBreakerMode::kContent,
                               ConstraintSpace(), line_opportunity,
                               leading_floats, handled_leading_floats_index,
                               break_token, &exclusion_space);
    line_breaker.NextLine(&line_info);

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size, and the
    // container autowraps, continue to the next opportunity.
    if (line_info.HasOverflow() &&
        !line_opportunity.IsEqualToAvailableFloatInlineSize(
            ConstraintSpace().AvailableSize().inline_size) &&
        Node().Style().AutoWrap()) {
      // Shapes are *special*. We need to potentially increment the block-delta
      // by 1px each loop to properly test each potential position of the line.
      if (UNLIKELY(opportunity.HasShapeExclusions()) &&
          block_delta < opportunity.rect.BlockSize() &&
          !opportunity.IsBlockDeltaBelowShapes(block_delta)) {
        block_delta += LayoutUnit(1);
        line_block_size = LayoutUnit();
        continue;
      }
      // We've either don't have any shapes, or run out of block-delta space
      // to test, proceed to the next layout opportunity.
      if (opportunities_it + 1 != opportunities.end()) {
        block_delta = LayoutUnit();
        line_block_size = LayoutUnit();
        ++opportunities_it;
        continue;
      }
      // Normally the last opportunity should fit the line, but arithmetic
      // overflow can lead to failures for all opportunities. Just let the line
      // to overflow in that case.
    }

    PrepareBoxStates(line_info, break_token);
    CreateLine(line_opportunity, &line_info, line_box, &exclusion_space);
    is_line_created = true;

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity.
    LayoutUnit line_height = container_builder_.LineHeight();

    // Now that we have the block-size of the line, we can re-test the layout
    // opportunity to see if we fit into the (potentially) non-rectangular
    // shape area.
    //
    // If the AvailableInlineSize changes we need to run the line breaker again
    // with the calculated line_block_size. This is *safe* as the line breaker
    // won't produce a line which has a larger block-size, (as it can only
    // decrease or stay the same size).
    //
    // We skip attempting to fit empty lines into the shape area, as they
    // should only contain floats and/or abs-pos which shouldn't be affected by
    // this logic.
    if (UNLIKELY(opportunity.HasShapeExclusions() &&
                 !line_info.IsEmptyLine())) {
      NGLineLayoutOpportunity line_opportunity_with_height =
          opportunity.ComputeLineLayoutOpportunity(ConstraintSpace(),
                                                   line_height, block_delta);

      if (line_opportunity_with_height.AvailableInlineSize() !=
          line_opportunity.AvailableInlineSize()) {
        line_block_size = line_height;
        continue;
      }
    }

    // Check if the line will fit in the current opportunity.
    if (line_height + block_delta > opportunity.rect.BlockSize()) {
      block_delta = LayoutUnit();
      line_block_size = LayoutUnit();
      ++opportunities_it;
      continue;
    }

    // Success!
    container_builder_.SetBreakToken(line_breaker.CreateBreakToken(line_info));

    // Propagate any break tokens for floats that we fragmented before or inside
    // to the block container.
    for (scoped_refptr<const NGBlockBreakToken> float_break_token :
         line_breaker.PropagatedBreakTokens())
      context_->PropagateBreakToken(std::move(float_break_token));

    if (is_empty_inline) {
      DCHECK_EQ(container_builder_.BlockSize(), 0);
    } else {
      // A <br clear=both> will strech the line-box height, such that the
      // block-end edge will clear any floats.
      // TODO(ikilpatrick): Move this into ng_block_layout_algorithm.
      container_builder_.SetBlockSize(
          ComputeContentSize(line_info, exclusion_space, line_height));

      // As we aren't an empty inline we should have correctly placed all
      // our adjoining objects, and shouldn't propagate this information
      // to siblings.
      container_builder_.ResetAdjoiningObjectTypes();

      if (opportunity.rect.BlockStartOffset() >
          ConstraintSpace().BfcOffset().block_offset)
        container_builder_.SetIsPushedByFloats();
    }
    break;
  }

  CHECK(is_line_created);
  container_builder_.SetExclusionSpace(std::move(exclusion_space));

  if (items_builder) {
    DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
    container_builder_.PropagateChildrenData(*line_box);
    scoped_refptr<const NGLayoutResult> layout_result =
        container_builder_.ToLineBoxFragment();
    items_builder->AssociateLogicalLineItems(line_box,
                                             layout_result->PhysicalFragment());
    return layout_result;
  }

  DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  container_builder_.AddChildren(*line_box);
  container_builder_.MoveOutOfFlowDescendantCandidatesToDescendants();
  return container_builder_.ToLineBoxFragment();
}

// This positions any "leading" floats within the given exclusion space.
// If we are also an empty inline, it will add any out-of-flow descendants.
unsigned NGInlineLayoutAlgorithm::PositionLeadingFloats(
    NGExclusionSpace* exclusion_space,
    NGPositionedFloatVector* positioned_floats) {
  bool is_empty_inline = Node().IsEmptyInline();

  const Vector<NGInlineItem>& items =
      Node().ItemsData(/* is_first_line */ false).items;

  unsigned index = BreakToken() ? BreakToken()->ItemIndex() : 0;
  for (; index < items.size(); ++index) {
    const NGInlineItem& item = items[index];

    // Abort if we've found something that makes this a non-empty inline.
    if (!item.IsEmptyItem()) {
      DCHECK(!is_empty_inline);
      break;
    }

    if (item.Type() != NGInlineItem::kFloating)
      continue;

    container_builder_.AddAdjoiningObjectTypes(
        item.GetLayoutObject()->StyleRef().Floating(
            ConstraintSpace().Direction()) == EFloat::kLeft
            ? kAdjoiningFloatLeft
            : kAdjoiningFloatRight);

    // Place any floats at the "expected" BFC block-offset, this may be an
    // optimistic guess.
    LayoutUnit origin_bfc_block_offset =
        is_empty_inline ? ConstraintSpace().ExpectedBfcBlockOffset()
                        : ConstraintSpace().BfcOffset().block_offset;

    NGPositionedFloat positioned_float = PositionFloat(
        origin_bfc_block_offset, item.GetLayoutObject(), exclusion_space);

    if (ConstraintSpace().HasBlockFragmentation()) {
      // Propagate any breaks before or inside floats to the block container.
      if (positioned_float.need_break_before) {
        NGBlockNode float_node(ToLayoutBox(item.GetLayoutObject()));
        auto break_before = NGBlockBreakToken::CreateBreakBefore(
            float_node, /* is_forced_break */ false);
        context_->PropagateBreakToken(std::move(break_before));
        positioned_float.layout_result = nullptr;
      } else if (scoped_refptr<const NGBreakToken> token =
                     positioned_float.layout_result->PhysicalFragment()
                         .BreakToken()) {
        context_->PropagateBreakToken(To<NGBlockBreakToken>(token.get()));
      }
    }

    positioned_floats->push_back(std::move(positioned_float));
  }

  return index;
}

NGPositionedFloat NGInlineLayoutAlgorithm::PositionFloat(
    LayoutUnit origin_bfc_block_offset,
    LayoutObject* floating_object,
    NGExclusionSpace* exclusion_space) const {
  NGBfcOffset origin_bfc_offset = {ConstraintSpace().BfcOffset().line_offset,
                                   origin_bfc_block_offset};

  NGUnpositionedFloat unpositioned_float(
      NGBlockNode(ToLayoutBox(floating_object)),
      /* break_token */ nullptr, ConstraintSpace().AvailableSize(),
      ConstraintSpace().PercentageResolutionSize(),
      ConstraintSpace().ReplacedPercentageResolutionSize(), origin_bfc_offset,
      ConstraintSpace(), Style());

  return ::blink::PositionFloat(&unpositioned_float, exclusion_space);
}

void NGInlineLayoutAlgorithm::BidiReorder(TextDirection base_direction,
                                          NGLogicalLineItems* line_box) {
  if (line_box->IsEmpty())
    return;

  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // A sentinel value for items that are opaque to bidi reordering. Should be
  // larger than the maximum resolved level.
  constexpr UBiDiLevel kOpaqueBidiLevel = 0xff;
  DCHECK_GT(kOpaqueBidiLevel, UBIDI_MAX_EXPLICIT_LEVEL + 1);

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  Vector<UBiDiLevel, 32> levels;
  levels.ReserveInitialCapacity(line_box->size());
  bool has_opaque_items = false;
  for (NGLogicalLineItem& item : *line_box) {
    if (item.IsOpaqueToBidiReordering()) {
      levels.push_back(kOpaqueBidiLevel);
      has_opaque_items = true;
      continue;
    }
    DCHECK_NE(item.bidi_level, kOpaqueBidiLevel);
    levels.push_back(item.bidi_level);
  }

  // For opaque items, copy bidi levels from adjacent items.
  if (has_opaque_items) {
    // Use the paragraph level for trailing opaque items.
    UBiDiLevel last_level = IsLtr(base_direction) ? 0 : 1;
    for (UBiDiLevel& level : base::Reversed(levels)) {
      if (level == kOpaqueBidiLevel)
        level = last_level;
      else
        last_level = level;
    }
  }

  // Compute visual indices from resolved levels.
  Vector<int32_t, 32> indices_in_visual_order(levels.size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  NGLogicalLineItems visual_items;
  visual_items.ReserveInitialCapacity(line_box->size());
  for (unsigned logical_index : indices_in_visual_order) {
    visual_items.AddChild(std::move((*line_box)[logical_index]));
    DCHECK(!(*line_box)[logical_index].HasInFlowFragment() ||
           // |inline_item| will not be null by moving.
           (*line_box)[logical_index].inline_item);
  }
  DCHECK_EQ(line_box->size(), visual_items.size());
  *line_box = std::move(visual_items);
}

}  // namespace blink
