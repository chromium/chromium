// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

#include <math.h>

#include "third_party/blink/renderer/core/layout/anchor_scroll_data.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_simplified_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_simplified_oof_layout_algorithm.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {

namespace {

bool IsInPreOrder(const HeapVector<NGLogicalOOFNodeForFragmentation>& nodes) {
  return std::is_sorted(nodes.begin(), nodes.end(),
                        [](const NGLogicalOOFNodeForFragmentation& a,
                           const NGLogicalOOFNodeForFragmentation& b) {
                          return a.box->IsBeforeInPreOrder(*b.box);
                        });
}

void SortInPreOrder(HeapVector<NGLogicalOOFNodeForFragmentation>* nodes) {
  std::sort(nodes->begin(), nodes->end(),
            [](const NGLogicalOOFNodeForFragmentation& a,
               const NGLogicalOOFNodeForFragmentation& b) {
              return a.box->IsBeforeInPreOrder(*b.box);
            });
}

bool MayHaveAnchorQuery(
    const HeapVector<NGLogicalOOFNodeForFragmentation>& nodes) {
  for (const NGLogicalOOFNodeForFragmentation& node : nodes) {
    if (node.box->MayHaveAnchorQuery())
      return true;
  }
  return false;
}

bool CalculateNonOverflowingRangeInOneAxis(
    const absl::optional<LayoutUnit>& inset_start,
    const absl::optional<LayoutUnit>& inset_end,
    const LayoutUnit& container_start,
    const LayoutUnit& container_end,
    const LayoutUnit& margin_box_start,
    const LayoutUnit& margin_box_end,
    absl::optional<LayoutUnit>* out_scroll_min,
    absl::optional<LayoutUnit>* out_scroll_max) {
  LayoutUnit start_available_space = margin_box_start - container_start;
  if (inset_start) {
    // If the start inset is non-auto, then the start edges of both the
    // scroll-adjusted inset-modified containing block and the scroll-shifted
    // margin box always move by the same amount on scrolling. Then it overflows
    // if and only if it overflows at the initial scroll location.
    if (start_available_space < 0) {
      return false;
    }
  } else {
    // Otherwise, the start edge of the SAIMCB is always at the same location,
    // while that of the scroll-shifted margin box can move by at most
    // |start_available_space| before overflowing.
    *out_scroll_max = start_available_space;
  }
  // Calculation for the end edge is symmetric.
  LayoutUnit end_available_space = container_end - margin_box_end;
  if (inset_end) {
    if (end_available_space < 0) {
      return false;
    }
  } else {
    *out_scroll_min = -end_available_space;
  }
  if (*out_scroll_min && *out_scroll_max &&
      out_scroll_min->value() > out_scroll_max->value()) {
    return false;
  }
  return true;
}

}  // namespace

// static
absl::optional<LogicalSize>
NGOutOfFlowLayoutPart::InitialContainingBlockFixedSize(NGBlockNode container) {
  if (!container.GetLayoutBox()->IsLayoutView() ||
      container.GetDocument().Printing())
    return absl::nullopt;
  const auto* frame_view = container.GetDocument().View();
  DCHECK(frame_view);
  PhysicalSize size(
      frame_view->LayoutViewport()->ExcludeScrollbars(frame_view->Size()));
  return size.ConvertToLogical(container.Style().GetWritingMode());
}

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    const NGBlockNode& container_node,
    const NGConstraintSpace& container_space,
    NGBoxFragmentBuilder* container_builder)
    : NGOutOfFlowLayoutPart(container_node.IsAbsoluteContainer(),
                            container_node.IsFixedContainer(),
                            container_node.IsGrid(),
                            container_space,
                            container_builder,
                            InitialContainingBlockFixedSize(container_node)) {}

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    bool is_absolute_container,
    bool is_fixed_container,
    bool is_grid_container,
    const NGConstraintSpace& container_space,
    NGBoxFragmentBuilder* container_builder,
    absl::optional<LogicalSize> initial_containing_block_fixed_size)
    : container_builder_(container_builder),
      is_absolute_container_(is_absolute_container),
      is_fixed_container_(is_fixed_container),
      has_block_fragmentation_(
          InvolvedInBlockFragmentation(*container_builder)) {
  // TODO(almaher): Should we early return here in the case of block
  // fragmentation? If not, what should |allow_first_tier_oof_cache_| be set to
  // in this case?
  if (!container_builder->HasOutOfFlowPositionedCandidates() &&
      !container_builder->HasOutOfFlowFragmentainerDescendants() &&
      !container_builder->HasMulticolsWithPendingOOFs() &&
      !To<LayoutBlock>(container_builder_->GetLayoutObject())
           ->HasPositionedObjects())
    return;

  // Disable first tier cache for grid layouts, as grid allows for out-of-flow
  // items to be placed in grid areas, which is complex to maintain a cache for.
  const NGBoxStrut border_scrollbar =
      container_builder->Borders() + container_builder->Scrollbar();
  allow_first_tier_oof_cache_ = border_scrollbar.IsEmpty() &&
                                !is_grid_container && !has_block_fragmentation_;
  default_containing_block_info_for_absolute_.writing_direction =
      ConstraintSpace().GetWritingDirection();
  default_containing_block_info_for_fixed_.writing_direction =
      ConstraintSpace().GetWritingDirection();
  if (container_builder_->HasBlockSize()) {
    default_containing_block_info_for_absolute_.rect.size =
        ShrinkLogicalSize(container_builder_->Size(), border_scrollbar);
    default_containing_block_info_for_fixed_.rect.size =
        initial_containing_block_fixed_size
            ? *initial_containing_block_fixed_size
            : default_containing_block_info_for_absolute_.rect.size;
  }
  LogicalOffset container_offset = {border_scrollbar.inline_start,
                                    border_scrollbar.block_start};
  default_containing_block_info_for_absolute_.rect.offset = container_offset;
  default_containing_block_info_for_fixed_.rect.offset = container_offset;
}

void NGOutOfFlowLayoutPart::Run(const LayoutBox* only_layout) {
  HandleFragmentation();
  const LayoutObject* current_container = container_builder_->GetLayoutObject();
  if (!container_builder_->HasOutOfFlowPositionedCandidates() &&
      !To<LayoutBlock>(current_container)->HasPositionedObjects()) {
    container_builder_
        ->AdjustFixedposContainingBlockForFragmentainerDescendants();
    container_builder_->AdjustFixedposContainingBlockForInnerMulticols();
    return;
  }

  // If the container is display-locked, then we skip the layout of descendants,
  // so we can early out immediately.
  if (current_container->ChildLayoutBlockedByDisplayLock())
    return;

  HeapVector<NGLogicalOutOfFlowPositionedNode> candidates;
  ClearCollectionScope<HeapVector<NGLogicalOutOfFlowPositionedNode>>
      clear_scope(&candidates);
  container_builder_->SwapOutOfFlowPositionedCandidates(&candidates);

  LayoutCandidates(&candidates, only_layout);
}

void NGOutOfFlowLayoutPart::HandleFragmentation(
    ColumnBalancingInfo* column_balancing_info) {
  // OOF fragmentation depends on LayoutBox data being up-to-date, which isn't
  // the case if side-effects are disabled. So we cannot safely do anything
  // here.
  if (NGDisableSideEffectsScope::IsDisabled())
    return;

  if (!column_balancing_info &&
      (!container_builder_->IsBlockFragmentationContextRoot() ||
       has_block_fragmentation_))
    return;

  // Don't use the cache if we are handling fragmentation.
  allow_first_tier_oof_cache_ = false;

  if (container_builder_->Node().IsPaginatedRoot()) {
    // Column balancing only affects multicols.
    DCHECK(!column_balancing_info);
    HeapVector<NGLogicalOutOfFlowPositionedNode> candidates;
    ClearCollectionScope<HeapVector<NGLogicalOutOfFlowPositionedNode>> scope(
        &candidates);
    container_builder_->SwapOutOfFlowPositionedCandidates(&candidates);
    // Catch everything for paged layout. We want to fragment everything. If the
    // containing block is the initial containing block, it should be fragmented
    // now, and not bubble further to the viewport (where we'd end up with
    // non-fragmented layout). Note that we're not setting a containing block
    // fragment for the candidates, as that would confuse
    // GetContainingBlockInfo(), which expects a containing block fragment to
    // also have a LayoutObject, which fragmentainers don't. Fixing that is
    // possible, but requires special-code there. This approach seems easier.
    for (NGLogicalOutOfFlowPositionedNode candidate : candidates)
      container_builder_->AddOutOfFlowFragmentainerDescendant(candidate);
  }

#if DCHECK_IS_ON()
  if (column_balancing_info) {
    DCHECK(!column_balancing_info->columns.empty());
    DCHECK(
        !column_balancing_info->out_of_flow_fragmentainer_descendants.empty());
  }
#endif
  base::AutoReset<ColumnBalancingInfo*> balancing_scope(&column_balancing_info_,
                                                        column_balancing_info);

  auto ShouldContinue = [&]() -> bool {
    if (column_balancing_info_)
      return column_balancing_info_->HasOutOfFlowFragmentainerDescendants();
    return container_builder_->HasOutOfFlowFragmentainerDescendants() ||
           container_builder_->HasMulticolsWithPendingOOFs();
  };

  while (ShouldContinue()) {
    HeapVector<NGLogicalOOFNodeForFragmentation> fragmentainer_descendants;
    ClearCollectionScope<HeapVector<NGLogicalOOFNodeForFragmentation>> scope(
        &fragmentainer_descendants);
    if (column_balancing_info_) {
      column_balancing_info_->SwapOutOfFlowFragmentainerDescendants(
          &fragmentainer_descendants);
      DCHECK(!fragmentainer_descendants.empty());
    } else {
      HandleMulticolsWithPendingOOFs(container_builder_);
      if (container_builder_->HasOutOfFlowFragmentainerDescendants()) {
        container_builder_->SwapOutOfFlowFragmentainerDescendants(
            &fragmentainer_descendants);
        DCHECK(!fragmentainer_descendants.empty());
      }
    }
    if (!fragmentainer_descendants.empty()) {
      LogicalOffset fragmentainer_progression = GetFragmentainerProgression(
          *container_builder_, GetFragmentainerType());
      LayoutFragmentainerDescendants(&fragmentainer_descendants,
                                     fragmentainer_progression);
    }
  }
  if (!column_balancing_info_) {
    for (auto& descendant : delayed_descendants_)
      container_builder_->AddOutOfFlowFragmentainerDescendant(descendant);
  }
}

// Retrieve the stored ContainingBlockInfo needed for placing positioned nodes.
// When fragmenting, the ContainingBlockInfo is not stored ahead of time and
// must be generated on demand. The reason being that during fragmentation, we
// wait to place positioned nodes until they've reached the fragmentation
// context root. In such cases, we cannot use default |ContainingBlockInfo|
// since the fragmentation root is not the containing block of the positioned
// nodes. Rather, we must generate their ContainingBlockInfo based on the
// |candidate.containing_block.fragment|.
const NGOutOfFlowLayoutPart::ContainingBlockInfo
NGOutOfFlowLayoutPart::GetContainingBlockInfo(
    const NGLogicalOutOfFlowPositionedNode& candidate) {
  const auto* container_object = container_builder_->GetLayoutObject();
  const auto& node_style = candidate.Node().Style();

  auto IsPlacedWithinGridArea = [&](const auto* containing_block) {
    if (!containing_block->IsLayoutNGGrid())
      return false;

    return !node_style.GridColumnStart().IsAuto() ||
           !node_style.GridColumnEnd().IsAuto() ||
           !node_style.GridRowStart().IsAuto() ||
           !node_style.GridRowEnd().IsAuto();
  };

  auto GridAreaContainingBlockInfo = [&](const LayoutNGGrid& containing_grid,
                                         const NGGridLayoutData& layout_data,
                                         const NGBoxStrut& borders,
                                         const LogicalSize& size)
      -> NGOutOfFlowLayoutPart::ContainingBlockInfo {
    const auto& grid_style = containing_grid.StyleRef();
    GridItemData grid_item(candidate.Node(), grid_style);

    return {grid_style.GetWritingDirection(),
            NGGridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
                containing_grid.CachedPlacementData(), layout_data, grid_style,
                borders, size, &grid_item)};
  };

  if (candidate.inline_container.container) {
    const auto it =
        containing_blocks_map_.find(candidate.inline_container.container);
    DCHECK(it != containing_blocks_map_.end());
    return it->value;
  }

  if (candidate.is_for_fragmentation) {
    NGLogicalOOFNodeForFragmentation fragmentainer_descendant =
        To<NGLogicalOOFNodeForFragmentation>(candidate);
    if (fragmentainer_descendant.containing_block.Fragment()) {
      DCHECK(container_builder_->IsBlockFragmentationContextRoot());

      const NGPhysicalFragment* containing_block_fragment =
          fragmentainer_descendant.containing_block.Fragment();
      const LayoutObject* containing_block =
          containing_block_fragment->GetLayoutObject();
      DCHECK(containing_block);

      bool is_placed_within_grid_area =
          IsPlacedWithinGridArea(containing_block);
      auto it = containing_blocks_map_.find(containing_block);
      if (it != containing_blocks_map_.end() && !is_placed_within_grid_area)
        return it->value;

      const auto writing_direction =
          containing_block->StyleRef().GetWritingDirection();
      LogicalSize size = containing_block_fragment->Size().ConvertToLogical(
          writing_direction.GetWritingMode());
      size.block_size =
          LayoutBoxUtils::TotalBlockSize(*To<LayoutBox>(containing_block));

      // TODO(1079031): This should eventually include scrollbar and border.
      NGBoxStrut border = To<NGPhysicalBoxFragment>(containing_block_fragment)
                              ->Borders()
                              .ConvertToLogical(writing_direction);

      if (is_placed_within_grid_area) {
        return GridAreaContainingBlockInfo(
            *To<LayoutNGGrid>(containing_block),
            *To<LayoutNGGrid>(containing_block)->GridLayoutData(), border,
            size);
      }

      LogicalSize content_size = ShrinkLogicalSize(size, border);
      LogicalOffset container_offset =
          LogicalOffset(border.inline_start, border.block_start);
      container_offset += fragmentainer_descendant.containing_block.Offset();

      ContainingBlockInfo containing_block_info{
          writing_direction, LogicalRect(container_offset, content_size),
          fragmentainer_descendant.containing_block.RelativeOffset(),
          fragmentainer_descendant.containing_block.Offset()};

      return containing_blocks_map_
          .insert(containing_block, containing_block_info)
          .stored_value->value;
    }
  }

  if (IsPlacedWithinGridArea(container_object)) {
    return GridAreaContainingBlockInfo(
        *To<LayoutNGGrid>(container_object),
        container_builder_->GridLayoutData(), container_builder_->Borders(),
        {container_builder_->InlineSize(),
         container_builder_->FragmentBlockSize()});
  }

  // The ::view-transition element is special in that its containing block is
  // the "snapshot root" rect, rather than a viewport or parent box:
  // https://drafts.csswg.org/css-view-transitions-1/#selectordef-view-transition.
  DCHECK(candidate.box);
  if (ViewTransitionUtils::IsViewTransitionRoot(*candidate.box)) {
    DCHECK(container_object->IsLayoutView());
    const ViewTransition* transition =
        ViewTransitionUtils::GetActiveTransition(candidate.box->GetDocument());
    DCHECK(transition);

    PhysicalRect physical_snapshot_root_in_frame(
        PhysicalOffset(transition->GetFrameToSnapshotRootOffset()),
        PhysicalSize(transition->GetSnapshotRootSize()));

    WritingDirectionMode writing_direction =
        ConstraintSpace().GetWritingDirection();
    LogicalSize outer_size = container_builder_->Size();
    WritingModeConverter converter(writing_direction, outer_size);

    NGOutOfFlowLayoutPart::ContainingBlockInfo containing_block_for_snapshot;
    containing_block_for_snapshot.rect =
        converter.ToLogical(physical_snapshot_root_in_frame);

    containing_block_for_snapshot.writing_direction = writing_direction;

    return containing_block_for_snapshot;
  }

  return node_style.GetPosition() == EPosition::kAbsolute
             ? default_containing_block_info_for_absolute_
             : default_containing_block_info_for_fixed_;
}

