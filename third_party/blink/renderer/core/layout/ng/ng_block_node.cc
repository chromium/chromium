// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_marquee_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

namespace {

inline LayoutMultiColumnFlowThread* GetFlowThread(const LayoutBox& box) {
  if (!box.IsLayoutBlockFlow())
    return nullptr;
  return ToLayoutBlockFlow(box).MultiColumnFlowThread();
}

#define WITH_ALGORITHM(ret, func, argdecl, args)                             \
  ret func##WithAlgorithm(NGBlockNode node, const NGConstraintSpace& space,  \
                          const NGBreakToken* break_token, argdecl) {        \
    const auto* token = ToNGBlockBreakToken(break_token);                    \
    const ComputedStyle& style = node.Style();                               \
    if (node.GetLayoutBox()->IsLayoutNGFlexibleBox())                        \
      return NGFlexLayoutAlgorithm(node, space, token).func args;            \
    if (node.GetLayoutBox()->IsLayoutNGFieldset())                           \
      return NGFieldsetLayoutAlgorithm(node, space, token).func args;        \
    /* If there's a legacy layout box, we can only do block fragmentation if \
     * we would have done block fragmentation with the legacy engine.        \
     * Otherwise writing data back into the legacy tree will fail. Look for  \
     * the flow thread. */                                                   \
    if (GetFlowThread(*node.GetLayoutBox())) {                               \
      if (style.IsOverflowPaged())                                           \
        return NGPageLayoutAlgorithm(node, space, token).func args;          \
      if (style.SpecifiesColumns())                                          \
        return NGColumnLayoutAlgorithm(node, space, token).func args;        \
      NOTREACHED();                                                          \
    }                                                                        \
    return NGBlockLayoutAlgorithm(node, space, token).func args;             \
  }

WITH_ALGORITHM(scoped_refptr<NGLayoutResult>, Layout, void*, ())
WITH_ALGORITHM(base::Optional<MinMaxSize>,
               ComputeMinMaxSize,
               MinMaxSizeInput input,
               (input))

#undef WITH_ALGORITHM

bool IsFloatFragment(const NGPhysicalFragment& fragment) {
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  return layout_object && layout_object->IsFloating() && fragment.IsBox();
}

void UpdateLegacyMultiColumnFlowThread(
    NGBlockNode node,
    LayoutMultiColumnFlowThread* flow_thread,
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& fragment) {
  WritingMode writing_mode = constraint_space.GetWritingMode();
  LayoutUnit flow_end;
  LayoutUnit column_block_size;
  bool has_processed_first_child = false;

  // Stitch the columns together.
  for (const auto& child : fragment.Children()) {
    NGFragment child_fragment(writing_mode, *child);
    flow_end += child_fragment.BlockSize();
    // Non-uniform fragmentainer widths not supported by legacy layout.
    DCHECK(!has_processed_first_child ||
           flow_thread->LogicalWidth() == child_fragment.InlineSize());
    if (!has_processed_first_child) {
      // The offset of the flow thread should be the same as that of the first
      // first column.
      flow_thread->SetX(child.Offset().left);
      flow_thread->SetY(child.Offset().top);
      flow_thread->SetLogicalWidth(child_fragment.InlineSize());
      column_block_size = child_fragment.BlockSize();
      has_processed_first_child = true;
    }
  }

  if (LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet()) {
    NGFragment logical_fragment(writing_mode, fragment);
    auto border_scrollbar_padding =
        CalculateBorderScrollbarPadding(constraint_space, node);

    column_set->SetLogicalLeft(border_scrollbar_padding.inline_start);
    column_set->SetLogicalTop(border_scrollbar_padding.block_start);
    column_set->SetLogicalWidth(logical_fragment.InlineSize() -
                                border_scrollbar_padding.InlineSum());
    column_set->SetLogicalHeight(column_block_size);
    column_set->EndFlow(flow_end);
  }
  // TODO(mstensho): Update all column boxes, not just the first column set
  // (like we do above). This is needed to support column-span:all.

  flow_thread->UpdateFromNG();
  flow_thread->ValidateColumnSets();
  flow_thread->SetLogicalHeight(flow_end);
  flow_thread->UpdateAfterLayout();
  flow_thread->ClearNeedsLayout();
}

NGConstraintSpaceBuilder CreateConstraintSpaceBuilderForMinMax(
    NGBlockNode node,
    NGPhysicalSize icb_size) {
  return NGConstraintSpaceBuilder(node.Style().GetWritingMode(), icb_size)
      .SetTextDirection(node.Style().Direction())
      .SetIsIntermediateLayout(true)
      .SetIsNewFormattingContext(node.CreatesNewFormattingContext())
      .SetFloatsBfcBlockOffset(LayoutUnit());
}

LayoutUnit CalculateAvailableInlineSizeForLegacy(
    const LayoutBox& box,
    const NGConstraintSpace& space) {
  if (box.StyleRef().LogicalWidth().IsPercent()) {
    if (box.ShouldComputeSizeAsReplaced())
      return space.ReplacedPercentageResolutionSize().inline_size;

    return space.PercentageResolutionSize().inline_size;
  }

  return space.AvailableSize().inline_size;
}

