// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_simplified_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

NGSimplifiedLayoutAlgorithm::NGSimplifiedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const NGLayoutResult& result)
    : NGLayoutAlgorithm(params),
      previous_result_(result),
      writing_direction_(Style().GetWritingDirection()) {
  // Currently this only supports block-flow layout due to the static-position
  // calculations. If support for other layout types is added this logic will
  // need to be changed.
  bool is_block_flow = Node().IsBlockFlow();
  const NGPhysicalBoxFragment& physical_fragment =
      To<NGPhysicalBoxFragment>(result.PhysicalFragment());

  container_builder_.SetIsNewFormattingContext(
      physical_fragment.IsFormattingContextRoot());

  if (is_block_flow && !physical_fragment.IsFieldsetContainer()) {
    container_builder_.SetIsInlineFormattingContext(
        Node().IsInlineFormattingContextRoot());
    container_builder_.SetStyleVariant(physical_fragment.StyleVariant());

    if (result.SubtreeModifiedMarginStrut())
      container_builder_.SetSubtreeModifiedMarginStrut();
    container_builder_.SetEndMarginStrut(result.EndMarginStrut());

    // Ensure that the parent layout hasn't asked us to move our BFC position.
    DCHECK_EQ(ConstraintSpace().BfcOffset(),
              previous_result_.GetConstraintSpaceForCaching().BfcOffset());
    container_builder_.SetBfcLineOffset(result.BfcLineOffset());
    if (result.BfcBlockOffset())
      container_builder_.SetBfcBlockOffset(*result.BfcBlockOffset());

    if (result.LinesUntilClamp())
      container_builder_.SetLinesUntilClamp(result.LinesUntilClamp());

    NGExclusionSpace exclusion_space = result.ExclusionSpace();
    container_builder_.SetExclusionSpace(std::move(exclusion_space));

    if (result.IsSelfCollapsing())
      container_builder_.SetIsSelfCollapsing();
    if (result.IsPushedByFloats())
      container_builder_.SetIsPushedByFloats();
    container_builder_.SetAdjoiningObjectTypes(result.AdjoiningObjectTypes());
    container_builder_.SetUnpositionedListMarker(
        result.UnpositionedListMarker());

    if (physical_fragment.LastBaseline())
      container_builder_.SetLastBaseline(*physical_fragment.LastBaseline());

    if (ConstraintSpace().IsTableCell()) {
      if (physical_fragment.HasCollapsedBorders())
        container_builder_.SetHasCollapsedBorders(true);

      if (!ConstraintSpace().IsLegacyTableCell()) {
        container_builder_.SetTableCellColumnIndex(
            physical_fragment.TableCellColumnIndex());
      }
    } else {
      DCHECK(!physical_fragment.HasCollapsedBorders());
    }
  } else {
    // Only block-flow layout sets the following fields.
    DCHECK(physical_fragment.IsFormattingContextRoot());
    DCHECK(!Node().IsInlineFormattingContextRoot());
    DCHECK_EQ(physical_fragment.StyleVariant(), NGStyleVariant::kStandard);

    DCHECK(!result.SubtreeModifiedMarginStrut());
    DCHECK(result.EndMarginStrut().IsEmpty());

    DCHECK_EQ(ConstraintSpace().BfcOffset(), NGBfcOffset());
    DCHECK_EQ(result.BfcLineOffset(), LayoutUnit());
    DCHECK_EQ(result.BfcBlockOffset().value_or(LayoutUnit()), LayoutUnit());

    DCHECK(!result.LinesUntilClamp());

    DCHECK(result.ExclusionSpace().IsEmpty());

    DCHECK(!result.IsSelfCollapsing());
    DCHECK(!result.IsPushedByFloats());
    DCHECK_EQ(result.AdjoiningObjectTypes(), kAdjoiningNone);
    DCHECK(!result.UnpositionedListMarker());

    DCHECK(!physical_fragment.LastBaseline());

    if (physical_fragment.IsFieldsetContainer())
      container_builder_.SetIsFieldsetContainer();

    if (physical_fragment.IsMathMLFraction())
      container_builder_.SetIsMathMLFraction();

    container_builder_.SetCustomLayoutData(result.CustomLayoutData());
  }

  // TODO(atotic,ikilpatrick): Copy across table related data for table,
  // table-row, table-section.
  DCHECK(!physical_fragment.IsTable());
  DCHECK(!physical_fragment.IsTableRow());
  DCHECK(!physical_fragment.IsTableSection());

  if (physical_fragment.IsHiddenForPaint())
    container_builder_.SetIsHiddenForPaint(true);

  if (physical_fragment.Baseline())
    container_builder_.SetBaseline(*physical_fragment.Baseline());

  container_builder_.SetIntrinsicBlockSize(result.IntrinsicBlockSize());
  container_builder_.SetOverflowBlockSize(result.OverflowBlockSize());

  LayoutUnit new_block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), result.IntrinsicBlockSize(),
      container_builder_.InitialBorderBoxSize().inline_size);

  // Only block-flow is allowed to change its block-size during "simplified"
  // layout, all other layout types must remain the same size.
  if (is_block_flow) {
    container_builder_.SetFragmentBlockSize(new_block_size);
  } else {
    LayoutUnit old_block_size =
        NGFragment(writing_direction_, physical_fragment).BlockSize();
    DCHECK_EQ(old_block_size, new_block_size);
    container_builder_.SetFragmentBlockSize(old_block_size);
  }

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = physical_fragment.Size();
}