void NGOutOfFlowLayoutPart::ComputeInlineContainingBlocks(
    const HeapVector<NGLogicalOutOfFlowPositionedNode>& candidates) {
  InlineContainingBlockUtils::InlineContainingBlockMap
      inline_container_fragments;

  for (auto& candidate : candidates) {
    if (candidate.inline_container.container &&
        !inline_container_fragments.Contains(
            candidate.inline_container.container)) {
      InlineContainingBlockUtils::InlineContainingBlockGeometry
          inline_geometry = {};
      inline_container_fragments.insert(
          candidate.inline_container.container.Get(), inline_geometry);
    }
  }

  // Fetch the inline start/end fragment geometry.
  InlineContainingBlockUtils::ComputeInlineContainerGeometry(
      &inline_container_fragments, container_builder_);

  LogicalSize container_builder_size = container_builder_->Size();
  PhysicalSize container_builder_physical_size = ToPhysicalSize(
      container_builder_size, ConstraintSpace().GetWritingMode());
  AddInlineContainingBlockInfo(
      inline_container_fragments,
      default_containing_block_info_for_absolute_.writing_direction,
      container_builder_physical_size);
}

void NGOutOfFlowLayoutPart::ComputeInlineContainingBlocksForFragmentainer(
    const HeapVector<NGLogicalOOFNodeForFragmentation>& descendants) {
  struct InlineContainingBlockInfo {
    InlineContainingBlockUtils::InlineContainingBlockMap map;
    // The relative offset of the inline's containing block to the
    // fragmentation context root.
    LogicalOffset relative_offset;
    // The offset of the containing block relative to the fragmentation context
    // root (not including any relative offset).
    LogicalOffset offset_to_fragmentation_context;
  };

  HeapHashMap<Member<const LayoutBox>, InlineContainingBlockInfo>
      inline_containg_blocks;

  // Collect the inline containers by shared containing block.
  for (auto& descendant : descendants) {
    if (descendant.inline_container.container) {
      DCHECK(descendant.containing_block.Fragment());
      const LayoutBox* containing_block = To<LayoutBox>(
          descendant.containing_block.Fragment()->GetLayoutObject());

      InlineContainingBlockUtils::InlineContainingBlockGeometry
          inline_geometry = {};
      inline_geometry.relative_offset =
          descendant.inline_container.relative_offset;
      auto it = inline_containg_blocks.find(containing_block);
      if (it != inline_containg_blocks.end()) {
        if (!it->value.map.Contains(descendant.inline_container.container)) {
          it->value.map.insert(descendant.inline_container.container.Get(),
                               inline_geometry);
        }
        continue;
      }
      InlineContainingBlockUtils::InlineContainingBlockMap inline_containers;
      inline_containers.insert(descendant.inline_container.container.Get(),
                               inline_geometry);
      InlineContainingBlockInfo inline_info{
          inline_containers, descendant.containing_block.RelativeOffset(),
          descendant.containing_block.Offset()};
      inline_containg_blocks.insert(containing_block, inline_info);
    }
  }

  for (auto& inline_containg_block : inline_containg_blocks) {
    const LayoutBox* containing_block = inline_containg_block.key;
    InlineContainingBlockInfo& inline_info = inline_containg_block.value;

    LogicalSize size(LayoutBoxUtils::InlineSize(*containing_block),
                     LayoutBoxUtils::TotalBlockSize(*containing_block));
    PhysicalSize container_builder_physical_size =
        ToPhysicalSize(size, containing_block->StyleRef().GetWritingMode());

    // Fetch the inline start/end fragment geometry.
    InlineContainingBlockUtils::ComputeInlineContainerGeometryForFragmentainer(
        containing_block, container_builder_physical_size, &inline_info.map);

    AddInlineContainingBlockInfo(
        inline_info.map, containing_block->StyleRef().GetWritingDirection(),
        container_builder_physical_size, inline_info.relative_offset,
        inline_info.offset_to_fragmentation_context,
        /* adjust_for_fragmentation */ true);
  }
}

void NGOutOfFlowLayoutPart::AddInlineContainingBlockInfo(
    const InlineContainingBlockUtils::InlineContainingBlockMap&
        inline_container_fragments,
    const WritingDirectionMode container_writing_direction,
    PhysicalSize container_builder_size,
    LogicalOffset containing_block_relative_offset,
    LogicalOffset containing_block_offset,
    bool adjust_for_fragmentation) {
  // Transform the start/end fragments into a ContainingBlockInfo.
  for (const auto& block_info : inline_container_fragments) {
    DCHECK(block_info.value.has_value());

    // The calculation below determines the size of the inline containing block
    // rect.
    //
    // To perform this calculation we:
    // 1. Determine the start_offset "^", this is at the logical-start (wrt.
    //    default containing block), of the start fragment rect.
    // 2. Determine the end_offset "$", this is at the logical-end (wrt.
    //    default containing block), of the end  fragment rect.
    // 3. Determine the logical rectangle defined by these two offsets.
    //
    // Case 1a: Same direction, overlapping fragments.
    //      +---------------
    // ---> |^*****-------->
    //      +*----*---------
    //       *    *
    // ------*----*+
    // ----> *****$| --->
    // ------------+
    //
    // Case 1b: Different direction, overlapping fragments.
    //      +---------------
    // ---> ^******* <-----|
    //      *------*--------
    //      *      *
    // -----*------*
    // |<-- *******$ --->
    // ------------+
    //
    // Case 2a: Same direction, non-overlapping fragments.
    //             +--------
    // --------->  |^ ----->
    //             +*-------
    //              *
    // --------+    *
    // ------->|    $ --->
    // --------+
    //
    // Case 2b: Same direction, non-overlapping fragments.
    //             +--------
    // --------->  ^ <-----|
    //             *--------
    //             *
    // --------+   *
    // | <------   $  --->
    // --------+
    //
    // Note in cases [1a, 2a] we need to account for the inline borders of the
    // rectangles, where-as in [1b, 2b] we do not. This is handled by the
    // is_same_direction check(s).
    //
    // Note in cases [2a, 2b] we don't allow a "negative" containing block size,
    // we clamp negative sizes to zero.
    const ComputedStyle* inline_cb_style = block_info.key->Style();
    DCHECK(inline_cb_style);

    const auto inline_writing_direction =
        inline_cb_style->GetWritingDirection();
    NGBoxStrut inline_cb_borders = ComputeBordersForInline(*inline_cb_style);
    DCHECK_EQ(container_writing_direction.GetWritingMode(),
              inline_writing_direction.GetWritingMode());

    bool is_same_direction =
        container_writing_direction == inline_writing_direction;

    // Step 1 - determine the start_offset.
    const PhysicalRect& start_rect =
        block_info.value->start_fragment_union_rect;
    LogicalOffset start_offset = start_rect.offset.ConvertToLogical(
        container_writing_direction, container_builder_size, start_rect.size);

    // Make sure we add the inline borders, we don't need to do this in the
    // inline direction if the blocks are in opposite directions.
    start_offset.block_offset += inline_cb_borders.block_start;
    if (is_same_direction)
      start_offset.inline_offset += inline_cb_borders.inline_start;

    // Step 2 - determine the end_offset.
    const PhysicalRect& end_rect = block_info.value->end_fragment_union_rect;
    LogicalOffset end_offset = end_rect.offset.ConvertToLogical(
        container_writing_direction, container_builder_size, end_rect.size);

    // Add in the size of the fragment to get the logical end of the fragment.
    end_offset += end_rect.size.ConvertToLogical(
        container_writing_direction.GetWritingMode());

    // Make sure we subtract the inline borders, we don't need to do this in the
    // inline direction if the blocks are in opposite directions.
    end_offset.block_offset -= inline_cb_borders.block_end;
    if (is_same_direction)
      end_offset.inline_offset -= inline_cb_borders.inline_end;

    // Make sure we don't end up with a rectangle with "negative" size.
    end_offset.inline_offset =
        std::max(end_offset.inline_offset, start_offset.inline_offset);
    end_offset.block_offset =
        std::max(end_offset.block_offset, start_offset.block_offset);

    // Step 3 - determine the logical rectangle.

    // Determine the logical size of the containing block.
    LogicalSize inline_cb_size = {
        end_offset.inline_offset - start_offset.inline_offset,
        end_offset.block_offset - start_offset.block_offset};
    DCHECK_GE(inline_cb_size.inline_size, LayoutUnit());
    DCHECK_GE(inline_cb_size.block_size, LayoutUnit());

    if (adjust_for_fragmentation) {
      // When fragmenting, the containing block will not be associated with the
      // current builder. Thus, we need to adjust the start offset to take the
      // writing mode of the builder into account.
      PhysicalSize physical_size =
          ToPhysicalSize(inline_cb_size, ConstraintSpace().GetWritingMode());
      start_offset =
          start_offset
              .ConvertToPhysical(container_writing_direction,
                                 container_builder_size, physical_size)
              .ConvertToLogical(ConstraintSpace().GetWritingDirection(),
                                container_builder_size, physical_size);
    }

    // Subtract out the inline relative offset, if set, so that it can be
    // applied after fragmentation is performed on the fragmentainer
    // descendants.
    DCHECK((block_info.value->relative_offset == LogicalOffset() &&
            containing_block_relative_offset == LogicalOffset() &&
            containing_block_offset == LogicalOffset()) ||
           container_builder_->IsBlockFragmentationContextRoot());
    LogicalOffset container_offset =
        start_offset - block_info.value->relative_offset;
    LogicalOffset total_relative_offset =
        containing_block_relative_offset + block_info.value->relative_offset;

    // The offset of the container is currently relative to the containing
    // block. Add the offset of the containng block to the fragmentation context
    // root so that it is relative to the fragmentation context root, instead.
    container_offset += containing_block_offset;

    // If an OOF has an inline containing block, the OOF offset that is written
    // back to legacy is relative to the containing block of the inline rather
    // than the inline itself. |containing_block_offset| will be used when
    // calculating this OOF offset. However, there may be some relative offset
    // between the containing block and the inline container that should be
    // included in the final OOF offset that is written back to legacy. Adjust
    // for that relative offset here.
    containing_blocks_map_.insert(
        block_info.key.Get(),
        ContainingBlockInfo{
            inline_writing_direction,
            LogicalRect(container_offset, inline_cb_size),
            total_relative_offset,
            containing_block_offset - block_info.value->relative_offset});
  }
}