LayoutUnit CalculateAvailableBlockSizeForLegacy(
    const LayoutBox& box,
    const NGConstraintSpace& space) {
  if (box.StyleRef().LogicalHeight().IsPercent()) {
    if (box.ShouldComputeSizeAsReplaced())
      return space.ReplacedPercentageResolutionSize().block_size;

    return space.PercentageResolutionSize().block_size;
  }

  return space.AvailableSize().block_size;
}

}  // namespace

scoped_refptr<NGLayoutResult> NGBlockNode::Layout(
    const NGConstraintSpace& constraint_space,
    const NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    return RunOldLayout(constraint_space);
  }

  LayoutBlockFlow* block_flow =
      box_->IsLayoutNGMixin() ? ToLayoutBlockFlow(box_) : nullptr;
  NGLayoutInputNode first_child = FirstChild();
  scoped_refptr<NGLayoutResult> layout_result;
  if (block_flow) {
    layout_result =
        block_flow->CachedLayoutResult(constraint_space, break_token);
    if (layout_result) {
      // TODO(layoutng): Figure out why these two call can't be inside the
      // !constraint_space.IsIntermediateLayout() block below.
      UpdateShapeOutsideInfoIfNeeded(
          constraint_space.PercentageResolutionSize().inline_size);
      // We may need paint invalidation even if we can reuse layout, as our
      // paint offset/visual rect may have changed due to relative
      // positioning changes. Otherwise we fail fast/css/
      // fast/css/relative-positioned-block-with-inline-ancestor-and-parent
      // -dynamic.html
      // TODO(layoutng): See if we can optimize this. When we natively
      // support relative positioning in NG we can probably remove this,
      box_->SetShouldCheckForPaintInvalidation();

      // We have to re-set the cached result here, because it is used for
      // LayoutNGMixin::CurrentFragment and therefore has to be up-to-date.
      // In particular, that fragment would have an incorrect offset if we
      // don't re-set the result here.
      block_flow->SetCachedLayoutResult(constraint_space, break_token,
                                        *layout_result);
      if (!constraint_space.IsIntermediateLayout() && first_child &&
          first_child.IsInline()) {
        block_flow->UpdatePaintFragmentFromCachedLayoutResult(
            break_token, layout_result->PhysicalFragment(),
            layout_result->Offset());
      }
      return layout_result;
    }
  }

  // This follows the code from LayoutBox::UpdateLogicalWidth
  if (box_->NeedsPreferredWidthsRecalculation() &&
      !box_->PreferredLogicalWidthsDirty()) {
    // Laying out this object means that its containing block is also being
    // laid out. This object is special, in that its min/max widths depend on
    // the ancestry (min/max width calculation should ideally be strictly
    // bottom-up, but that's not always the case), so since the containing
    // block size may have changed, we need to recalculate the min/max widths
    // of this object, and every child that has the same issue, recursively.
    box_->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    // Since all this takes place during actual layout, instead of being part
    // of min/max the width calculation machinery, we need to enter said
    // machinery here, to make sure that what was dirtied is actualy
    // recalculated. Leaving things dirty would mean that any subsequent
    // dirtying of descendants would fail.
    box_->ComputePreferredLogicalWidths();
  }

  PrepareForLayout();

  NGBoxStrut old_scrollbars = GetScrollbarSizes();
  layout_result = LayoutWithAlgorithm(*this, constraint_space, break_token,
                                      /* ignored */ nullptr);

  FinishLayout(constraint_space, break_token, layout_result);
  if (old_scrollbars != GetScrollbarSizes()) {
    // If our scrollbars have changed, we need to relayout because either:
    // - Our size has changed (if shrinking to fit), or
    // - Space available to our children has changed.
    // This mirrors legacy code in PaintLayerScrollableArea::UpdateAfterLayout.
    // TODO(cbiesinger): It seems that we should also check if
    // PreferredLogicalWidthsDirty() has changed from false to true during
    // layout, so that we correctly size ourselves when shrinking to fit
    // and a child gained a vertical scrollbar. However, no test fails
    // without that check.
    PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars;
    layout_result = LayoutWithAlgorithm(*this, constraint_space, break_token,
                                        /* ignored */ nullptr);
    FinishLayout(constraint_space, break_token, layout_result);
  }

  return layout_result;
}

void NGBlockNode::PrepareForLayout() {
  if (box_->IsLayoutBlock()) {
    LayoutBlock* block = ToLayoutBlock(box_);
    if (block->HasOverflowClip()) {
      DCHECK(block->GetScrollableArea());
      if (block->GetScrollableArea()->ShouldPerformScrollAnchoring())
        block->GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();
    }
  }

  if (IsListItem())
    ToLayoutNGListItem(box_)->UpdateMarkerTextIfNeeded();
}

