// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/simplified_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

SimplifiedLayoutAlgorithm::SimplifiedLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    const LayoutResult& result,
    bool keep_old_size)
    : LayoutAlgorithm(params),
      previous_result_(result),
      writing_direction_(Style().GetWritingDirection()) {
  DCHECK(!Node().IsReplaced());

  const bool is_block_flow = Node().IsBlockFlow();
  const auto& physical_fragment =
      To<PhysicalBoxFragment>(result.GetPhysicalFragment());

  container_builder_.SetIsNewFormattingContext(
      physical_fragment.IsFormattingContextRoot());

  container_builder_.SetIsFirstForNode(physical_fragment.IsFirstForNode());

  if (physical_fragment.IsFragmentationContextRoot())
    container_builder_.SetIsBlockFragmentationContextRoot();

  if (keep_old_size) {
    // When we're cloning a fragment to insert additional fragmentainers to hold
    // OOFs, re-use the old break token. This may not be the last fragment.
    container_builder_.PresetNextBreakToken(physical_fragment.GetBreakToken());
  }

  if (is_block_flow && !physical_fragment.IsFieldsetContainer()) {
    container_builder_.SetIsInlineFormattingContext(
        Node().IsInlineFormattingContextRoot());
    container_builder_.SetStyleVariant(physical_fragment.GetStyleVariant());

    if (result.SubtreeModifiedMarginStrut())
      container_builder_.SetSubtreeModifiedMarginStrut();
    container_builder_.SetEndMarginStrut(result.EndMarginStrut());

    // Ensure that the parent layout hasn't asked us to move our BFC position.
    DCHECK_EQ(GetConstraintSpace().GetBfcOffset(),
              previous_result_.GetConstraintSpaceForCaching().GetBfcOffset());
    container_builder_.SetBfcLineOffset(result.BfcLineOffset());
    if (result.BfcBlockOffset())
      container_builder_.SetBfcBlockOffset(*result.BfcBlockOffset());

    if (result.LinesUntilClamp()) {
      container_builder_.SetLinesUntilClamp(result.LinesUntilClamp());
    }

    container_builder_.SetExclusionSpace(result.GetExclusionSpace());

    if (result.IsSelfCollapsing())
      container_builder_.SetIsSelfCollapsing();
    if (result.IsPushedByFloats())
      container_builder_.SetIsPushedByFloats();
    container_builder_.SetAdjoiningObjectTypes(
        result.GetAdjoiningObjectTypes());

    if (GetConstraintSpace().IsTableCell()) {
      container_builder_.SetHasCollapsedBorders(
          physical_fragment.HasCollapsedBorders());
      container_builder_.SetTableCellColumnIndex(
          physical_fragment.TableCellColumnIndex());
    } else {
      DCHECK(!physical_fragment.HasCollapsedBorders());
    }
  } else {
    // Only block-flow layout sets the following fields.
    DCHECK(physical_fragment.IsFormattingContextRoot());
    DCHECK(!Node().IsInlineFormattingContextRoot());
    DCHECK_EQ(physical_fragment.GetStyleVariant(), StyleVariant::kStandard);

    DCHECK(!result.SubtreeModifiedMarginStrut());
    DCHECK(result.EndMarginStrut().IsEmpty());

    DCHECK_EQ(GetConstraintSpace().GetBfcOffset(), BfcOffset());
    DCHECK_EQ(result.BfcLineOffset(), LayoutUnit());
    DCHECK_EQ(result.BfcBlockOffset().value_or(LayoutUnit()), LayoutUnit());

    DCHECK(!result.LinesUntilClamp());

    DCHECK(result.GetExclusionSpace().IsEmpty());

    DCHECK(!result.IsSelfCollapsing());
    DCHECK(!result.IsPushedByFloats());
    DCHECK_EQ(result.GetAdjoiningObjectTypes(), kAdjoiningNone);

    if (physical_fragment.IsFieldsetContainer())
      container_builder_.SetIsFieldsetContainer();

    if (physical_fragment.IsMathMLFraction())
      container_builder_.SetIsMathMLFraction();

    container_builder_.SetCustomLayoutData(result.CustomLayoutData());
  }

  if (physical_fragment.IsTable()) {
    container_builder_.SetTableColumnCount(result.TableColumnCount());
    container_builder_.SetTableGridRect(physical_fragment.TableGridRect());

    container_builder_.SetHasCollapsedBorders(
        physical_fragment.HasCollapsedBorders());

    if (const auto* table_column_geometries =
            physical_fragment.TableColumnGeometries())
      container_builder_.SetTableColumnGeometries(*table_column_geometries);

    if (const auto* table_collapsed_borders =
            physical_fragment.TableCollapsedBorders())
      container_builder_.SetTableCollapsedBorders(*table_collapsed_borders);

    if (const auto* table_collapsed_borders_geometry =
            physical_fragment.TableCollapsedBordersGeometry()) {
      container_builder_.SetTableCollapsedBordersGeometry(
          std::make_unique<TableFragmentData::CollapsedBordersGeometry>(
              *table_collapsed_borders_geometry));
    }
  } else if (physical_fragment.IsTableSection()) {
    if (const auto section_start_row_index =
            physical_fragment.TableSectionStartRowIndex()) {
      Vector<LayoutUnit> section_row_offsets =
          *physical_fragment.TableSectionRowOffsets();
      container_builder_.SetTableSectionCollapsedBordersGeometry(
          *section_start_row_index, std::move(section_row_offsets));
    }
  }

  if (physical_fragment.IsGrid()) {
    container_builder_.TransferGridLayoutData(
        std::make_unique<GridLayoutData>(*result.GetGridLayoutData()));
  } else if (physical_fragment.IsFrameSet()) {
    container_builder_.TransferFrameSetLayoutData(
        std::make_unique<FrameSetLayoutData>(
            *physical_fragment.GetFrameSetLayoutData()));
  }

  if (physical_fragment.IsHiddenForPaint())
    container_builder_.SetIsHiddenForPaint(true);

  if (auto first_baseline = physical_fragment.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = physical_fragment.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);
  if (physical_fragment.UseLastBaselineForInlineBaseline())
    container_builder_.SetUseLastBaselineForInlineBaseline();
  if (physical_fragment.IsTablePart()) {
    container_builder_.SetIsTablePart();
  }

  if (keep_old_size) {
    LayoutUnit old_block_size =
        LogicalFragment(writing_direction_, physical_fragment).BlockSize();
    container_builder_.SetFragmentBlockSize(old_block_size);
  } else {
    container_builder_.SetIntrinsicBlockSize(result.IntrinsicBlockSize());

    auto ComputeNewBlockSize = [&]() -> LayoutUnit {
      return ComputeBlockSizeForFragment(
          GetConstraintSpace(), Node(), BorderPadding(),
          result.IntrinsicBlockSize(),
          container_builder_.InitialBorderBoxSize().inline_size);
    };

    // Only block-flow is allowed to change its block-size during "simplified"
    // layout, all other layout types must remain the same size.
    if (is_block_flow) {
      container_builder_.SetFragmentBlockSize(ComputeNewBlockSize());
    } else {
      LayoutUnit old_block_size =
          LogicalFragment(writing_direction_, physical_fragment).BlockSize();
#if DCHECK_IS_ON()
      // Tables, sections, rows don't respect the typical block-sizing rules.
      if (!physical_fragment.IsTable() && !physical_fragment.IsTableSection() &&
          !physical_fragment.IsTableRow()) {
        DCHECK_EQ(old_block_size, ComputeNewBlockSize());
      }
#endif
      container_builder_.SetFragmentBlockSize(old_block_size);
    }
  }

  // We need the previous physical container size to calculate the position of
  // any child fragments.
  previous_physical_container_size_ = physical_fragment.Size();
}