void NGOutOfFlowLayoutPart::LayoutCandidates(
    HeapVector<NGLogicalOutOfFlowPositionedNode>* candidates,
    const LayoutBox* only_layout) {
  const WritingModeConverter conainer_converter(
      container_builder_->GetWritingDirection(), container_builder_->Size());
  const NGFragmentItemsBuilder::ItemWithOffsetList* items = nullptr;
  absl::optional<NGLogicalAnchorQueryMap> anchor_queries;
  while (candidates->size() > 0) {
    if (!has_block_fragmentation_ ||
        container_builder_->IsInitialColumnBalancingPass())
      ComputeInlineContainingBlocks(*candidates);
    for (auto& candidate : *candidates) {
      LayoutBox* layout_box = candidate.box;
      if (!container_builder_->IsBlockFragmentationContextRoot())
        SaveStaticPositionOnPaintLayer(layout_box, candidate.static_position);
      if (IsContainingBlockForCandidate(candidate) &&
          (!only_layout || layout_box == only_layout)) {
        if (has_block_fragmentation_) {
          container_builder_->SetHasOutOfFlowInFragmentainerSubtree(true);
          if (!container_builder_->IsInitialColumnBalancingPass()) {
            // As an optimization, only populate legacy positioned objects lists
            // when inside a fragmentation context root, since otherwise we can
            // just look at the children in the fragment tree.
            if (layout_box != only_layout) {
              container_builder_->InsertLegacyPositionedObject(
                  candidate.Node());
            }
            NGLogicalOOFNodeForFragmentation fragmentainer_descendant(
                candidate);
            container_builder_->AdjustFragmentainerDescendant(
                fragmentainer_descendant);
            container_builder_
                ->AdjustFixedposContainingBlockForInnerMulticols();
            container_builder_->AddOutOfFlowFragmentainerDescendant(
                fragmentainer_descendant);
            continue;
          }
        }

        // If the containing block is inline, it may have a different anchor
        // query than |container_builder_|. Compute the anchor query for it.
        const bool needs_anchor_queries =
            candidate.inline_container.container &&
            container_builder_->AnchorQuery();
        if (needs_anchor_queries && !anchor_queries) {
          if (NGFragmentItemsBuilder* items_builder =
                  container_builder_->ItemsBuilder()) {
            items = &items_builder->Items(conainer_converter.OuterSize());
          }
          anchor_queries.emplace(*container_builder_->Node().GetLayoutBox(),
                                 container_builder_->Children(), items,
                                 conainer_converter);
        }

        NodeInfo node_info = SetupNodeInfo(candidate);
        NodeToLayout node_to_layout = {
            node_info,
            CalculateOffset(node_info, only_layout, /* is_first_run */ false,
                            needs_anchor_queries ? &*anchor_queries : nullptr)};
        const NGLayoutResult* result =
            LayoutOOFNode(node_to_layout, only_layout);
        container_builder_->AddResult(
            *result, result->OutOfFlowPositionedOffset(),
            /* relative_offset */ absl::nullopt, &candidate.inline_container);
        container_builder_->SetHasOutOfFlowFragmentChild(true);
        if (container_builder_->IsInitialColumnBalancingPass()) {
          container_builder_->PropagateTallestUnbreakableBlockSize(
              result->TallestUnbreakableBlockSize());
        }
        if (needs_anchor_queries) {
          DCHECK(anchor_queries);
          if (result->PhysicalFragment().HasAnchorQueryToPropagate())
            anchor_queries->SetChildren(container_builder_->Children(), items);
        }
      } else {
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }
    // Sweep any candidates that might have been added.
    // This happens when an absolute container has a fixed child.
    candidates->Shrink(0);
    container_builder_->SwapOutOfFlowPositionedCandidates(candidates);
  }
}

void NGOutOfFlowLayoutPart::HandleMulticolsWithPendingOOFs(
    NGBoxFragmentBuilder* container_builder) {
  if (!container_builder->HasMulticolsWithPendingOOFs())
    return;

  NGFragmentBuilder::MulticolCollection multicols_with_pending_oofs;
  container_builder->SwapMulticolsWithPendingOOFs(&multicols_with_pending_oofs);
  DCHECK(!multicols_with_pending_oofs.empty());

  while (!multicols_with_pending_oofs.empty()) {
    for (auto& multicol : multicols_with_pending_oofs)
      LayoutOOFsInMulticol(NGBlockNode(multicol.key), multicol.value);
    multicols_with_pending_oofs.clear();
    container_builder->SwapMulticolsWithPendingOOFs(
        &multicols_with_pending_oofs);
  }
}

void NGOutOfFlowLayoutPart::LayoutOOFsInMulticol(
    const NGBlockNode& multicol,
    const NGMulticolWithPendingOOFs<LogicalOffset>* multicol_info) {
  HeapVector<NGLogicalOOFNodeForFragmentation> oof_nodes_to_layout;
  ClearCollectionScope<HeapVector<NGLogicalOOFNodeForFragmentation>>
      oof_nodes_scope(&oof_nodes_to_layout);
  HeapVector<MulticolChildInfo> multicol_children;
  ClearCollectionScope<HeapVector<MulticolChildInfo>> multicol_scope(
      &multicol_children);

  const NGBlockBreakToken* current_column_break_token = nullptr;
  const NGBlockBreakToken* previous_multicol_break_token = nullptr;

  LayoutUnit column_inline_progression = kIndefiniteSize;
  LogicalOffset multicol_offset = multicol_info->multicol_offset;

  // Create a simplified container builder for multicol children. It cannot be
  // used to generate a fragment (since no size has been set, for one), but is
  // suitable for holding child fragmentainers while we're cloning them.
  NGConstraintSpace limited_multicol_constraint_space =
      CreateConstraintSpaceForMulticol(multicol);
  NGFragmentGeometry limited_fragment_geometry =
      CalculateInitialFragmentGeometry(limited_multicol_constraint_space,
                                       multicol, /* break_token */ nullptr);
  NGBoxFragmentBuilder limited_multicol_container_builder =
      CreateContainerBuilderForMulticol(multicol,
                                        limited_multicol_constraint_space,
                                        limited_fragment_geometry);
  // The block size that we set on the multicol builder doesn't matter since
  // we only care about the size of the fragmentainer children when laying out
  // the remaining OOFs.
  limited_multicol_container_builder.SetFragmentsTotalBlockSize(LayoutUnit());

  limited_multicol_container_builder.SetDisableOOFDescendantsPropagation();

  WritingDirectionMode writing_direction =
      multicol.Style().GetWritingDirection();
  const NGPhysicalBoxFragment* last_fragment_with_fragmentainer = nullptr;

  // Accumulate all of the pending OOF positioned nodes that are stored inside
  // |multicol|.
  for (auto& multicol_fragment : multicol.GetLayoutBox()->PhysicalFragments()) {
    const NGPhysicalBoxFragment* multicol_box_fragment =
        To<NGPhysicalBoxFragment>(&multicol_fragment);

    const ComputedStyle& style = multicol_box_fragment->Style();
    const WritingModeConverter converter(writing_direction,
                                         multicol_box_fragment->Size());
    wtf_size_t current_column_index = kNotFound;

    if (column_inline_progression == kIndefiniteSize) {
      // TODO(almaher): This should eventually include scrollbar, as well.
      NGBoxStrut border_padding =
          multicol_box_fragment->Borders().ConvertToLogical(writing_direction) +
          multicol_box_fragment->Padding().ConvertToLogical(writing_direction);
      LayoutUnit available_inline_size =
          multicol_box_fragment->Size()
              .ConvertToLogical(writing_direction.GetWritingMode())
              .inline_size -
          border_padding.InlineSum();
      column_inline_progression =
          ColumnInlineProgression(available_inline_size, style);
    }

    // Collect the children of the multicol fragments.
    for (auto& child :
         multicol_box_fragment->GetMutableChildrenForOutOfFlow().Children()) {
      const auto* fragment = child.get();
      LogicalOffset offset =
          converter.ToLogical(child.Offset(), fragment->Size());
      if (fragment->IsFragmentainerBox()) {
        current_column_break_token =
            To<NGBlockBreakToken>(fragment->BreakToken());
        current_column_index = multicol_children.size();
        last_fragment_with_fragmentainer = multicol_box_fragment;
      }

      limited_multicol_container_builder.AddChild(*fragment, offset);
      multicol_children.emplace_back(MulticolChildInfo(&child));
    }

    // If a column fragment is updated with OOF children, we may need to update
    // the reference to its break token in its parent's break token. There
    // should be at most one column break token per parent break token
    // (representing the last column laid out in that fragment). Thus, search
    // for |current_column_break_token| in |multicol_box_fragment|'s list of
    // child break tokens and update the stored MulticolChildInfo if found.
    const NGBlockBreakToken* break_token = multicol_box_fragment->BreakToken();
    if (current_column_index != kNotFound && break_token &&
        break_token->ChildBreakTokens().size()) {
      // If there is a column break token, it will be the last item in its
      // parent's list of break tokens.
      const auto children = break_token->ChildBreakTokens();
      const NGBlockBreakToken* child_token =
          To<NGBlockBreakToken>(children[children.size() - 1].Get());
      if (child_token == current_column_break_token) {
        MulticolChildInfo& child_info = multicol_children[current_column_index];
        child_info.parent_break_token = break_token;
      }
    }

    // Convert the OOF fragmentainer descendants to the logical coordinate space
    // and store the resulting nodes inside |oof_nodes_to_layout|.
    for (const auto& descendant :
         NGFragmentedOutOfFlowData::OutOfFlowPositionedFragmentainerDescendants(
             *multicol_box_fragment)) {
      if (oof_nodes_to_layout.empty() &&
          multicol_info->fixedpos_containing_block.Fragment() &&
          previous_multicol_break_token) {
        // At this point, the multicol offset is the offset from the fixedpos
        // containing block to the first multicol fragment holding OOF
        // fragmentainer descendants. Update this offset such that it is the
        // offset from the fixedpos containing block to the top of the first
        // multicol fragment.
        multicol_offset.block_offset -=
            previous_multicol_break_token->ConsumedBlockSize();
      }
      const NGPhysicalFragment* containing_block_fragment =
          descendant.containing_block.Fragment();
      // If the containing block is not set, that means that the inner multicol
      // was its containing block, and the OOF will be laid out elsewhere.
      if (!containing_block_fragment)
        continue;
      LogicalOffset containing_block_offset =
          converter.ToLogical(descendant.containing_block.Offset(),
                              containing_block_fragment->Size());
      LogicalOffset containing_block_rel_offset =
          converter.ToLogical(descendant.containing_block.RelativeOffset(),
                              containing_block_fragment->Size());

      const NGPhysicalFragment* fixedpos_containing_block_fragment =
          descendant.fixedpos_containing_block.Fragment();
      LogicalOffset fixedpos_containing_block_offset;
      LogicalOffset fixedpos_containing_block_rel_offset;
      if (fixedpos_containing_block_fragment) {
        fixedpos_containing_block_offset =
            converter.ToLogical(descendant.fixedpos_containing_block.Offset(),
                                fixedpos_containing_block_fragment->Size());
        fixedpos_containing_block_rel_offset = RelativeInsetToLogical(
            descendant.fixedpos_containing_block.RelativeOffset(),
            writing_direction);
      }

      NGInlineContainer<LogicalOffset> inline_container(
          descendant.inline_container.container,
          converter.ToLogical(descendant.inline_container.relative_offset,
                              PhysicalSize()));

      NGInlineContainer<LogicalOffset> fixedpos_inline_container(
          descendant.fixedpos_inline_container.container,
          converter.ToLogical(
              descendant.fixedpos_inline_container.relative_offset,
              PhysicalSize()));

      // The static position should remain relative to its containing block
      // fragment.
      const WritingModeConverter containing_block_converter(
          writing_direction, containing_block_fragment->Size());
      NGLogicalStaticPosition static_position =
          descendant.StaticPosition().ConvertToLogical(
              containing_block_converter);

      NGLogicalOOFNodeForFragmentation node = {
          descendant.Node(),
          static_position,
          inline_container,
          NGContainingBlock<LogicalOffset>(
              containing_block_offset, containing_block_rel_offset,
              containing_block_fragment,
              descendant.containing_block.ClippedContainerBlockOffset(),
              descendant.containing_block.IsInsideColumnSpanner(),
              descendant.containing_block.RequiresContentBeforeBreaking()),
          NGContainingBlock<LogicalOffset>(
              fixedpos_containing_block_offset,
              fixedpos_containing_block_rel_offset,
              fixedpos_containing_block_fragment,
              descendant.fixedpos_containing_block
                  .ClippedContainerBlockOffset(),
              descendant.fixedpos_containing_block.IsInsideColumnSpanner(),
              descendant.fixedpos_containing_block
                  .RequiresContentBeforeBreaking()),
          fixedpos_inline_container};
      oof_nodes_to_layout.push_back(node);
    }
    previous_multicol_break_token = break_token;
  }
  // When an OOF's CB is a spanner (or a descendant of a spanner), we will lay
  // out the OOF at the next fragmentation context root ancestor. As such, we
  // remove any such OOF nodes from the nearest multicol's list of OOF
  // descendants during OOF node propagation, which may cause
  // |oof_nodes_to_layout| to be empty. Return early if this is the case.
  if (oof_nodes_to_layout.empty())
    return;

  DCHECK(!limited_multicol_container_builder
              .HasOutOfFlowFragmentainerDescendants());

  wtf_size_t old_fragment_count =
      limited_multicol_container_builder.Children().size();

  LogicalOffset fragmentainer_progression(column_inline_progression,
                                          LayoutUnit());

  // Layout the OOF positioned elements inside the inner multicol.
  NGOutOfFlowLayoutPart inner_part(multicol, limited_multicol_constraint_space,
                                   &limited_multicol_container_builder);
  inner_part.allow_first_tier_oof_cache_ = false;
  inner_part.outer_container_builder_ =
      outer_container_builder_ ? outer_container_builder_ : container_builder_;
  inner_part.LayoutFragmentainerDescendants(
      &oof_nodes_to_layout, fragmentainer_progression,
      multicol_info->fixedpos_containing_block.Fragment(), &multicol_children);

  wtf_size_t new_fragment_count =
      limited_multicol_container_builder.Children().size();

  if (old_fragment_count != new_fragment_count) {
    DCHECK_GT(new_fragment_count, old_fragment_count);
    // We created additional fragmentainers to hold OOFs, and this is in a
    // nested fragmentation context. This means that the multicol fragment has
    // already been created, and we will therefore need to replace one of those
    // fragments. Locate the last multicol container fragment that already has
    // fragmentainers, and append all new fragmentainers there. Note that this
    // means that we may end up with more inner fragmentainers than what we
    // actually have room for (so that they'll overflow in the inline
    // direction), because we don't attempt to put fragmentainers into
    // additional multicol fragments in outer fragmentainers. This is an
    // implementation limitation which we can hopefully live with.
    DCHECK(last_fragment_with_fragmentainer);
    LayoutBox& box = *last_fragment_with_fragmentainer->MutableOwnerLayoutBox();
    wtf_size_t fragment_count = box.PhysicalFragmentCount();
    DCHECK_GE(fragment_count, 1u);
    const NGLayoutResult* old_result = nullptr;
    wtf_size_t fragment_idx = fragment_count - 1;
    do {
      old_result = box.GetLayoutResult(fragment_idx);
      if (&old_result->PhysicalFragment() == last_fragment_with_fragmentainer)
        break;
      DCHECK_GT(fragment_idx, 0u);
      fragment_idx--;
    } while (true);

    // We have located the right multicol fragment to replace. Re-use its old
    // constraint space and establish a layout algorithm to regenerate the
    // fragment.
    const NGConstraintSpace& constraint_space =
        old_result->GetConstraintSpaceForCaching();
    NGFragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
        constraint_space, multicol, /* break_token */ nullptr);
    NGLayoutAlgorithmParams params(multicol, fragment_geometry,
                                   constraint_space);
    NGSimplifiedLayoutAlgorithm algorithm(params, *old_result,
                                          /* keep_old_size */ true);

    // First copy the fragmentainers (and other child fragments) that we already
    // had.
    algorithm.CloneOldChildren();

    WritingModeConverter converter(constraint_space.GetWritingDirection(),
                                   old_result->PhysicalFragment().Size());
    LayoutUnit additional_column_block_size;
    // Then append the new fragmentainers.
    for (wtf_size_t i = old_fragment_count; i < new_fragment_count; i++) {
      const NGLogicalLink& child =
          limited_multicol_container_builder.Children()[i];
      algorithm.AppendNewChildFragment(*child.fragment, child.offset);
      additional_column_block_size +=
          converter.ToLogical(child.fragment->Size()).block_size;
    }

    // We've already written back to legacy for |multicol|, but if we added
    // new columns to hold any OOF descendants, we need to extend the final
    // size of the legacy flow thread to encompass those new columns.
    multicol.MakeRoomForExtraColumns(additional_column_block_size);

    // Create a new multicol container fragment and replace all references to
    // the old one with this new one.
    const NGLayoutResult* new_result =
        algorithm.CreateResultAfterManualChildLayout();
    ReplaceFragment(std::move(new_result), *last_fragment_with_fragmentainer,
                    fragment_idx);
  }

  // Any descendants should have been handled in
  // LayoutFragmentainerDescendants(). However, if there were any candidates
  // found, pass them back to |container_builder_| so they can continue
  // propagating up the tree.
  DCHECK(
      !limited_multicol_container_builder.HasOutOfFlowPositionedDescendants());
  DCHECK(!limited_multicol_container_builder
              .HasOutOfFlowFragmentainerDescendants());
  limited_multicol_container_builder.TransferOutOfFlowCandidates(
      container_builder_, multicol_offset, multicol_info);

  // Handle any inner multicols with OOF descendants that may have propagated up
  // while laying out the direct OOF descendants of the current multicol.
  HandleMulticolsWithPendingOOFs(&limited_multicol_container_builder);
}