void NGBlockNode::FinishLayout(const NGConstraintSpace& constraint_space,
                               const NGBreakToken* break_token,
                               scoped_refptr<NGLayoutResult> layout_result) {
  if (!IsBlockLayoutComplete(constraint_space, *layout_result))
    return;

  DCHECK(layout_result->PhysicalFragment());

  if (box_->IsLayoutNGMixin()) {
    LayoutBlockFlow* block_flow = ToLayoutBlockFlow(box_);
    block_flow->SetCachedLayoutResult(constraint_space, break_token,
                                      *layout_result);
    NGLayoutInputNode first_child = FirstChild();
    bool has_inline_children = first_child && first_child.IsInline();
    if (has_inline_children || box_->IsLayoutNGFieldset()) {
      if (has_inline_children) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()),
            layout_result->PhysicalFragment()->Size().width,
            Style().IsFlippedBlocksWritingMode());
      }

      block_flow->SetPaintFragment(break_token,
                                   layout_result->PhysicalFragment(),
                                   layout_result->Offset());
    } else {
      // We still need to clear paint fragments in case it had inline children,
      // and thus had NGPaintFragment.
      block_flow->SetPaintFragment(break_token, nullptr, NGPhysicalOffset());
    }
  }

  CopyFragmentDataToLayoutBox(constraint_space, *layout_result);
}