void SimplifiedLayoutAlgorithm::AppendNewChildFragment(
    const PhysicalFragment& fragment,
    LogicalOffset offset) {
  container_builder_.AddChild(fragment, offset);
}

const LayoutResult* SimplifiedLayoutAlgorithm::Layout() {
  // Since simplified layout's |Layout()| function deals with laying out
  // children, we can early out if we are display-locked.
  if (Node().ChildLayoutBlockedByDisplayLock())
    return container_builder_.ToBoxFragment();

  const auto& previous_fragment =
      To<PhysicalBoxFragment>(previous_result_.GetPhysicalFragment());

  for (const auto& child_link : previous_fragment.Children()) {
    const auto& child_fragment = *child_link.get();

    // We'll add OOF-positioned candidates below.
    if (child_fragment.IsOutOfFlowPositioned())
      continue;

    // We don't need to relayout list-markers, or line-box fragments.
    if (child_fragment.IsListMarker() || child_fragment.IsLineBox()) {
      AddChildFragment(child_link, child_fragment);
      continue;
    }

    // Add the (potentially updated) layout result.
    const LayoutResult* result =
        BlockNode(To<LayoutBox>(child_fragment.GetMutableLayoutObject()))
            .SimplifiedLayout(child_fragment);

    // The child may have failed "simplified" layout! (Due to adding/removing
    // scrollbars). In this case we also return a nullptr, indicating a full
    // layout is required.
    if (!result)
      return nullptr;

    const MarginStrut end_margin_strut = result->EndMarginStrut();
    // No margins should pierce outside formatting-context roots.
    DCHECK(!result->GetPhysicalFragment().IsFormattingContextRoot() ||
           end_margin_strut.IsEmpty());

    AddChildFragment(child_link, result->GetPhysicalFragment(),
                     &end_margin_strut, result->IsSelfCollapsing());
  }

  // Iterate through all our OOF-positioned children and add them as candidates.
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (!child.IsOutOfFlowPositioned())
      continue;

    // TODO(ikilpatrick): Accessing the static-position from the layer isn't
    // ideal. We should save this on the physical fragment which initially
    // calculated it.
    const auto* layer = child.GetLayoutBox()->Layer();
    LogicalStaticPosition position = layer->GetStaticPosition();
    container_builder_.AddOutOfFlowChildCandidate(
        To<BlockNode>(child), position.offset, position.inline_edge,
        position.block_edge);
  }

  // We add both items and line-box fragments for existing mechanisms to work.
  // We may revisit this in future. See also |BoxFragmentBuilder::AddResult|.
  if (const FragmentItems* previous_items = previous_fragment.Items()) {
    auto* items_builder = container_builder_.ItemsBuilder();
    DCHECK(items_builder);
    DCHECK_EQ(items_builder->GetWritingDirection(), writing_direction_);
    const auto result =
        items_builder->AddPreviousItems(previous_fragment, *previous_items);
    if (!result.succeeded)
      return nullptr;
  }

  // Some layout types (grid) manually calculate their inflow-bounds rather
  // than use the value determined inside the builder. Just explicitly set this
  // from the previous fragment for all types.
  if (previous_fragment.InflowBounds()) {
    LogicalRect inflow_bounds =
        WritingModeConverter(writing_direction_,
                             previous_physical_container_size_)
            .ToLogical(*previous_fragment.InflowBounds());
    container_builder_.SetInflowBounds(inflow_bounds);
  }
  container_builder_.SetHasAdjoiningObjectDescendants(
      previous_fragment.HasAdjoiningObjectDescendants());
  container_builder_.SetMayHaveDescendantAboveBlockStart(
      previous_fragment.MayHaveDescendantAboveBlockStart());
  container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(
      previous_result_.HasDescendantThatDependsOnPercentageBlockSize());
  container_builder_.SetInitialBreakBefore(
      previous_result_.InitialBreakBefore());
  container_builder_.SetPreviousBreakAfter(previous_result_.FinalBreakAfter());

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