scoped_refptr<const NGLayoutResult> NGSimplifiedLayoutAlgorithm::Layout() {
  // Since simplified layout's |Layout()| function deals with laying out
  // children, we can early out if we are display-locked.
  if (Node().ChildLayoutBlockedByDisplayLock())
    return container_builder_.ToBoxFragment();

  const auto& previous_fragment =
      To<NGPhysicalBoxFragment>(previous_result_.PhysicalFragment());

  for (const auto& child_link : previous_fragment.Children()) {
    const auto& child_fragment =
        *To<NGPhysicalContainerFragment>(child_link.get());

    // We'll add OOF-positioned candidates below.
    if (child_fragment.IsOutOfFlowPositioned())
      continue;

    // We don't need to relayout list-markers, or line-box fragments.
    if (child_fragment.IsListMarker() || child_fragment.IsLineBox()) {
      AddChildFragment(child_link, child_fragment);
      continue;
    }

    // Add the (potentially updated) layout result.
    scoped_refptr<const NGLayoutResult> result =
        NGBlockNode(ToLayoutBox(child_fragment.GetMutableLayoutObject()))
            .SimplifiedLayout(child_fragment);

    // The child may have failed "simplified" layout! (Due to adding/removing
    // scrollbars). In this case we also return a nullptr, indicating a full
    // layout is required.
    if (!result)
      return nullptr;

    AddChildFragment(child_link, result->PhysicalFragment());
  }

  // Iterate through all our OOF-positioned children and add them as candidates.
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (!child.IsOutOfFlowPositioned())
      continue;

    // TODO(ikilpatrick): Accessing the static-position from the layer isn't
    // ideal. We should save this on the physical fragment which initially
    // calculated it.
    const auto* layer = child.GetLayoutBox()->Layer();
    NGLogicalStaticPosition position = layer->GetStaticPosition();

    container_builder_.AddOutOfFlowChildCandidate(
        To<NGBlockNode>(child), position.offset, position.inline_edge,
        position.block_edge, /* needs_block_offset_adjustment */ false);
  }

  // We add both items and line-box fragments for existing mechanisms to work.
  // We may revisit this in future. See also |NGBoxFragmentBuilder::AddResult|.
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    if (const NGFragmentItems* previous_items = previous_fragment.Items()) {
      auto* items_builder = container_builder_.ItemsBuilder();
      DCHECK(items_builder);
      DCHECK_EQ(items_builder->GetWritingDirection(), writing_direction_);
      items_builder->AddPreviousItems(previous_fragment, *previous_items);
    }
  }

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  // The block size may have been changed. This may affect the inline block
  // baseline if it is from the logical bottom margin edge.
  DCHECK_EQ(previous_fragment.LastBaseline().has_value(),
            container_builder_.LastBaseline().has_value());
  if (container_builder_.LastBaseline())
    container_builder_.SetLastBaselineToBlockEndMarginEdgeIfNeeded();

  return container_builder_.ToBoxFragment();
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGSimplifiedLayoutAlgorithm::LayoutWithItemsBuilder() {
  NGFragmentItemsBuilder items_builder(writing_direction_);
  container_builder_.SetItemsBuilder(&items_builder);
  scoped_refptr<const NGLayoutResult> result = Layout();
  // Ensure stack-allocated |NGFragmentItemsBuilder| is not used anymore.
  // TODO(kojii): Revisit when the storage of |NGFragmentItemsBuilder| is
  // finalized.
  container_builder_.SetItemsBuilder(nullptr);
  return result;
}

void NGSimplifiedLayoutAlgorithm::AddChildFragment(
    const NGLink& old_fragment,
    const NGPhysicalContainerFragment& new_fragment) {
  DCHECK_EQ(old_fragment->Size(), new_fragment.Size());

  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(old_fragment.Offset(), new_fragment.Size());

  // Un-apply the relative position offset.
  if (const auto* box_child = DynamicTo<NGPhysicalBoxFragment>(*old_fragment)) {
    if (box_child->Style().GetPosition() == EPosition::kRelative) {
      child_offset -= ComputeRelativeOffsetForBoxFragment(
          *box_child, ConstraintSpace().GetWritingDirection(),
          container_builder_.ChildAvailableSize());
    }
  }

  // Add the new fragment to the builder.
  container_builder_.AddChild(new_fragment, child_offset);
}

}  // namespace blink