MinMaxSize NGBlockNode::ComputeMinMaxSize(
    WritingMode container_writing_mode,
    const MinMaxSizeInput& input,
    const NGConstraintSpace* constraint_space) {
  bool is_orthogonal_flow_root =
      !IsParallelWritingMode(container_writing_mode, Style().GetWritingMode());

  MinMaxSize sizes;
  // If we're orthogonal, we have to run layout to compute the sizes. However,
  // if we're outside of layout, we can't do that. This can happen on Mac.
  if ((!CanUseNewLayout() && !is_orthogonal_flow_root) ||
      (is_orthogonal_flow_root && !box_->GetFrameView()->IsInPerformLayout())) {
    return ComputeMinMaxSizeFromLegacy(input.size_type);
  }

  NGPhysicalSize icb_size = constraint_space
                                ? constraint_space->InitialContainingBlockSize()
                                : InitialContainingBlockSize();
  NGConstraintSpace zero_constraint_space =
      CreateConstraintSpaceBuilderForMinMax(*this, icb_size)
          .ToConstraintSpace(Style().GetWritingMode());

  if (!constraint_space) {
    // Using the zero-sized constraint space when measuring for an orthogonal
    // flow root isn't going to give the right result.
    DCHECK(!is_orthogonal_flow_root);

    constraint_space = &zero_constraint_space;
  }

  if (is_orthogonal_flow_root || !CanUseNewLayout()) {
    scoped_refptr<NGLayoutResult> layout_result = Layout(*constraint_space);
    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
    NGBoxFragment fragment(
        container_writing_mode,
        TextDirection::kLtr,  // irrelevant here
        ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
    sizes.min_size = sizes.max_size = fragment.Size().inline_size;
    if (input.size_type == NGMinMaxSizeType::kContentBoxSize) {
      sizes -= fragment.Borders().InlineSum() + fragment.Padding().InlineSum() +
               box_->ScrollbarLogicalWidth();
      DCHECK_GE(sizes.min_size, LayoutUnit());
      DCHECK_GE(sizes.max_size, LayoutUnit());
    }
    return sizes;
  }

  base::Optional<MinMaxSize> maybe_sizes =
      ComputeMinMaxSizeWithAlgorithm(*this, *constraint_space,
                                     /* break token */ nullptr, input);
  if (maybe_sizes.has_value()) {
    if (UNLIKELY(IsHTMLMarqueeElement(box_->GetNode()) &&
                 ToHTMLMarqueeElement(box_->GetNode())->IsHorizontal()))
      maybe_sizes->min_size = LayoutUnit();
    return *maybe_sizes;
  }

  if (!box_->GetFrameView()->IsInPerformLayout()) {
    // We can't synthesize these using Layout() if we're not in PerformLayout.
    // This situation can happen on mac. Fall back to legacy instead.
    return ComputeMinMaxSizeFromLegacy(input.size_type);
  }

  // Have to synthesize this value.
  scoped_refptr<NGLayoutResult> layout_result = Layout(zero_constraint_space);
  NGBoxFragment min_fragment(
      container_writing_mode,
      TextDirection::kLtr,  // irrelevant here
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
  sizes.min_size = min_fragment.Size().inline_size;

  // Now, redo with infinite space for max_content
  NGConstraintSpace infinite_constraint_space =
      CreateConstraintSpaceBuilderForMinMax(*this, icb_size)
          .SetAvailableSize({LayoutUnit::Max(), LayoutUnit()})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .ToConstraintSpace(Style().GetWritingMode());

  layout_result = Layout(infinite_constraint_space);
  NGBoxFragment max_fragment(
      container_writing_mode,
      TextDirection::kLtr,  // irrelevant here
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
  sizes.max_size = max_fragment.Size().inline_size;

  if (input.size_type == NGMinMaxSizeType::kContentBoxSize) {
    sizes -= max_fragment.Borders().InlineSum() +
             max_fragment.Padding().InlineSum() + box_->ScrollbarLogicalWidth();
    DCHECK_GE(sizes.min_size, LayoutUnit());
    DCHECK_GE(sizes.max_size, LayoutUnit());
  }
  return sizes;
}

MinMaxSize NGBlockNode::ComputeMinMaxSizeFromLegacy(
    NGMinMaxSizeType type) const {
  MinMaxSize sizes;
  // ComputeIntrinsicLogicalWidths returns content-box + scrollbar.
  box_->ComputeIntrinsicLogicalWidths(sizes.min_size, sizes.max_size);
  if (type == NGMinMaxSizeType::kContentBoxSize) {
    sizes -= LayoutUnit(box_->ScrollbarLogicalWidth());
    DCHECK_GE(sizes.min_size, LayoutUnit());
    DCHECK_GE(sizes.max_size, LayoutUnit());
  } else {
    sizes += box_->BorderAndPaddingLogicalWidth();
  }
  return sizes;
}

NGBoxStrut NGBlockNode::GetScrollbarSizes() const {
  NGPhysicalBoxStrut sizes;
  const ComputedStyle& style = box_->StyleRef();
  if (!style.IsOverflowVisible()) {
    LayoutUnit vertical = LayoutUnit(box_->VerticalScrollbarWidth());
    LayoutUnit horizontal = LayoutUnit(box_->HorizontalScrollbarHeight());
    sizes.bottom = horizontal;
    if (box_->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
      sizes.left = vertical;
    else
      sizes.right = vertical;
  }
  return sizes.ConvertToLogical(style.GetWritingMode(), style.Direction());
}

NGLayoutInputNode NGBlockNode::NextSibling() const {
  LayoutObject* next_sibling = GetLayoutObjectForNextSiblingNode(box_);
  if (next_sibling) {
    DCHECK(!next_sibling->IsInline());
    return NGBlockNode(ToLayoutBox(next_sibling));
  }
  return nullptr;
}

NGLayoutInputNode NGBlockNode::FirstChild() const {
  auto* block = ToLayoutBlock(box_);
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (AreNGBlockFlowChildrenInline(block))
    return NGInlineNode(ToLayoutBlockFlow(block));
  return NGBlockNode(ToLayoutBox(child));
}

NGBlockNode NGBlockNode::GetRenderedLegend() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return NGBlockNode(LayoutFieldset::FindInFlowLegend(*ToLayoutBlock(box_)));
}

NGBlockNode NGBlockNode::GetFieldsetContent() const {
  if (!IsFieldsetContainer())
    return nullptr;
  auto* child = GetLayoutObjectForFirstChildNode(ToLayoutBlock(box_));
  if (!child)
    return nullptr;
  return NGBlockNode(ToLayoutBox(child));
}

bool NGBlockNode::CanUseNewLayout(const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
  if (box.StyleRef().ForceLegacyLayout())
    return false;

  // When the style has |ForceLegacyLayout|, it's usually not LayoutNGMixin,
  // but anonymous block can be.
  return box.IsLayoutNGMixin() || box.IsLayoutNGFlexibleBox();
}

bool NGBlockNode::CanUseNewLayout() const {
  return CanUseNewLayout(*box_);
}

String NGBlockNode::ToString() const {
  return String::Format("NGBlockNode: '%s'",
                        GetLayoutBox()->DebugName().Ascii().data());
}

void NGBlockNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGLayoutResult& layout_result) {
  DCHECK(layout_result.PhysicalFragment());
  if (constraint_space.IsIntermediateLayout())
    return;

  const NGPhysicalBoxFragment& physical_fragment =
      ToNGPhysicalBoxFragment(*layout_result.PhysicalFragment());

  NGBoxFragment fragment(constraint_space.GetWritingMode(),
                         constraint_space.Direction(), physical_fragment);
  NGLogicalSize fragment_logical_size = fragment.Size();
  // For each fragment we process, we'll accumulate the logical height and
  // logical intrinsic content box height. We reset it at the first fragment,
  // and accumulate at each method call for fragments belonging to the same
  // layout object. Logical width will only be set at the first fragment and is
  // expected to remain the same throughout all subsequent fragments, since
  // legacy layout doesn't support non-uniform fragmentainer widths.
  LayoutUnit logical_height;
  LayoutUnit intrinsic_content_logical_height;
  if (IsFirstFragment(constraint_space, physical_fragment)) {
    box_->SetLogicalWidth(fragment_logical_size.inline_size);
  } else {
    DCHECK_EQ(box_->LogicalWidth(), fragment_logical_size.inline_size)
        << "Variable fragment inline size not supported";
    logical_height =
        PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    // TODO(layout-ng): We should store this on the break token instead of
    // relying on previously-stored data. Our relayout in NGBlockNode::Layout
    // will otherwise lead to wrong data.
    intrinsic_content_logical_height = box_->IntrinsicContentLogicalHeight();
  }
  logical_height += fragment_logical_size.block_size;
  intrinsic_content_logical_height += layout_result.IntrinsicBlockSize();

  NGBoxStrut borders = fragment.Borders();
  NGBoxStrut scrollbars = GetScrollbarSizes();
  NGBoxStrut padding = fragment.Padding();
  NGBoxStrut border_scrollbar_padding = borders + scrollbars + padding;

  if (IsLastFragment(physical_fragment))
    intrinsic_content_logical_height -= border_scrollbar_padding.BlockSum();
  box_->SetLogicalHeight(logical_height);
  box_->SetIntrinsicContentLogicalHeight(intrinsic_content_logical_height);
  // TODO(mstensho): This should always be done by the parent algorithm, since
  // we may have auto margins, which only the parent is able to resolve. Remove
  // the following line when all layout modes do this properly.
  box_->SetMargin(ComputePhysicalMargins(constraint_space, Style()));

  LayoutMultiColumnFlowThread* flow_thread = GetFlowThread(*box_);
  if (flow_thread) {
    PlaceChildrenInFlowThread(constraint_space, physical_fragment);
  } else {
    NGPhysicalOffset offset_from_start;
    if (constraint_space.HasBlockFragmentation()) {
      // Need to include any block space that this container has used in
      // previous fragmentainers. The offset of children will be relative to
      // the container, in flow thread coordinates, i.e. the model where
      // everything is represented as one single strip, rather than being
      // sliced and translated into columns.

      // TODO(mstensho): writing modes
      offset_from_start.top =
          PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    }
    PlaceChildrenInLayoutBox(constraint_space, physical_fragment,
                             offset_from_start);
  }

  if (box_->IsLayoutBlock() && IsLastFragment(physical_fragment)) {
    LayoutBlock* block = ToLayoutBlock(box_);
    LayoutUnit intrinsic_block_size = layout_result.IntrinsicBlockSize();
    if (constraint_space.HasBlockFragmentation()) {
      intrinsic_block_size +=
          PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    }
    block->LayoutPositionedObjects(/* relayout_children */ false);

    if (flow_thread) {
      UpdateLegacyMultiColumnFlowThread(*this, flow_thread, constraint_space,
                                        physical_fragment);
    }

    // |ComputeOverflow()| below calls |AddVisualOverflowFromChildren()|, which
    // computes visual overflow from |RootInlineBox| if |ChildrenInline()|
    block->ComputeOverflow(intrinsic_block_size - borders.block_end -
                           scrollbars.block_end);
  }

  box_->UpdateAfterLayout();
  box_->ClearNeedsLayout();

  UpdateShapeOutsideInfoIfNeeded(
      constraint_space.PercentageResolutionSize().inline_size);

  if (box_->IsLayoutBlockFlow()) {
    LayoutBlockFlow* block_flow = ToLayoutBlockFlow(box_);
    block_flow->UpdateIsSelfCollapsing();
  }
}

void NGBlockNode::PlaceChildrenInLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& physical_fragment,
    const NGPhysicalOffset& offset_from_start) {
  LayoutBox* rendered_legend = nullptr;
  for (const auto& child_fragment : physical_fragment.Children()) {
    auto* child_object = child_fragment->GetLayoutObject();

    // Skip any line-boxes we have as children, this is handled within
    // NGInlineNode at the moment.
    if (!child_fragment->IsBox() && !child_fragment->IsRenderedLegend())
      continue;

    const auto& box_fragment = *ToNGPhysicalBoxFragment(child_fragment.get());
    if (IsFirstFragment(constraint_space, box_fragment)) {
      if (box_fragment.IsRenderedLegend())
        rendered_legend = ToLayoutBox(box_fragment.GetLayoutObject());
      CopyChildFragmentPosition(box_fragment, child_fragment.Offset(),
                                offset_from_start);
    }
    if (child_object->IsLayoutBlockFlow()) {
      ToLayoutBlockFlow(child_object)->AddVisualOverflowFromFloats();
      ToLayoutBlockFlow(child_object)->AddLayoutOverflowFromFloats();
    }
  }

  if (rendered_legend) {
    // The rendered legend is a child of the the anonymous fieldset content
    // child wrapper object on the legacy side. LayoutNG, on the other hand,
    // generates a fragment for the rendered legend as a direct child of the
    // fieldset container fragment (as a *sibling* preceding the anonymous
    // fieldset content wrapper). Now that we have positioned the anonymous
    // wrapper, we're ready to compensate for this discrepancy. See
    // LayoutNGFieldset for more details.
    LayoutBlock* content_wrapper = rendered_legend->ContainingBlock();
    DCHECK(content_wrapper->IsAnonymous());
    DCHECK(IsHTMLFieldSetElement(content_wrapper->Parent()->GetNode()));
    LayoutPoint location = rendered_legend->Location();
    location -= content_wrapper->Location();
    rendered_legend->SetLocation(location);
  }
}

void NGBlockNode::PlaceChildrenInFlowThread(
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& physical_fragment) {
  LayoutUnit flowthread_offset;
  for (const auto& child : physical_fragment.Children()) {
    // Each anonymous child of a multicol container constitutes one column.
    DCHECK(child->GetLayoutObject() == box_);

    // TODO(mstensho): writing modes
    NGPhysicalOffset offset(LayoutUnit(), flowthread_offset);

    // Position each child node in the first column that they occur, relatively
    // to the block-start of the flow thread.
    const auto* column = ToNGPhysicalBoxFragment(child.get());
    PlaceChildrenInLayoutBox(constraint_space, *column, offset);
    const auto* token = ToNGBlockBreakToken(column->BreakToken());
    flowthread_offset = token->UsedBlockSize();
  }
}

// Copies data back to the legacy layout tree for a given child fragment.
void NGBlockNode::CopyChildFragmentPosition(
    const NGPhysicalFragment& fragment,
    const NGPhysicalOffset fragment_offset,
    const NGPhysicalOffset additional_offset) {
  LayoutBox* layout_box = ToLayoutBox(fragment.GetLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  // The containing block of |layout_box| on the legacy layout side is normally
  // |box_|, but this is not an invariant. Among other things, it does not apply
  // to list item markers and multicol container children. Multicol containiner
  // children typically have their flow thread (not the multicol container
  // itself) as their containing block, and we need to use the right containing
  // block for inserting floats, flipping for writing modes, etc.
  LayoutBlock* containing_block = layout_box->ContainingBlock();

  // LegacyLayout flips vertical-rl horizontal coordinates before paint.
  // NGLayout flips X location for LegacyLayout compatibility. horizontal_offset
  // will be the offset from the left edge of the container to the left edge of
  // the layout object, except when in vertical-rl: Then it will be the offset
  // from the right edge of the container to the right edge of the layout
  // object.
  LayoutUnit horizontal_offset = fragment_offset.left + additional_offset.left;
  bool has_flipped_x_axis =
      containing_block->StyleRef().IsFlippedBlocksWritingMode();
  if (has_flipped_x_axis) {
    horizontal_offset = containing_block->Size().Width() - horizontal_offset -
                        fragment.Size().width;
  }
  layout_box->SetX(horizontal_offset);
  layout_box->SetY(fragment_offset.top + additional_offset.top);

  // Floats need an associated FloatingObject for painting.
  if (IsFloatFragment(fragment) && containing_block->IsLayoutBlockFlow()) {
    FloatingObject* floating_object =
        ToLayoutBlockFlow(containing_block)->InsertFloatingObject(*layout_box);
    floating_object->SetIsInPlacedTree(false);
    floating_object->SetShouldPaint(!layout_box->HasSelfPaintingLayer());
    LayoutUnit horizontal_margin_edge_offset = horizontal_offset;
    if (has_flipped_x_axis)
      horizontal_margin_edge_offset -= layout_box->MarginRight();
    else
      horizontal_margin_edge_offset -= layout_box->MarginLeft();
    floating_object->SetX(horizontal_margin_edge_offset);
    floating_object->SetY(fragment_offset.top + additional_offset.top -
                          layout_box->MarginTop());
    floating_object->SetIsPlaced(true);
    floating_object->SetIsInPlacedTree(true);
  }
}

// For inline children, NG painters handles fragments directly, but there are
// some cases where we need to copy data to the LayoutObject tree. This function
// handles such cases.
void NGBlockNode::CopyFragmentDataToLayoutBoxForInlineChildren(
    const NGPhysicalContainerFragment& container,
    LayoutUnit initial_container_width,
    bool initial_container_is_flipped,
    NGPhysicalOffset offset) {
  for (const auto& child : container.Children()) {
    if (child->IsContainer()) {
      NGPhysicalOffset child_offset = offset + child.Offset();

      // Replaced elements and inline blocks need Location() set relative to
      // their block container.
      LayoutObject* layout_object = child->GetLayoutObject();
      if (layout_object && layout_object->IsBox()) {
        LayoutBox& layout_box = ToLayoutBox(*layout_object);
        NGPhysicalOffset maybe_flipped_offset = child_offset;
        if (initial_container_is_flipped) {
          maybe_flipped_offset.left = initial_container_width -
                                      child->Size().width -
                                      maybe_flipped_offset.left;
        }
        layout_box.SetLocation(maybe_flipped_offset.ToLayoutPoint());
      }

      // The Location() of inline LayoutObject is relative to the
      // LayoutBlockFlow. If |child| establishes a new block formatting context,
      // it also creates another inline formatting context. Do not copy to its
      // descendants in this case.
      if (!child->IsBlockFormattingContextRoot()) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            ToNGPhysicalContainerFragment(*child), initial_container_width,
            initial_container_is_flipped, child_offset);
      }
    }
  }
}