NOINLINE const LayoutResult*
SimplifiedLayoutAlgorithm::LayoutWithItemsBuilder() {
  FragmentItemsBuilder items_builder(writing_direction_);
  container_builder_.SetItemsBuilder(&items_builder);
  const LayoutResult* result = Layout();
  // Ensure stack-allocated |FragmentItemsBuilder| is not used anymore.
  // TODO(kojii): Revisit when the storage of |FragmentItemsBuilder| is
  // finalized.
  container_builder_.SetItemsBuilder(nullptr);
  return result;
}

void SimplifiedLayoutAlgorithm::AddChildFragment(
    const PhysicalFragmentLink& old_fragment,
    const PhysicalFragment& new_fragment,
    const MarginStrut* margin_strut,
    bool is_self_collapsing) {
  DCHECK_EQ(old_fragment->Size(), new_fragment.Size());

  // Determine the previous position in the logical coordinate system.
  LogicalOffset child_offset =
      WritingModeConverter(writing_direction_,
                           previous_physical_container_size_)
          .ToLogical(old_fragment.Offset(), new_fragment.Size());
  // Any relative offset will have already been applied, avoid re-adding one.
  std::optional<LogicalOffset> relative_offset = LogicalOffset();

  // Add the new fragment to the builder.
  container_builder_.AddChild(new_fragment, child_offset, margin_strut,
                              is_self_collapsing, relative_offset);
}

}  // namespace blink