void NGOutOfFlowLayoutPart::LayoutFragmentainerDescendants(
    HeapVector<NGLogicalOOFNodeForFragmentation>* descendants,
    LogicalOffset fragmentainer_progression,
    bool outer_context_has_fixedpos_container,
    HeapVector<MulticolChildInfo>* multicol_children) {
  multicol_children_ = multicol_children;
  outer_context_has_fixedpos_container_ = outer_context_has_fixedpos_container;
  DCHECK(multicol_children_ || !outer_context_has_fixedpos_container_);

  original_column_block_size_ =
      ShrinkLogicalSize(container_builder_->InitialBorderBoxSize(),
                        container_builder_->BorderScrollbarPadding())
          .block_size;

  NGBoxFragmentBuilder* builder_for_anchor_query = container_builder_;
  if (outer_container_builder_) {
    // If this is an inner layout of the nested block fragmentation, and if this
    // block fragmentation context is block fragmented, |multicol_children|
    // doesn't have correct block offsets of fragmentainers anchor query needs.
    // Calculate the anchor query from the outer block fragmentation context
    // instead in order to get the correct offsets.
    for (const MulticolChildInfo& multicol_child : *multicol_children) {
      if (multicol_child.parent_break_token) {
        builder_for_anchor_query = outer_container_builder_;
        break;
      }
    }
  }
  NGLogicalAnchorQueryMap stitched_anchor_queries(
      *builder_for_anchor_query->Node().GetLayoutBox(),
      builder_for_anchor_query->Children(),
      builder_for_anchor_query->GetWritingDirection());

  // |descendants| are sorted by fragmentainers, and then by the layout order,
  // which is pre-order of the box tree. When fragments are pushed to later
  // fragmentainers by overflow, |descendants| need to be re-sorted by the
  // pre-order. Note that both |SortInPreOrder| and |IsInPreOrder| are not
  // cheap, limit only when needed.
  const bool may_have_anchors_on_oof = MayHaveAnchorQuery(*descendants);
  if (may_have_anchors_on_oof && !IsInPreOrder(*descendants))
    SortInPreOrder(descendants);

  HeapVector<HeapVector<NodeToLayout>> descendants_to_layout;
  ClearCollectionScope<HeapVector<HeapVector<NodeToLayout>>>
      descendants_to_layout_scope(&descendants_to_layout);

  // List of repeated fixed-positioned elements. Elements will be added as they
  // are discovered (which might not happen in the first iteration, if they are
  // nested inside another OOFs).
  HeapVector<NodeToLayout> repeated_fixedpos_descendants;
  ClearCollectionScope<HeapVector<NodeToLayout>>
      repeated_fixedpos_descendants_scope(&repeated_fixedpos_descendants);

  // The fragmentainer index at which we have to resume repetition of
  // fixed-positioned elements, if additional fragmentainers are added. We'll
  // add repeated elements to every fragmentainer that exists, but if there's a
  // nested OOF that triggers creation of additional fragmentainers, we'll need
  // to add the fixed-positioned elements to those as well.
  wtf_size_t previous_repeaded_fixedpos_resume_idx = WTF::kNotFound;

  while (descendants->size() > 0) {
    ComputeInlineContainingBlocksForFragmentainer(*descendants);

    // When there are anchor queries, each containing block should be laid out
    // separately. This loop chunks |descendants| by their containing blocks, if
    // they have anchor queries.
    base::span<NGLogicalOOFNodeForFragmentation> descendants_span =
        base::make_span(*descendants);
    for (;;) {
      bool has_new_descendants_span = false;
      // The CSS containing block of the last descendant, to group |descendants|
      // by the CSS containing block.
      const LayoutObject* last_css_containing_block = nullptr;

      // Sort the descendants by fragmentainer index in |descendants_to_layout|.
      // This will ensure that the descendants are laid out in the correct
      // order.
      DCHECK(!descendants_span.empty());
      for (auto& descendant : descendants_span) {
        if (GetFragmentainerType() == kFragmentColumn) {
          auto* containing_block = To<LayoutBox>(
              descendant.containing_block.Fragment()->GetLayoutObject());
          DCHECK(containing_block);

          // We may try to lay out an OOF once we reach a column spanner or when
          // column balancing. However, if the containing block has not finished
          // layout, we should wait to lay out the OOF in case its position is
          // dependent on its containing block's final size.
          if (containing_block->PhysicalFragments().back().BreakToken()) {
            delayed_descendants_.push_back(descendant);
            continue;
          }
        }

        // Ensure each containing block is laid out before laying out other
        // containing blocks. The CSS Anchor Positioning may evaluate
        // differently when the containing block is different, and may refer to
        // other containing blocks that were already laid out.
        //
        // Do this only when needed, because doing so may rebuild fragmentainers
        // multiple times, which can hit the performance when there are many
        // containing blocks in the block formatting context.
        //
        // Use |LayoutObject::Container|, not |LayoutObject::ContainingBlock|.
        // The latter is not the CSS containing block for inline boxes. See the
        // comment of |LayoutObject::ContainingBlock|.
        //
        // Note |descendant.containing_block.fragment| is |ContainingBlock|, not
        // the CSS containing block.
        if (!stitched_anchor_queries.IsEmpty() || may_have_anchors_on_oof) {
          const LayoutObject* css_containing_block =
              descendant.box->Container();
          DCHECK(css_containing_block);
          if (css_containing_block != last_css_containing_block) {
            // Chunking the layout of OOFs by the containing blocks is done only
            // if it has anchor query, for the performance reasons to minimize
            // the number of rebuilding fragmentainer fragments.
            if (last_css_containing_block &&
                (last_css_containing_block->MayHaveAnchorQuery() ||
                 may_have_anchors_on_oof)) {
              has_new_descendants_span = true;
              descendants_span = descendants_span.subspan(
                  &descendant - descendants_span.data());
              break;
            }
            last_css_containing_block = css_containing_block;
          }
        }

        NodeInfo node_info = SetupNodeInfo(descendant);
        NodeToLayout node_to_layout = {
            node_info,
            CalculateOffset(node_info, /* only_layout */ nullptr,
                            /* is_first_run */ true, &stitched_anchor_queries)};
        node_to_layout.containing_block_fragment =
            descendant.containing_block.Fragment();
        node_to_layout.offset_info.original_offset =
            node_to_layout.offset_info.offset;

        DCHECK(node_to_layout.offset_info.block_estimate);

        // Determine in which fragmentainer this OOF element will start its
        // layout and adjust the offset to be relative to that fragmentainer.
        wtf_size_t start_index = 0;
        ComputeStartFragmentIndexAndRelativeOffset(
            node_info.default_writing_direction.GetWritingMode(),
            *node_to_layout.offset_info.block_estimate,
            node_info.containing_block.ClippedContainerBlockOffset(),
            &start_index, &node_to_layout.offset_info.offset);
        if (start_index >= descendants_to_layout.size())
          descendants_to_layout.resize(start_index + 1);
        descendants_to_layout[start_index].emplace_back(node_to_layout);
      }

      HeapVector<NodeToLayout> fragmented_descendants;
      ClearCollectionScope<HeapVector<NodeToLayout>>
          fragmented_descendants_scope(&fragmented_descendants);
      fragmentainer_consumed_block_size_ = LayoutUnit();
      auto& children = FragmentationContextChildren();
      wtf_size_t num_children = children.size();

      // Set to true if an OOF inside a fragmentainer breaks. This does not
      // include repeated fixed-positioned elements.
      bool last_fragmentainer_has_break_inside = false;

      // Layout the OOF descendants in order of fragmentainer index.
      for (wtf_size_t index = 0; index < descendants_to_layout.size();
           index++) {
        const NGPhysicalFragment* fragment = nullptr;
        if (index < num_children)
          fragment = children[index].fragment;
        else if (column_balancing_info_)
          column_balancing_info_->num_new_columns++;

        // Skip over any column spanners.
        if (!fragment || fragment->IsFragmentainerBox()) {
          HeapVector<NodeToLayout>& pending_descendants =
              descendants_to_layout[index];

          if (!repeated_fixedpos_descendants.empty() &&
              index == previous_repeaded_fixedpos_resume_idx) {
            // This is a new fragmentainer, and we had previously added repeated
            // fixed-positioned elements to all preceding fragmentainers (in a
            // previous iteration; this may happen when there are nested OOFs).
            // We now need to make sure that we add the repeated
            // fixed-positioned elements to all new fragmentainers as well.
            fragmented_descendants.PrependVector(repeated_fixedpos_descendants);
            // We need to clear the vector, since we'll find and re-add all the
            // repeated elements (both these, and any new ones discovered) in
            // fragmented_descendants when we're done with the current loop.
            repeated_fixedpos_descendants.clear();
          }

          last_fragmentainer_has_break_inside = false;
          LayoutOOFsInFragmentainer(
              pending_descendants, index, fragmentainer_progression,
              &last_fragmentainer_has_break_inside, &fragmented_descendants);

          // Retrieve the updated or newly added fragmentainer, and add its
          // block contribution to the consumed block size. Skip this if we are
          // column balancing, though, since this is only needed when adding
          // OOFs to the builder in the true layout pass.
          if (!column_balancing_info_) {
            fragment = children[index].fragment;
            fragmentainer_consumed_block_size_ +=
                fragment->Size()
                    .ConvertToLogical(
                        container_builder_->Style().GetWritingMode())
                    .block_size;
          }
        }

        // Extend |descendants_to_layout| if an OOF element fragments into a
        // fragmentainer at an index that does not yet exist in
        // |descendants_to_layout|. At the same time we need to make sure that
        // repeated fixed-positioned elements don't trigger creation of
        // additional fragmentainers (since they'd just repeat forever).
        if (index == descendants_to_layout.size() - 1 &&
            (last_fragmentainer_has_break_inside ||
             (!fragmented_descendants.empty() &&
              index + 1 < FragmentationContextChildren().size()))) {
          descendants_to_layout.resize(index + 2);
        }
      }

      if (!fragmented_descendants.empty()) {
        // We have repeated fixed-positioned elements. If we add more
        // fragmentainers in the next iteration (because of nested OOFs), we
        // need to resume those when a new fragmentainer is added.
        DCHECK(container_builder_->Node().IsPaginatedRoot());
        DCHECK(previous_repeaded_fixedpos_resume_idx == WTF::kNotFound ||
               previous_repeaded_fixedpos_resume_idx <=
                   descendants_to_layout.size());
        previous_repeaded_fixedpos_resume_idx = descendants_to_layout.size();

        // Add all repeated fixed-positioned elements to a list that we'll
        // consume if we add more fragmentainers in a subsequent iteration
        // (because of nested OOFs), so that we keep on generating fragments for
        // the repeated fixed-positioned elements in the new fragmentainers as
        // well.
        repeated_fixedpos_descendants.AppendVector(fragmented_descendants);
      }
      descendants_to_layout.Shrink(0);

      if (!has_new_descendants_span)
        break;
      // If laying out by containing blocks and there are more containing blocks
      // to be laid out, move on to the next containing block. Before laying
      // them out, if OOFs have anchors, update the anchor queries.
      if (may_have_anchors_on_oof) {
        stitched_anchor_queries.SetChildren(
            builder_for_anchor_query->Children());
      }
    }

    // Sweep any descendants that might have been bubbled up from the fragment
    // to the |container_builder_|. This happens when we have nested absolute
    // position elements.
    //
    // Don't do this if we are in a column balancing pass, though, since we
    // won't propagate OOF info of nested OOFs in this case. Any OOFs already
    // added to the builder should remain there so that they can be handled
    // later.
    descendants->Shrink(0);
    if (!column_balancing_info_)
      container_builder_->SwapOutOfFlowFragmentainerDescendants(descendants);
  }

  if (container_builder_->Node().IsPaginatedRoot()) {
    // Finish repeated fixed-positioned elements.
    for (const NodeToLayout& node_to_layout : repeated_fixedpos_descendants) {
      const NGBlockNode& node = node_to_layout.node_info.node;
      DCHECK_EQ(node.Style().GetPosition(), EPosition::kFixed);
      node.FinishRepeatableRoot();
    }
  } else {
    DCHECK(repeated_fixedpos_descendants.empty());
  }
}