bool NGBlockNode::IsInlineLevel() const {
  return GetLayoutBox()->IsInline();
}

bool NGBlockNode::IsAtomicInlineLevel() const {
  // LayoutObject::IsAtomicInlineLevel() returns true for e.g., <img
  // style="display: block">. Check IsInline() as well.
  return GetLayoutBox()->IsAtomicInlineLevel() && GetLayoutBox()->IsInline();
}

bool NGBlockNode::UseLogicalBottomMarginEdgeForInlineBlockBaseline() const {
  LayoutBox* layout_box = GetLayoutBox();
  return layout_box->IsLayoutBlock() &&
         ToLayoutBlock(layout_box)
             ->UseLogicalBottomMarginEdgeForInlineBlockBaseline();
}

scoped_refptr<NGLayoutResult> NGBlockNode::LayoutAtomicInline(
    const NGConstraintSpace& parent_constraint_space,
    FontBaseline baseline_type,
    bool use_first_line_style) {
  NGConstraintSpaceBuilder space_builder(parent_constraint_space);
  space_builder.SetUseFirstLineStyle(use_first_line_style);

  // Request to compute baseline during the layout, except when we know the box
  // would synthesize box-baseline.
  if (NGBaseline::ShouldPropagateBaselines(GetLayoutBox())) {
    space_builder.AddBaselineRequest(
        {NGBaselineAlgorithmType::kAtomicInline, baseline_type});
  }

  const ComputedStyle& style = Style();
  NGConstraintSpace constraint_space =
      space_builder.SetIsNewFormattingContext(true)
          .SetIsShrinkToFit(true)
          .SetAvailableSize(parent_constraint_space.AvailableSize())
          .SetPercentageResolutionSize(
              parent_constraint_space.PercentageResolutionSize())
          .SetReplacedPercentageResolutionSize(
              parent_constraint_space.ReplacedPercentageResolutionSize())
          .SetTextDirection(style.Direction())
          .ToConstraintSpace(style.GetWritingMode());
  return Layout(constraint_space);
}

