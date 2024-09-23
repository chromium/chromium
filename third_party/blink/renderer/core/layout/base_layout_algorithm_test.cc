// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/forms/fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"

namespace blink {

void BaseLayoutAlgorithmTest::SetUp() {
  EnableCompositing();
  RenderingTest::SetUp();
}

void BaseLayoutAlgorithmTest::AdvanceToLayoutPhase() {
  if (GetDocument().Lifecycle().GetState() ==
      DocumentLifecycle::kInPerformLayout)
    return;
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);
}

const PhysicalBoxFragment* BaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
    BlockNode node,
    const ConstraintSpace& space,
    const BreakToken* break_token) {
  AdvanceToLayoutPhase();

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  const LayoutResult* result =
      BlockLayoutAlgorithm(
          {node, fragment_geometry, space, To<BlockBreakToken>(break_token)})
          .Layout();

  return To<PhysicalBoxFragment>(&result->GetPhysicalFragment());
}

const PhysicalBoxFragment* BaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
    BlockNode node,
    const ConstraintSpace& space,
    const BreakToken* break_token) {
  AdvanceToLayoutPhase();

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  const LayoutResult* result =
      FieldsetLayoutAlgorithm(
          {node, fragment_geometry, space, To<BlockBreakToken>(break_token)})
          .Layout();

  return To<PhysicalBoxFragment>(&result->GetPhysicalFragment());
}

const PhysicalBoxFragment* BaseLayoutAlgorithmTest::GetBoxFragmentByElementId(
    const char* id) {
  LayoutObject* layout_object = GetLayoutObjectByElementId(id);
  CHECK(layout_object && layout_object->IsLayoutNGObject());
  const PhysicalBoxFragment* fragment =
      To<LayoutBlockFlow>(layout_object)->GetPhysicalFragment(0);
  CHECK(fragment);
  return fragment;
}

const PhysicalBoxFragment* BaseLayoutAlgorithmTest::CurrentFragmentFor(
    const LayoutBlockFlow* block_flow) {
  return block_flow->GetPhysicalFragment(0);
}

const PhysicalBoxFragment* FragmentChildIterator::NextChild(
    PhysicalOffset* fragment_offset) {
  if (!parent_)
    return nullptr;
  if (index_ >= parent_->Children().size())
    return nullptr;
  while (parent_->Children()[index_]->Type() !=
         PhysicalFragment::kFragmentBox) {
    ++index_;
    if (index_ >= parent_->Children().size())
      return nullptr;
  }
  auto& child = parent_->Children()[index_++];
  if (fragment_offset)
    *fragment_offset = child.Offset();
  return To<PhysicalBoxFragment>(child.get());
}

ConstraintSpace ConstructBlockLayoutTestConstraintSpace(
    WritingDirectionMode writing_direction,
    LogicalSize size,
    bool stretch_inline_size_if_auto,
    bool is_new_formatting_context,
    LayoutUnit fragmentainer_space_available) {
  FragmentationType block_fragmentation =
      fragmentainer_space_available != kIndefiniteSize
          ? FragmentationType::kFragmentColumn
          : FragmentationType::kFragmentNone;

  ConstraintSpaceBuilder builder(writing_direction.GetWritingMode(),
                                 writing_direction, is_new_formatting_context);
  builder.SetAvailableSize(size);
  builder.SetPercentageResolutionSize(size);
  builder.SetInlineAutoBehavior(stretch_inline_size_if_auto
                                    ? AutoSizeBehavior::kStretchImplicit
                                    : AutoSizeBehavior::kFitContent);
  builder.SetFragmentainerBlockSize(fragmentainer_space_available);
  builder.SetFragmentationType(block_fragmentation);
  if (block_fragmentation != FragmentationType::kFragmentNone) {
    builder.SetShouldPropagateChildBreakValues();
  }
  return builder.ToConstraintSpace();
}

}  // namespace blink