NGOutOfFlowLayoutPart::NodeInfo NGOutOfFlowLayoutPart::SetupNodeInfo(
    const NGLogicalOutOfFlowPositionedNode& oof_node) {
  NGBlockNode node = oof_node.Node();
  const NGPhysicalFragment* containing_block_fragment =
      oof_node.is_for_fragmentation
          ? To<NGLogicalOOFNodeForFragmentation>(oof_node)
                .containing_block.Fragment()
          : nullptr;

#if DCHECK_IS_ON()
  const LayoutObject* container =
      containing_block_fragment ? containing_block_fragment->GetLayoutObject()
                                : container_builder_->GetLayoutObject();

  if (container) {
    // "NGOutOfFlowLayoutPart container is ContainingBlock" invariant cannot be
    // enforced for tables. Tables are special, in that the ContainingBlock is
    // TABLE, but constraint space is generated by TBODY/TR/. This happens
    // because TBODY/TR are not LayoutBlocks, but LayoutBoxModelObjects.
    DCHECK(container == node.GetLayoutBox()->ContainingBlock() ||
           node.GetLayoutBox()->ContainingBlock()->IsTable());
  } else {
    // If there's no layout object associated, the containing fragment should be
    // a page, and the containing block of the node should be the LayoutView.
    DCHECK_EQ(containing_block_fragment->BoxType(),
              NGPhysicalFragment::kPageBox);
    DCHECK_EQ(node.GetLayoutBox()->ContainingBlock(),
              node.GetLayoutBox()->View());
  }
#endif

  const ContainingBlockInfo container_info = GetContainingBlockInfo(oof_node);
  const ComputedStyle& oof_style = node.Style();
  const auto oof_writing_direction = oof_style.GetWritingDirection();

  LogicalSize container_content_size = container_info.rect.size;
  PhysicalSize container_physical_content_size = ToPhysicalSize(
      container_content_size, ConstraintSpace().GetWritingMode());

  bool requires_content_before_breaking = false;

  // Adjust the |static_position| (which is currently relative to the default
  // container's border-box). ng_absolute_utils expects the static position to
  // be relative to the container's padding-box. Since
  // |container_info.rect.offset| is relative to its fragmentainer in this
  // case, we also need to adjust the offset to account for this.
  NGLogicalStaticPosition static_position = oof_node.static_position;
  static_position.offset -= container_info.rect.offset;
  if (containing_block_fragment) {
    const auto& containing_block_for_fragmentation =
        To<NGLogicalOOFNodeForFragmentation>(oof_node).containing_block;
    static_position.offset += containing_block_for_fragmentation.Offset();
    requires_content_before_breaking =
        containing_block_for_fragmentation.RequiresContentBeforeBreaking();
  }

  NGLogicalStaticPosition oof_static_position =
      static_position
          .ConvertToPhysical({ConstraintSpace().GetWritingDirection(),
                              container_physical_content_size})
          .ConvertToLogical(
              {oof_writing_direction, container_physical_content_size});

  // Need a constraint space to resolve offsets.
  NGConstraintSpaceBuilder builder(ConstraintSpace(), oof_writing_direction,
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(container_content_size);
  builder.SetPercentageResolutionSize(container_content_size);

  if (container_builder_->IsInitialColumnBalancingPass()) {
    // The |fragmentainer_offset_delta| will not make a difference in the
    // initial column balancing pass.
    SetupSpaceBuilderForFragmentation(
        ConstraintSpace(), node,
        /* fragmentainer_offset_delta */ LayoutUnit(), &builder,
        /* is_new_fc */ true,
        /* requires_content_before_breaking */ false);
  }

  NGContainingBlock<LogicalOffset> containing_block;
  NGContainingBlock<LogicalOffset> fixedpos_containing_block;
  NGInlineContainer<LogicalOffset> fixedpos_inline_container;
  if (containing_block_fragment) {
    containing_block =
        To<NGLogicalOOFNodeForFragmentation>(oof_node).containing_block;
    fixedpos_containing_block = To<NGLogicalOOFNodeForFragmentation>(oof_node)
                                    .fixedpos_containing_block;
    fixedpos_inline_container = To<NGLogicalOOFNodeForFragmentation>(oof_node)
                                    .fixedpos_inline_container;
  }

  return NodeInfo(
      node, builder.ToConstraintSpace(), oof_static_position,
      container_physical_content_size, container_info,
      ConstraintSpace().GetWritingDirection(),
      /* is_fragmentainer_descendant */ containing_block_fragment,
      containing_block, fixedpos_containing_block, fixedpos_inline_container,
      oof_node.inline_container.container, requires_content_before_breaking);
}

const NGLayoutResult* NGOutOfFlowLayoutPart::LayoutOOFNode(
    NodeToLayout& oof_node_to_layout,
    const LayoutBox* only_layout,
    const NGConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const NodeInfo& node_info = oof_node_to_layout.node_info;
  OffsetInfo& offset_info = oof_node_to_layout.offset_info;
  if (offset_info.has_cached_layout_result) {
    DCHECK(offset_info.initial_layout_result);
    return offset_info.initial_layout_result;
  }

  NGBoxStrut scrollbars_before =
      ComputeScrollbarsForNonAnonymous(node_info.node);
  const NGLayoutResult* layout_result =
      Layout(oof_node_to_layout, fragmentainer_constraint_space,
             is_last_fragmentainer_so_far);

  // Since out-of-flow positioning sets up a constraint space with fixed
  // inline-size, the regular layout code (|NGBlockNode::Layout()|) cannot
  // re-layout if it discovers that a scrollbar was added or removed. Handle
  // that situation here. The assumption is that if intrinsic logical widths are
  // dirty after layout, AND its inline-size depends on the intrinsic logical
  // widths, it means that scrollbars appeared or disappeared.
  if (node_info.node.GetLayoutBox()->IntrinsicLogicalWidthsDirty() &&
      offset_info.inline_size_depends_on_min_max_sizes) {
    WritingDirectionMode writing_mode_direction =
        node_info.node.Style().GetWritingDirection();
    bool freeze_horizontal = false, freeze_vertical = false;
    NGBoxStrut scrollbars_after =
        ComputeScrollbarsForNonAnonymous(node_info.node);
    bool ignore_first_inline_freeze =
        scrollbars_after.InlineSum() && scrollbars_after.BlockSum();
    // If we're in a measure pass, freeze both scrollbars right away, to avoid
    // quadratic time complexity for deeply nested flexboxes.
    if (ConstraintSpace().CacheSlot() == NGCacheSlot::kMeasure) {
      freeze_horizontal = freeze_vertical = true;
      ignore_first_inline_freeze = false;
    }
    do {
      // Freeze any scrollbars that appeared, and relayout. Repeat until both
      // have appeared, or until the scrollbar situation doesn't change,
      // whichever comes first.
      AddScrollbarFreeze(scrollbars_before, scrollbars_after,
                         writing_mode_direction, &freeze_horizontal,
                         &freeze_vertical);
      if (ignore_first_inline_freeze) {
        ignore_first_inline_freeze = false;
        // We allow to remove the inline-direction scrollbar only once
        // because the box might have unnecessary scrollbar due to
        // SetIsFixedInlineSize(true).
        if (writing_mode_direction.IsHorizontal())
          freeze_horizontal = false;
        else
          freeze_vertical = false;
      }
      scrollbars_before = scrollbars_after;
      PaintLayerScrollableArea::FreezeScrollbarsRootScope freezer(
          *node_info.node.GetLayoutBox(), freeze_horizontal, freeze_vertical);

      if (!IsBreakInside(oof_node_to_layout.break_token)) {
        // The offset itself does not need to be recalculated. However, the
        // |node_dimensions| and |initial_layout_result| may need to be updated,
        // so recompute the OffsetInfo.
        //
        // Only do this if we're currently building the first fragment of the
        // OOF. If we're resuming after a fragmentainer break, we can't update
        // our intrinsic inline-size. First of all, the intrinsic inline-size
        // should be the same across all fragments [1], and besides, this
        // operation would lead to performing a non-fragmented layout pass (to
        // measure intrinsic block-size; see IntrinsicBlockSizeFunc in
        // ComputeOutOfFlowBlockDimensions()), which in turn would overwrite the
        // result of the first fragment entry in LayoutBox without a break
        // token, causing major confusion everywhere.
        //
        // [1] https://drafts.csswg.org/css-break/#varying-size-boxes
        offset_info = CalculateOffset(node_info, only_layout,
                                      /* is_first_run */ false);
      }

      layout_result = Layout(oof_node_to_layout, fragmentainer_constraint_space,
                             is_last_fragmentainer_so_far);

      scrollbars_after = ComputeScrollbarsForNonAnonymous(node_info.node);
      DCHECK(!freeze_horizontal || !freeze_vertical ||
             scrollbars_after == scrollbars_before);
    } while (scrollbars_after != scrollbars_before);
  }

  return layout_result;
}

NGOutOfFlowLayoutPart::OffsetInfo NGOutOfFlowLayoutPart::CalculateOffset(
    const NodeInfo& node_info,
    const LayoutBox* only_layout,
    bool is_first_run,
    const NGLogicalAnchorQueryMap* anchor_queries) {
  const ComputedStyle* style = &node_info.node.Style();

  // If `@position-fallback` exists, let |TryCalculateOffset| check if the
  // result fits the available space.
  Element* element = DynamicTo<Element>(node_info.node.GetDOMNode());
  absl::optional<wtf_size_t> fallback_index;
  const ComputedStyle* next_fallback_style = nullptr;
  const LayoutObject* implicit_anchor = nullptr;
  gfx::Vector2dF anchor_scroll_offset;
  if (element) {
    if (UNLIKELY(style->PositionFallback())) {
      DCHECK(RuntimeEnabledFeatures::CSSAnchorPositioningEnabled());
      next_fallback_style = element->StyleForPositionFallback(0);
      if (next_fallback_style) {
        fallback_index = 0;
      }
      if (element->GetAnchorScrollData()) {
        anchor_scroll_offset =
            element->GetAnchorScrollData()->AccumulatedScrollOffset();
      }
    }
    if (element->ImplicitAnchorElement())
      implicit_anchor = element->ImplicitAnchorElement()->GetLayoutObject();
  }

  // See anchor_scroll_data.h for documentation of non-overflowing ranges.
  Vector<PhysicalScrollRange> non_overflowing_ranges;
  absl::optional<OffsetInfo> offset_info;
  while (!offset_info) {
    if (next_fallback_style) {
      DCHECK(element);
      style = next_fallback_style;
      next_fallback_style =
          element->StyleForPositionFallback(*fallback_index + 1);
    }

    const bool try_fit_available_space = next_fallback_style;
    PhysicalScrollRange non_overflowing_range;
    offset_info = TryCalculateOffset(
        node_info, *style, only_layout, anchor_queries, implicit_anchor,
        try_fit_available_space, is_first_run, &non_overflowing_range);

    // Also check if it fits the containing block after applying scroll offset.
    if (offset_info && next_fallback_style) {
      non_overflowing_ranges.push_back(non_overflowing_range);
      if (!non_overflowing_range.Contains(anchor_scroll_offset)) {
        offset_info = absl::nullopt;
      }
    }

    if (!offset_info) {
      ++*fallback_index;
    }
  }

  if (fallback_index) {
    offset_info->fallback_index = fallback_index;
    offset_info->non_overflowing_ranges = std::move(non_overflowing_ranges);
  } else {
    DCHECK(!offset_info->fallback_index);
    DCHECK(offset_info->non_overflowing_ranges.empty());
  }

  return *offset_info;
}

absl::optional<NGOutOfFlowLayoutPart::OffsetInfo>
NGOutOfFlowLayoutPart::TryCalculateOffset(
    const NodeInfo& node_info,
    const ComputedStyle& candidate_style,
    const LayoutBox* only_layout,
    const NGLogicalAnchorQueryMap* anchor_queries,
    const LayoutObject* implicit_anchor,
    bool try_fit_available_space,
    bool is_first_run,
    PhysicalScrollRange* out_non_overflowing_range) {
  const WritingDirectionMode candidate_writing_direction =
      candidate_style.GetWritingDirection();
  const auto container_writing_direction =
      node_info.container_info.writing_direction;
  const LogicalSize container_content_size_in_candidate_writing_mode =
      node_info.container_physical_content_size.ConvertToLogical(
          candidate_writing_direction.GetWritingMode());

  // Determine if we need to actually run the full OOF-positioned sizing, and
  // positioning algorithm.
  //
  // The first-tier cache compares the given available-size. However we can't
  // reuse the result if the |ContainingBlockInfo::container_offset| may change.
  // This can occur when:
  //  - The default containing-block has borders and/or scrollbars.
  //  - The candidate has an inline container (instead of the default
  //    containing-block).
  // Note: Only check for cache results if this is our first layout pass.
  if (is_first_run && !try_fit_available_space && allow_first_tier_oof_cache_ &&
      !node_info.inline_container) {
    if (const NGLayoutResult* cached_result =
            node_info.node.CachedLayoutResultForOutOfFlowPositioned(
                container_content_size_in_candidate_writing_mode)) {
      OffsetInfo offset_info;
      offset_info.initial_layout_result = cached_result;
      offset_info.has_cached_layout_result = true;
      return offset_info;
    }
  }

  absl::optional<NGAnchorEvaluatorImpl> anchor_evaluator_storage;
  const WritingModeConverter container_converter(
      container_writing_direction, node_info.container_physical_content_size);
  if (anchor_queries) {
    // When the containing block is block-fragmented, the |container_builder_|
    // is the fragmentainer, not the containing block, and the coordinate system
    // is stitched. Use the given |anchor_query|.
    const LayoutObject* css_containing_block =
        node_info.node.GetLayoutBox()->Container();
    DCHECK(css_containing_block);
    anchor_evaluator_storage.emplace(
        *node_info.node.GetLayoutBox(), *anchor_queries,
        candidate_style.AnchorDefault(), implicit_anchor, *css_containing_block,
        container_converter, candidate_writing_direction,
        container_converter.ToPhysical(node_info.container_info.rect).offset);
  } else if (const NGLogicalAnchorQuery* anchor_query =
                 container_builder_->AnchorQuery()) {
    // Otherwise the |container_builder_| is the containing block.
    anchor_evaluator_storage.emplace(
        *node_info.node.GetLayoutBox(), *anchor_query,
        candidate_style.AnchorDefault(), implicit_anchor, container_converter,
        candidate_writing_direction,
        container_converter.ToPhysical(node_info.container_info.rect).offset);
  } else {
    anchor_evaluator_storage.emplace();
  }
  NGAnchorEvaluatorImpl* anchor_evaluator = &*anchor_evaluator_storage;

  const NGLogicalOutOfFlowInsets insets = ComputeOutOfFlowInsets(
      candidate_style, node_info.constraint_space.AvailableSize(),
      anchor_evaluator);

  const LogicalRect unclamped_available_rect =
      ComputeOutOfFlowAvailableRect(node_info.node, node_info.constraint_space,
                                    insets, node_info.static_position);

  const LogicalSize computed_available_size =
      unclamped_available_rect.size.ClampNegativeToZero();

  const NGBoxStrut border_padding =
      ComputeBorders(node_info.constraint_space, node_info.node) +
      ComputePadding(node_info.constraint_space, candidate_style);

  absl::optional<LogicalSize> replaced_size;
  if (node_info.node.IsReplaced()) {
    replaced_size = ComputeReplacedSize(
        node_info.node, node_info.constraint_space, border_padding,
        computed_available_size, ReplacedSizeMode::kNormal, anchor_evaluator);
  }

  OffsetInfo offset_info;
  NGLogicalOutOfFlowDimensions& node_dimensions = offset_info.node_dimensions;
  offset_info.inline_size_depends_on_min_max_sizes =
      ComputeOutOfFlowInlineDimensions(
          node_info.node, candidate_style, node_info.constraint_space, insets,
          border_padding, node_info.static_position, computed_available_size,
          replaced_size, container_writing_direction, anchor_evaluator,
          &node_dimensions);

  // Calculate the inline scroll offset range where the inline dimension fits.
  absl::optional<LayoutUnit> inline_scroll_min;
  absl::optional<LayoutUnit> inline_scroll_max;
  if (try_fit_available_space) {
    if (!CalculateNonOverflowingRangeInOneAxis(
            insets.inline_start, insets.inline_end,
            unclamped_available_rect.offset.inline_offset,
            unclamped_available_rect.InlineEndOffset(),
            node_dimensions.MarginBoxInlineStart(),
            node_dimensions.MarginBoxInlineEnd(), &inline_scroll_min,
            &inline_scroll_max)) {
      return absl::nullopt;
    }
  }

  // We may have already pre-computed our block-dimensions when determining
  // our min/max sizes, only run if needed.
  if (node_dimensions.size.block_size == kIndefiniteSize) {
    offset_info.initial_layout_result = ComputeOutOfFlowBlockDimensions(
        node_info.node, candidate_style, node_info.constraint_space, insets,
        border_padding, node_info.static_position, computed_available_size,
        replaced_size, container_writing_direction, anchor_evaluator,
        &node_dimensions);
  }

  // Calculate the block scroll offset range where the block dimension fits.
  absl::optional<LayoutUnit> block_scroll_min;
  absl::optional<LayoutUnit> block_scroll_max;
  if (try_fit_available_space) {
    if (!CalculateNonOverflowingRangeInOneAxis(
            insets.block_start, insets.block_end,
            unclamped_available_rect.offset.block_offset,
            unclamped_available_rect.BlockEndOffset(),
            node_dimensions.MarginBoxBlockStart(),
            node_dimensions.MarginBoxBlockEnd(), &block_scroll_min,
            &block_scroll_max)) {
      return absl::nullopt;
    }
  }

  offset_info.disable_first_tier_cache |=
      anchor_evaluator->HasAnchorFunctions();
  offset_info.block_estimate = node_dimensions.size.block_size;

  // Calculate the offsets.
  const NGBoxStrut inset =
      node_dimensions.inset.ConvertToPhysical(candidate_writing_direction)
          .ConvertToLogical(node_info.default_writing_direction);

  // |inset| is relative to the container's padding-box. Convert this to being
  // relative to the default container's border-box.
  const LogicalRect& container_rect = node_info.container_info.rect;
  offset_info.offset = container_rect.offset;
  offset_info.offset.inline_offset += inset.inline_start;
  offset_info.offset.block_offset += inset.block_start;

  if (!only_layout && !container_builder_->IsBlockFragmentationContextRoot()) {
    // OOFs contained by an inline that's been split into continuations are
    // special, as their offset is relative to a fragment that's not the same as
    // their containing NG fragment; take a look inside
    // AdjustOffsetForSplitInline() for further details. This doesn't apply if
    // block fragmentation is involved, though, since all OOFs are then child
    // fragments of the nearest fragmentainer.
    AdjustOffsetForSplitInline(node_info.node, container_builder_,
                               offset_info.offset);
  }

  if (try_fit_available_space) {
    *out_non_overflowing_range =
        LogicalScrollRange{inline_scroll_min, inline_scroll_max,
                           block_scroll_min, block_scroll_max}
            .ToPhysical(candidate_writing_direction);
  }

  return offset_info;
}

const NGLayoutResult* NGOutOfFlowLayoutPart::Layout(
    const NodeToLayout& oof_node_to_layout,
    const NGConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const NodeInfo& node_info = oof_node_to_layout.node_info;
  const OffsetInfo& offset_info = oof_node_to_layout.offset_info;

  const NGLayoutResult* layout_result = offset_info.initial_layout_result;
  // Reset the layout result computed earlier to allow fragmentation in the next
  // layout pass, if needed. Also do this if we're inside repeatable content, as
  // the pre-computed layout result is unusable then.
  if (fragmentainer_constraint_space ||
      ConstraintSpace().IsInsideRepeatableContent()) {
    layout_result = nullptr;
  }

  // Skip this step if we produced a fragment that can be reused when
  // estimating the block-size.
  if (!layout_result) {
    layout_result =
        GenerateFragment(oof_node_to_layout, fragmentainer_constraint_space,
                         is_last_fragmentainer_so_far);
  }

  if (layout_result->Status() != NGLayoutResult::kSuccess) {
    DCHECK_EQ(layout_result->Status(),
              NGLayoutResult::kOutOfFragmentainerSpace);
    return layout_result;
  }

  // Legacy grid and flexbox handle OOF-positioned margins on their own, and
  // break if we set them here.
  if (!container_builder_->GetLayoutObject()
           ->Style()
           ->IsDisplayFlexibleOrGridBox()) {
    node_info.node.GetLayoutBox()->SetMargin(
        offset_info.node_dimensions.margins.ConvertToPhysical(
            node_info.node.Style().GetWritingDirection()));
  }

  if (offset_info.fallback_index) {
    layout_result->GetMutableForOutOfFlow().SetPositionFallbackResult(
        *offset_info.fallback_index, offset_info.non_overflowing_ranges);
  }

  layout_result->GetMutableForOutOfFlow().SetOutOfFlowPositionedOffset(
      offset_info.offset,
      allow_first_tier_oof_cache_ && !offset_info.disable_first_tier_cache);

  return layout_result;
}

bool NGOutOfFlowLayoutPart::IsContainingBlockForCandidate(
    const NGLogicalOutOfFlowPositionedNode& candidate) {
  // Fragmentainers are not allowed to be containing blocks.
  if (container_builder_->IsFragmentainerBoxType())
    return false;

  EPosition position = candidate.Node().Style().GetPosition();

  // Candidates whose containing block is inline are always positioned inside
  // closest parent block flow.
  if (candidate.inline_container.container) {
    DCHECK(candidate.inline_container.container
               ->CanContainOutOfFlowPositionedElement(position));
    return container_builder_->GetLayoutObject() ==
           candidate.box->ContainingBlock();
  }
  return (is_absolute_container_ && position == EPosition::kAbsolute) ||
         (is_fixed_container_ && position == EPosition::kFixed);
}

// The fragment is generated in one of these two scenarios:
// 1. To estimate candidate's block size, in this case block_size is
//    container's available size.
// 2. To compute final fragment, when block size is known from the absolute
//    position calculation.
const NGLayoutResult* NGOutOfFlowLayoutPart::GenerateFragment(
    const NodeToLayout& oof_node_to_layout,
    const NGConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const NodeInfo& node_info = oof_node_to_layout.node_info;
  const OffsetInfo& offset_info = oof_node_to_layout.offset_info;
  const NGBlockBreakToken* break_token = oof_node_to_layout.break_token;
  const NGBlockNode& node = node_info.node;
  const auto& style = node.Style();
  const LayoutUnit block_offset = offset_info.offset.block_offset;
  LogicalSize container_content_size_in_candidate_writing_mode =
      node_info.container_physical_content_size.ConvertToLogical(
          style.GetWritingDirection().GetWritingMode());

  LayoutUnit inline_size = offset_info.node_dimensions.size.inline_size;
  LayoutUnit block_size = offset_info.block_estimate.value_or(
      container_content_size_in_candidate_writing_mode.block_size);
  LogicalSize logical_size(inline_size, block_size);
  // Convert from logical size in the writing mode of the child to the logical
  // size in the writing mode of the container. That's what the constraint space
  // builder expects.
  PhysicalSize physical_size =
      ToPhysicalSize(logical_size, style.GetWritingMode());
  LogicalSize available_size =
      physical_size.ConvertToLogical(ConstraintSpace().GetWritingMode());
  bool is_repeatable = false;

  NGConstraintSpaceBuilder builder(ConstraintSpace(),
                                   style.GetWritingDirection(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(
      container_content_size_in_candidate_writing_mode);
  builder.SetIsFixedInlineSize(true);

  // In some cases we will need the fragment size in order to calculate the
  // offset. We may have to lay out to get the fragment size. For block
  // fragmentation, we *need* to know the block-offset before layout. In other
  // words, in that case, we may have to lay out, calculate the offset, and then
  // lay out again at the correct block-offset.
  if (offset_info.block_estimate.has_value() &&
      (!fragmentainer_constraint_space || !offset_info.initial_layout_result)) {
    builder.SetIsFixedBlockSize(true);
  }

  if (fragmentainer_constraint_space) {
    if (container_builder_->Node().IsPaginatedRoot() &&
        style.GetPosition() == EPosition::kFixed &&
        !oof_node_to_layout.containing_block_fragment) {
      // Paginated fixed-positioned elements are repeated on every page, if
      // contained by the initial containing block (i.e. when not contained by a
      // transformed element or similar) and may therefore not fragment.
      DCHECK(container_builder_->Node().IsPaginatedRoot());
      DCHECK_EQ(node.Style().GetPosition(), EPosition::kFixed);
      builder.SetShouldRepeat(true);
      builder.SetIsInsideRepeatableContent(true);
      is_repeatable = true;
    } else {
      SetupSpaceBuilderForFragmentation(
          *fragmentainer_constraint_space, node, block_offset, &builder,
          /* is_new_fc */ true, node_info.requires_content_before_breaking);

      // Out-of-flow positioned elements whose containing block is inside
      // clipped overflow shouldn't generate any additional fragmentainers. Just
      // place everything in the last fragmentainer. This is similar to what
      // NGLayoutAlgorithm::RelayoutWithoutFragmentation() does for in-flow
      // content overflowing a clipped ancestor, except that in this case we
      // know up front that we should disable fragmentation.
      //
      // Note that this approach isn't perfect. We don't know where (in which
      // fragmentainer) the clipped container ends. It may have ended in some
      // fragmentainer earlier than the last one, in which case we should have
      // finished this OOF there. But we have no (easy) way of telling where
      // that might be. But as long as the OOF doesn't contribute to any
      // additional fragmentainers, we should be (pretty) good.
      if (is_last_fragmentainer_so_far &&
          node_info.containing_block.IsFragmentedInsideClippedContainer()) {
        builder.DisableFurtherFragmentation();
      }
    }
  } else if (container_builder_->IsInitialColumnBalancingPass()) {
    SetupSpaceBuilderForFragmentation(
        ConstraintSpace(), node, block_offset, &builder, /* is_new_fc */ true,
        /* requires_content_before_breaking */ false);
  }
  NGConstraintSpace space = builder.ToConstraintSpace();

  if (is_repeatable)
    return node.LayoutRepeatableRoot(space, break_token);
  return node.Layout(space, break_token);
}

void NGOutOfFlowLayoutPart::LayoutOOFsInFragmentainer(
    HeapVector<NodeToLayout>& pending_descendants,
    wtf_size_t index,
    LogicalOffset fragmentainer_progression,
    bool* has_actual_break_inside,
    HeapVector<NodeToLayout>* fragmented_descendants) {
  auto& children = FragmentationContextChildren();
  wtf_size_t num_children = children.size();
  bool is_new_fragment = index >= num_children;
  bool is_last_fragmentainer_so_far = index + 1 == num_children;

  DCHECK(fragmented_descendants);
  HeapVector<NodeToLayout> descendants_continued;
  ClearCollectionScope<HeapVector<NodeToLayout>> descendants_continued_scope(
      &descendants_continued);
  std::swap(*fragmented_descendants, descendants_continued);

  // If |index| is greater than the number of current children, and there are
  // no OOF children to be added, we will still need to add an empty
  // fragmentainer in its place. Otherwise, return early since there is no work
  // to do.
  if (pending_descendants.empty() && descendants_continued.empty() &&
      !is_new_fragment)
    return;

  const NGConstraintSpace& space = GetFragmentainerConstraintSpace(index);

  // If we are a new fragment, find a non-spanner fragmentainer as a basis.
  wtf_size_t original_index = index;
  while (index >= num_children ||
         !children[index].fragment->IsFragmentainerBox()) {
    DCHECK_GT(num_children, 0u);
    index--;
  }

  const auto& fragmentainer = children[index];
  DCHECK(fragmentainer.fragment->IsFragmentainerBox());
  const NGBlockNode& node = container_builder_->Node();
  const auto* fragment =
      To<NGPhysicalBoxFragment>(fragmentainer.fragment.Get());
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);
  LogicalOffset fragmentainer_offset = UpdatedFragmentainerOffset(
      fragmentainer.offset, index, fragmentainer_progression, is_new_fragment);

  const NGBlockBreakToken* previous_break_token = nullptr;
  if (!column_balancing_info_) {
    // Note: We don't fetch this when column balancing because we don't actually
    // create and add new fragments to the builder until a later layout pass.
    // However, the break token is only needed when we are actually adding to
    // the builder, so it is ok to leave this as nullptr in such cases.
    previous_break_token =
        PreviousFragmentainerBreakToken(*container_builder_, original_index);
  }
  NGLayoutAlgorithmParams params(node, fragment_geometry, space,
                                 previous_break_token,
                                 /* early_break */ nullptr);

  // |algorithm| corresponds to the "mutable copy" of our original
  // fragmentainer. As long as this "copy" hasn't been laid out via
  // NGSimplifiedOOFLayoutAlgorithm::Layout, we can append new items to it.
  NGSimplifiedOOFLayoutAlgorithm algorithm(params, *fragment, is_new_fragment);
  // Layout any OOF elements that are a continuation of layout first.
  for (auto& descendant : descendants_continued) {
    AddOOFToFragmentainer(descendant, &space, fragmentainer_offset, index,
                          is_last_fragmentainer_so_far, has_actual_break_inside,
                          &algorithm, fragmented_descendants);
  }
  // Once we've laid out the OOF elements that are a continuation of layout,
  // we can layout the OOF elements that start layout in the current
  // fragmentainer.
  for (auto& descendant : pending_descendants) {
    AddOOFToFragmentainer(descendant, &space, fragmentainer_offset, index,
                          is_last_fragmentainer_so_far, has_actual_break_inside,
                          &algorithm, fragmented_descendants);
  }

  // Finalize layout on the cloned fragmentainer and replace all existing
  // references to the old result.
  ReplaceFragmentainer(index, fragmentainer_offset, is_new_fragment,
                       &algorithm);
}

void NGOutOfFlowLayoutPart::AddOOFToFragmentainer(
    NodeToLayout& descendant,
    const NGConstraintSpace* fragmentainer_space,
    LogicalOffset fragmentainer_offset,
    wtf_size_t index,
    bool is_last_fragmentainer_so_far,
    bool* has_actual_break_inside,
    NGSimplifiedOOFLayoutAlgorithm* algorithm,
    HeapVector<NodeToLayout>* fragmented_descendants) {
  const NGLayoutResult* result =
      LayoutOOFNode(descendant, /* only_layout */ nullptr, fragmentainer_space,
                    is_last_fragmentainer_so_far);

  if (result->Status() != NGLayoutResult::kSuccess) {
    DCHECK_EQ(result->Status(), NGLayoutResult::kOutOfFragmentainerSpace);
    // If we're out of space, continue layout in the next fragmentainer.
    NodeToLayout fragmented_descendant = descendant;
    fragmented_descendant.offset_info.offset.block_offset = LayoutUnit();
    fragmented_descendants->emplace_back(fragmented_descendant);
    *has_actual_break_inside = true;
    return;
  }

  // Apply the relative positioned offset now that fragmentation is complete.
  LogicalOffset oof_offset = result->OutOfFlowPositionedOffset();
  LogicalOffset relative_offset =
      descendant.node_info.container_info.relative_offset;
  LogicalOffset adjusted_offset = oof_offset + relative_offset;

  // In the case where an OOF descendant of |descendant| has its containing
  // block outside the current fragmentation context, we will want to apply an
  // additional offset to |oof_offset| in PropagateOOFPositionedInfo() such that
  // it's the offset relative to the current builder rather than the offset such
  // that all fragmentainers are stacked on top of each other.
  LogicalOffset offset_adjustment = fragmentainer_offset;

  result->GetMutableForOutOfFlow().SetOutOfFlowPositionedOffset(
      adjusted_offset, allow_first_tier_oof_cache_);

  LogicalOffset additional_fixedpos_offset;
  if (descendant.node_info.fixedpos_containing_block.Fragment()) {
    additional_fixedpos_offset =
        descendant.offset_info.original_offset -
        descendant.node_info.fixedpos_containing_block.Offset();
    // Currently, |additional_fixedpos_offset| is the offset from the top of
    // |descendant| to the fixedpos containing block. Adjust this so that it
    // includes the block contribution of |descendant| from previous
    // fragmentainers. This ensures that any fixedpos descendants in the current
    // fragmentainer have the correct static position.
    if (descendant.break_token) {
      additional_fixedpos_offset.block_offset +=
          descendant.break_token->ConsumedBlockSize();
    }
  } else if (outer_context_has_fixedpos_container_) {
    // If the fixedpos containing block is in an outer fragmentation context,
    // we should adjust any fixedpos static positions such that they are
    // relative to the top of the inner multicol. These will eventually be
    // updated again with the offset from the multicol to the fixedpos
    // containing block such that the static positions are relative to the
    // containing block.
    DCHECK(multicol_children_);
    for (wtf_size_t i = std::min(index, multicol_children_->size()); i > 0u;
         i--) {
      MulticolChildInfo& column_info = (*multicol_children_)[i - 1];
      if (column_info.parent_break_token) {
        additional_fixedpos_offset.block_offset +=
            column_info.parent_break_token->ConsumedBlockSize();
        break;
      }
    }
  }

  const auto& physical_fragment =
      To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  const NGBlockBreakToken* break_token = physical_fragment.BreakToken();
  if (break_token) {
    // We must continue layout in the next fragmentainer. Update any information
    // in NodeToLayout, and add the node to |fragmented_descendants|.
    NodeToLayout fragmented_descendant = descendant;
    fragmented_descendant.break_token = break_token;
    if (!break_token->IsRepeated()) {
      // Fragmented nodes usually resume at the block-start of the next
      // fragmentainer. One exception is if there's fragmentainer overflow
      // caused by monolithic content in paged media. Then we need to move past
      // that.
      fragmented_descendant.offset_info.offset.block_offset =
          break_token->MonolithicOverflow();
      *has_actual_break_inside = true;
    }
    fragmented_descendants->emplace_back(fragmented_descendant);
  }

  // Figure out if the current OOF affects column balancing. Then return since
  // we don't want to add the OOFs to the builder until the current columns have
  // completed layout.
  if (column_balancing_info_) {
    LayoutUnit space_shortage = CalculateSpaceShortage(
        *fragmentainer_space, result, oof_offset.block_offset);
    column_balancing_info_->PropagateSpaceShortage(space_shortage);
    // We don't check the break appeal of the layout result to determine if
    // there is a violating break because OOFs aren't affected by the various
    // break rules. However, OOFs aren't pushed to the next fragmentainer if
    // they don't fit (when they are monolithic). Use |has_violating_break| to
    // tell the column algorithm when this happens so that it knows to attempt
    // to expand the columns in such cases.
    if (!column_balancing_info_->has_violating_break) {
      if (space_shortage > LayoutUnit() && !physical_fragment.BreakToken())
        column_balancing_info_->has_violating_break = true;
    }
    return;
  }

  // Propagate new data to the |container_builder_|. |AppendOutOfFlowResult|
  // will add the |result| to the fragmentainer, and replace the fragmentainer
  // in the |container_builder_|. |ReplaceChild| can't compute the differences
  // of the new and the old fragments, so it skips all propagations usually done
  // in |AddChild|.
  container_builder_->PropagateChildAnchors(
      physical_fragment, oof_offset + relative_offset + offset_adjustment);
  LayoutUnit containing_block_adjustment =
      container_builder_->BlockOffsetAdjustmentForFragmentainer(
          fragmentainer_consumed_block_size_);
  if (result->PhysicalFragment().NeedsOOFPositionedInfoPropagation()) {
    container_builder_->PropagateOOFPositionedInfo(
        result->PhysicalFragment(), oof_offset, relative_offset,
        offset_adjustment,
        /* inline_container */ nullptr, containing_block_adjustment,
        &descendant.node_info.containing_block,
        &descendant.node_info.fixedpos_containing_block,
        &descendant.node_info.fixedpos_inline_container,
        additional_fixedpos_offset);
  }
  algorithm->AppendOutOfFlowResult(result);

  // Copy the offset of the OOF node back to legacy such that it is relative
  // to its containing block rather than the fragmentainer that it is being
  // added to.
  if (!descendant.break_token) {
    const NGPhysicalBoxFragment* container =
        To<NGPhysicalBoxFragment>(descendant.containing_block_fragment.Get());

    if (!container) {
      // If we're paginated, we don't have a containing block fragment, but we
      // need one now, to calcualte the position correctly for the legacy
      // engine. Just pick the first page, which actually happens to be defined
      // as the initial containing block:
      // https://www.w3.org/TR/CSS22/page.html#page-box
      DCHECK(container_builder_->Node().IsPaginatedRoot());
      container = To<NGPhysicalBoxFragment>(
          FragmentationContextChildren()[0].fragment.Get());
    }

    LogicalOffset legacy_offset =
        descendant.offset_info.original_offset -
        descendant.node_info.container_info.offset_to_border_box;
    descendant.node_info.node.CopyChildFragmentPosition(
        physical_fragment,
        legacy_offset.ConvertToPhysical(
            container->Style().GetWritingDirection(), container->Size(),
            physical_fragment.Size()),
        *container, /* previous_container_break_token */ nullptr);
  }
}

void NGOutOfFlowLayoutPart::ReplaceFragmentainer(
    wtf_size_t index,
    LogicalOffset offset,
    bool create_new_fragment,
    NGSimplifiedOOFLayoutAlgorithm* algorithm) {
  // Don't update the builder when performing column balancing.
  if (column_balancing_info_)
    return;

  if (create_new_fragment) {
    const NGLayoutResult* new_result = algorithm->Layout();
    container_builder_->AddChild(new_result->PhysicalFragment(), offset);
  } else {
    const NGLayoutResult* new_result = algorithm->Layout();
    const NGPhysicalFragment* new_fragment = &new_result->PhysicalFragment();
    container_builder_->ReplaceChild(index, *new_fragment, offset);

    if (multicol_children_ && index < multicol_children_->size()) {
      // We are in a nested fragmentation context. Replace the column entry
      // (that already existed) directly in the existing multicol fragment. If
      // there any new columns, they will be appended as part of regenerating
      // the multicol fragment.
      MulticolChildInfo& column_info = (*multicol_children_)[index];
      column_info.mutable_link->fragment = new_fragment;
    }
  }
}

LogicalOffset NGOutOfFlowLayoutPart::UpdatedFragmentainerOffset(
    LogicalOffset offset,
    wtf_size_t index,
    LogicalOffset fragmentainer_progression,
    bool create_new_fragment) {
  if (create_new_fragment) {
    auto& children = FragmentationContextChildren();
    wtf_size_t num_children = children.size();
    if (index != num_children - 1 &&
        !children[index + 1].fragment->IsFragmentainerBox()) {
      // If we are a new fragment and are separated from other columns by a
      // spanner, compute the correct column offset to use.
      const auto& spanner = children[index + 1];
      DCHECK(spanner.fragment->IsColumnSpanAll());

      offset = spanner.offset;
      LogicalSize spanner_size = spanner.fragment->Size().ConvertToLogical(
          container_builder_->Style().GetWritingMode());
      // TODO(almaher): Include trailing spanner margin.
      offset.block_offset += spanner_size.block_size;
    } else {
      offset += fragmentainer_progression;
    }
  }
  return offset;
}

NGConstraintSpace NGOutOfFlowLayoutPart::GetFragmentainerConstraintSpace(
    wtf_size_t index) {
  auto& children = FragmentationContextChildren();
  wtf_size_t num_children = children.size();
  bool is_new_fragment = index >= num_children;
  // Allow margins to be discarded if this is not the first column in the
  // multicol container, and we're not right after a spanner.
  //
  // TODO(layout-dev): This check is incorrect in nested multicol. If the
  // previous outer fragmentainer ended with regular column content (i.e. not a
  // spanner), and this is the first column in the next outer fragmentainer, we
  // should still discard margins, since there is no explicit break involved.
  bool allow_discard_start_margin =
      is_new_fragment ||
      (index > 0 && children[index - 1].fragment->IsFragmentainerBox());

  // If we are a new fragment, find a non-spanner fragmentainer to base our
  // constraint space off of.
  while (index >= num_children ||
         !children[index].fragment->IsFragmentainerBox()) {
    DCHECK_GT(num_children, 0u);
    index--;
  }

  const auto& fragmentainer = children[index];
  DCHECK(fragmentainer.fragment->IsFragmentainerBox());
  const auto& fragment = To<NGPhysicalBoxFragment>(*fragmentainer.fragment);
  const WritingMode container_writing_mode =
      container_builder_->Style().GetWritingMode();
  LogicalSize column_size =
      fragment.Size().ConvertToLogical(container_writing_mode);

  // If we are a new fragment and are separated from other columns by a
  // spanner, compute the correct column block size to use.
  if (is_new_fragment && index != num_children - 1 &&
      original_column_block_size_ != kIndefiniteSize &&
      !children[index + 1].fragment->IsFragmentainerBox()) {
    column_size.block_size =
        original_column_block_size_ -
        container_builder_->BlockOffsetForAdditionalColumns();
    column_size.block_size = column_size.block_size.ClampNegativeToZero();
  }

  LogicalSize percentage_resolution_size =
      LogicalSize(column_size.inline_size,
                  container_builder_->ChildAvailableSize().block_size);

  // In the current implementation it doesn't make sense to restrict imperfect
  // breaks inside OOFs, since we never break and resume OOFs in a subsequent
  // outer fragmentainer anyway (we'll always stay in the current outer
  // fragmentainer and just create overflowing columns in the current row,
  // rather than moving to the next one).
  NGBreakAppeal min_break_appeal = kBreakAppealLastResort;

  return CreateConstraintSpaceForFragmentainer(
      ConstraintSpace(), GetFragmentainerType(), column_size,
      percentage_resolution_size, allow_discard_start_margin,
      /* balance_columns */ false, min_break_appeal);
}

// Compute in which fragmentainer the OOF element will start its layout and
// position the offset relative to that fragmentainer.
void NGOutOfFlowLayoutPart::ComputeStartFragmentIndexAndRelativeOffset(
    WritingMode default_writing_mode,
    LayoutUnit block_estimate,
    absl::optional<LayoutUnit> clipped_container_block_offset,
    wtf_size_t* start_index,
    LogicalOffset* offset) const {
  wtf_size_t child_index = 0;
  // The sum of all previous fragmentainers' block size.
  LayoutUnit used_block_size;
  // The sum of all previous fragmentainers' block size + the current one.
  LayoutUnit current_max_block_size;
  // The block size for the last fragmentainer we encountered.
  LayoutUnit fragmentainer_block_size;

  LayoutUnit target_block_offset = offset->block_offset;
  if (clipped_container_block_offset &&
      container_builder_->Node().IsPaginatedRoot()) {
    // If we're printing, and we have an OOF inside a clipped container, prevent
    // the start fragmentainer from preceding that of the clipped container.
    // This way we increase the likelihood of luring the OOF into the same
    // fragmentainer as the clipped container, so that we get the correct clip
    // rectangle during pre-paint.
    //
    // TODO(crbug.com/1371426): We might be able to get rid of this, if we
    // either get pre-paint to handle missing ancestor fragments better, or if
    // we rewrite OOF layout to always generate the necessary ancestor
    // fragments.
    target_block_offset =
        std::max(target_block_offset, *clipped_container_block_offset);
  }
  auto& children = FragmentationContextChildren();
  // TODO(bebeaudr): There is a possible performance improvement here as we'll
  // repeat this for each abspos in a same fragmentainer.
  for (auto& child : children) {
    if (child.fragment->IsFragmentainerBox()) {
      fragmentainer_block_size = child.fragment->Size()
                                     .ConvertToLogical(default_writing_mode)
                                     .block_size;
      fragmentainer_block_size =
          ClampedToValidFragmentainerCapacity(fragmentainer_block_size);
      current_max_block_size += fragmentainer_block_size;

      // Edge case: an abspos with an height of 0 positioned exactly at the
      // |current_max_block_size| won't be fragmented, so no break token will be
      // produced - as we'd expect. However, the break token is used to compute
      // the |fragmentainer_consumed_block_size_| stored on the
      // |container_builder_| when we have a nested abspos. Because we use that
      // value to position the nested abspos, its start offset would be off by
      // exactly one fragmentainer block size.
      if (target_block_offset < current_max_block_size ||
          (target_block_offset == current_max_block_size &&
           block_estimate == 0)) {
        *start_index = child_index;
        offset->block_offset -= used_block_size;
        return;
      }
      used_block_size = current_max_block_size;
    }
    child_index++;
  }
  // If the right fragmentainer hasn't been found yet, the OOF element will
  // start its layout in a proxy fragment.
  LayoutUnit remaining_block_offset = offset->block_offset - used_block_size;

  // If we are a new fragment and are separated from other columns by a
  // spanner, compute the correct fragmentainer_block_size.
  if (original_column_block_size_ != kIndefiniteSize &&
      !children[child_index - 1].fragment->IsFragmentainerBox()) {
    fragmentainer_block_size =
        original_column_block_size_ -
        container_builder_->BlockOffsetForAdditionalColumns();
    fragmentainer_block_size =
        ClampedToValidFragmentainerCapacity(fragmentainer_block_size);
  }

  wtf_size_t additional_fragment_count =
      int(floorf(remaining_block_offset / fragmentainer_block_size));
  *start_index = child_index + additional_fragment_count;
  offset->block_offset = remaining_block_offset -
                         additional_fragment_count * fragmentainer_block_size;
}

void NGOutOfFlowLayoutPart::ReplaceFragment(
    const NGLayoutResult* new_result,
    const NGPhysicalBoxFragment& old_fragment,
    wtf_size_t index) {
  // Replace the LayoutBox entry.
  LayoutBox& box = *old_fragment.MutableOwnerLayoutBox();
  box.ReplaceLayoutResult(new_result, index);

  // Replace the entry in the parent fragment. Locating the parent fragment
  // isn't straight-forward if the containing block is a multicol container.
  LayoutBox* containing_block = box.ContainingNGBox();

  if (box.IsOutOfFlowPositioned()) {
    // If the inner multicol is out-of-flow positioned, its fragments will be
    // found as direct children of fragmentainers in some ancestor fragmentation
    // context. It may not be the *nearest* fragmentation context, though, since
    // the OOF inner multicol may be contained by other OOFs, which in turn may
    // not be contained by the innermost multicol container, and so on. Skip
    // above all OOFs in the containing block chain, to find the right
    // fragmentation context root.
    while (containing_block->IsOutOfFlowPositioned() &&
           !containing_block->IsLayoutView())
      containing_block = containing_block->ContainingNGBox();
    // If we got to the root LayoutView, it has to mean that it establishes a
    // fragmentation context (i.e. we're printing).
    if (containing_block->IsLayoutView())
      DCHECK(containing_block->IsFragmentationContextRoot());
    else
      containing_block = containing_block->ContainingFragmentationContextRoot();

    // Since this is treated as a nested multicol container, we should always
    // find an outer fragmentation context.
    DCHECK(containing_block);
  }

  // Replace the old fragment with the new one, if it's inside |parent|.
  auto ReplaceChild = [&new_result, &old_fragment](
                          const NGPhysicalBoxFragment& parent) -> bool {
    for (NGLink& child_link :
         parent.GetMutableChildrenForOutOfFlow().Children()) {
      if (child_link.fragment != &old_fragment)
        continue;
      child_link.fragment = &new_result->PhysicalFragment();
      return true;
    }
    return false;
  };

  // Replace the old fragment with the new one, if |multicol_child| is a
  // fragmentainer and has the old fragment as a child.
  auto ReplaceFragmentainerChild =
      [ReplaceChild](const NGPhysicalFragment& multicol_child) -> bool {
    // We're going to replace a child of a fragmentainer. First check if it's a
    // fragmentainer at all.
    if (!multicol_child.IsFragmentainerBox())
      return false;
    const auto& fragmentainer = To<NGPhysicalBoxFragment>(multicol_child);
    // Then search and replace inside the fragmentainer.
    return ReplaceChild(fragmentainer);
  };

  if (!containing_block->IsFragmentationContextRoot()) {
    DCHECK_NE(containing_block, container_builder_->GetLayoutObject());
    DCHECK(!box.IsColumnSpanAll());
    for (const auto& parent_fragment : containing_block->PhysicalFragments()) {
      if (parent_fragment.HasItems()) {
        // Look inside the inline formatting context to find and replace the
        // fragment generated for the nested multicol container. This happens
        // when we have a floated "inline-level" nested multicol container with
        // an OOF inside.
        if (NGFragmentItems::ReplaceBoxFragment(
                old_fragment,
                To<NGPhysicalBoxFragment>(new_result->PhysicalFragment()),
                parent_fragment))
          return;
      }
      // Search inside child fragments of the containing block.
      if (ReplaceChild(parent_fragment))
        return;
    }
  } else if (containing_block == container_builder_->GetLayoutObject()) {
    DCHECK(!box.IsColumnSpanAll());
    // We're currently laying out |containing_block|, and it's a multicol
    // container. Search inside fragmentainer children in the builder.
    auto& children = FragmentationContextChildren();
    for (const NGLogicalLink& child : children) {
      if (ReplaceFragmentainerChild(*child.fragment))
        return;
    }
  } else {
    // |containing_block| has already been laid out, and it's a multicol
    // container. Search inside fragmentainer children of the fragments
    // generated for the containing block.
    for (const auto& multicol : containing_block->PhysicalFragments()) {
      if (box.IsColumnSpanAll()) {
        // Column spanners are found as direct children of the multicol.
        if (ReplaceChild(multicol))
          return;
      } else {
        for (const auto& child : multicol.Children()) {
          if (ReplaceFragmentainerChild(*child.fragment))
            return;
        }
      }
    }
  }
  NOTREACHED();
}

void NGOutOfFlowLayoutPart::SaveStaticPositionOnPaintLayer(
    LayoutBox* layout_box,
    const NGLogicalStaticPosition& position) const {
  const LayoutObject* parent =
      GetLayoutObjectForParentNode<const LayoutObject*>(layout_box);
  const LayoutObject* container = container_builder_->GetLayoutObject();
  if (parent == container ||
      (parent->IsLayoutInline() && parent->ContainingBlock() == container)) {
    DCHECK(layout_box->Layer());
    layout_box->Layer()->SetStaticPositionFromNG(
        ToStaticPositionForLegacy(position));
  }
}

NGLogicalStaticPosition NGOutOfFlowLayoutPart::ToStaticPositionForLegacy(
    NGLogicalStaticPosition position) const {
  // Legacy expects the static position to include the block contribution from
  // previous columns.
  if (const auto* break_token = container_builder_->PreviousBreakToken())
    position.offset.block_offset += break_token->ConsumedBlockSizeForLegacy();
  return position;
}

void NGOutOfFlowLayoutPart::ColumnBalancingInfo::PropagateSpaceShortage(
    LayoutUnit space_shortage) {
  UpdateMinimalSpaceShortage(space_shortage, &minimal_space_shortage);
}

void NGOutOfFlowLayoutPart::MulticolChildInfo::Trace(Visitor* visitor) const {
  visitor->Trace(parent_break_token);
}

void NGOutOfFlowLayoutPart::NodeInfo::Trace(Visitor* visitor) const {
  visitor->Trace(node);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

void NGOutOfFlowLayoutPart::OffsetInfo::Trace(Visitor* visitor) const {
  visitor->Trace(initial_layout_result);
}

void NGOutOfFlowLayoutPart::NodeToLayout::Trace(Visitor* visitor) const {
  visitor->Trace(node_info);
  visitor->Trace(offset_info);
  visitor->Trace(break_token);
  visitor->Trace(containing_block_fragment);
}

}  // namespace blink