scoped_refptr<NGLayoutResult> NGBlockNode::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  // This is an exit-point from LayoutNG to the legacy engine. This means that
  // we need to be at a formatting context boundary, since NG and legacy don't
  // cooperate on e.g. margin collapsing.
  DCHECK(!box_->IsLayoutBlock() ||
         ToLayoutBlock(box_)->CreatesNewFormattingContext());

  WritingMode writing_mode = Style().GetWritingMode();
  const NGConstraintSpace* old_space =
      box_->IsLayoutBlock() ? ToLayoutBlock(box_)->CachedConstraintSpace()
                            : nullptr;
  if (!old_space || box_->NeedsLayout() || *old_space != constraint_space) {
    LayoutUnit inline_size =
        CalculateAvailableInlineSizeForLegacy(*box_, constraint_space);
    LayoutUnit block_size =
        CalculateAvailableBlockSizeForLegacy(*box_, constraint_space);

    LayoutObject* containing_block = box_->ContainingBlock();
    bool parallel_writing_mode;
    if (!containing_block) {
      parallel_writing_mode = true;
    } else {
      parallel_writing_mode = IsParallelWritingMode(
          containing_block->StyleRef().GetWritingMode(), writing_mode);
    }
    if (parallel_writing_mode) {
      box_->SetOverrideContainingBlockContentLogicalWidth(inline_size);
      box_->SetOverrideContainingBlockContentLogicalHeight(block_size);
    } else {
      // OverrideContainingBlock should be in containing block writing mode.
      box_->SetOverrideContainingBlockContentLogicalWidth(block_size);
      box_->SetOverrideContainingBlockContentLogicalHeight(inline_size);
    }

    if (constraint_space.IsFixedSizeInline()) {
      box_->SetOverrideLogicalWidth(
          constraint_space.AvailableSize().inline_size);
    }
    if (constraint_space.IsFixedSizeBlock()) {
      box_->SetOverrideLogicalHeight(
          constraint_space.AvailableSize().block_size);
    }
    box_->ComputeAndSetBlockDirectionMargins(box_->ContainingBlock());

    if (box_->IsLayoutNGMixin() && box_->NeedsLayout()) {
      ToLayoutBlockFlow(box_)->LayoutBlockFlow::UpdateBlockLayout(true);
    } else {
      box_->ForceLayout();
    }

    // Reset the containing block size override size, now that we're done with
    // subtree layout. Min/max calculation that depends on the block size of the
    // container (e.g. objects with intrinsic ratio and percentage block size)
    // in a subsequent layout pass might otherwise become wrong.
    box_->ClearOverrideContainingBlockContentSize();
    if (box_->IsLayoutBlock())
      ToLayoutBlock(box_)->SetCachedConstraintSpace(constraint_space);
  }
  NGLogicalSize box_size(box_->LogicalWidth(), box_->LogicalHeight());
  // TODO(kojii): Implement use_first_line_style.
  NGBoxFragmentBuilder builder(*this, box_->Style(), writing_mode,
                               box_->StyleRef().Direction());
  builder.SetIsOldLayoutRoot();
  builder.SetInlineSize(box_size.inline_size);
  builder.SetBlockSize(box_size.block_size);
  NGBoxStrut borders(box_->BorderStart(), box_->BorderEnd(),
                     box_->BorderBefore(), box_->BorderAfter());
  builder.SetBorders(borders);
  NGBoxStrut padding(box_->PaddingStart(), box_->PaddingEnd(),
                     box_->PaddingBefore(), box_->PaddingAfter());
  builder.SetPadding(padding);

  CopyBaselinesFromOldLayout(constraint_space, &builder);
  UpdateShapeOutsideInfoIfNeeded(
      constraint_space.PercentageResolutionSize().inline_size);
  return builder.ToBoxFragment();
}

void NGBlockNode::CopyBaselinesFromOldLayout(
    const NGConstraintSpace& constraint_space,
    NGBoxFragmentBuilder* builder) {
  const NGConstraintSpace::NGBaselineRequestVector& requests =
      constraint_space.BaselineRequests();
  if (requests.IsEmpty())
    return;

  if (UNLIKELY(constraint_space.GetWritingMode() != Style().GetWritingMode()))
    return;

  for (const auto& request : requests) {
    switch (request.algorithm_type) {
      case NGBaselineAlgorithmType::kAtomicInline: {
        LayoutUnit position =
            AtomicInlineBaselineFromOldLayout(request, constraint_space);
        if (position != -1)
          builder->AddBaseline(request, position);
        break;
      }
      case NGBaselineAlgorithmType::kFirstLine: {
        LayoutUnit position = box_->FirstLineBoxBaseline();
        if (position != -1)
          builder->AddBaseline(request, position);
        break;
      }
    }
  }
}

LayoutUnit NGBlockNode::AtomicInlineBaselineFromOldLayout(
    const NGBaselineRequest& request,
    const NGConstraintSpace& constraint_space) {
  LineDirectionMode line_direction = box_->IsHorizontalWritingMode()
                                         ? LineDirectionMode::kHorizontalLine
                                         : LineDirectionMode::kVerticalLine;

  // If this is an inline box, use |BaselinePosition()|. Some LayoutObject
  // classes override it assuming inline layout calls |BaselinePosition()|.
  if (box_->IsInline()) {
    LayoutUnit position = LayoutUnit(box_->BaselinePosition(
        request.baseline_type, constraint_space.UseFirstLineStyle(),
        line_direction, kPositionOnContainingLine));

    // BaselinePosition() uses margin edge for atomic inlines. Subtract
    // margin-over so that the position is relative to the border box.
    if (box_->IsAtomicInlineLevel())
      position -= box_->MarginOver();

    return position;
  }

  // If this is a block box, use |InlineBlockBaseline()|. When an inline block
  // has block children, their inline block baselines need to be propagated.
  return box_->InlineBlockBaseline(line_direction);
}

// Floats can optionally have a shape area, specifed by "shape-outside". The
// current shape machinery requires setting the size of the float after layout
// in the parents writing mode.
void NGBlockNode::UpdateShapeOutsideInfoIfNeeded(
    LayoutUnit percentage_resolution_inline_size) {
  if (!box_->IsFloating() || !box_->GetShapeOutsideInfo())
    return;

  // TODO(ikilpatrick): Ideally this should be moved to a NGLayoutResult
  // computing the shape area. There may be an issue with the new fragmentation
  // model and computing the correct sizes of shapes.
  ShapeOutsideInfo* shape_outside = box_->GetShapeOutsideInfo();
  LayoutBlock* containing_block = box_->ContainingBlock();
  shape_outside->SetReferenceBoxLogicalSize(
      containing_block->IsHorizontalWritingMode()
          ? box_->Size()
          : box_->Size().TransposedSize());
  shape_outside->SetPercentageResolutionInlineSize(
      percentage_resolution_inline_size);
}

void NGBlockNode::UseOldOutOfFlowPositioning() {
  DCHECK(box_->IsOutOfFlowPositioned());
  box_->ContainingBlock()->InsertPositionedObject(box_);
}

// Save static position for legacy AbsPos layout.
void NGBlockNode::SaveStaticOffsetForLegacy(
    const NGLogicalOffset& offset,
    const LayoutObject* offset_container) {
  DCHECK(box_->IsOutOfFlowPositioned());
  // Only set static position if the current offset container
  // is one that Legacy layout expects static offset from.
  const LayoutObject* parent = box_->Parent();
  if (parent == offset_container ||
      (parent && parent->IsLayoutInline() &&
       parent->ContainingBlock() == offset_container)) {
    DCHECK(box_->Layer());
    box_->Layer()->SetStaticBlockPosition(offset.block_offset);
    box_->Layer()->SetStaticInlinePosition(offset.inline_offset);
  }
}

void NGBlockNode::StoreMargins(const NGConstraintSpace& constraint_space,
                               const NGBoxStrut& margins) {
  if (constraint_space.IsIntermediateLayout())
    return;
  NGPhysicalBoxStrut physical_margins = margins.ConvertToPhysical(
      constraint_space.GetWritingMode(), constraint_space.Direction());
  box_->SetMargin(physical_margins);
}

}  // namespace blink
