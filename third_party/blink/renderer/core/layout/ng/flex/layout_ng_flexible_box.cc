// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

LayoutNGFlexibleBox::LayoutNGFlexibleBox(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {
  if (element)
    GetDocument().IncLayoutFlexboxCounterNG();
}

bool LayoutNGFlexibleBox::HasTopOverflow() const {
  if (IsHorizontalWritingMode())
    return StyleRef().ResolvedIsColumnReverseFlexDirection();
  return StyleRef().IsLeftToRightDirection() ==
         StyleRef().ResolvedIsRowReverseFlexDirection();
}

bool LayoutNGFlexibleBox::HasLeftOverflow() const {
  if (IsHorizontalWritingMode()) {
    return StyleRef().IsLeftToRightDirection() ==
           StyleRef().ResolvedIsRowReverseFlexDirection();
  }
  return (StyleRef().GetWritingMode() == WritingMode::kVerticalLr) ==
         StyleRef().ResolvedIsColumnReverseFlexDirection();
}

void LayoutNGFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  UpdateInFlowBlockLayout();
}

namespace {

void MergeAnonymousFlexItems(LayoutObject* remove_child) {
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  To<LayoutBoxModelObject>(next)->MoveAllChildrenTo(
      To<LayoutBoxModelObject>(prev));
  To<LayoutBlockFlow>(next)->DeleteLineBoxTree();
  next->Destroy();
}

}  // namespace

// See LayoutFlexibleBox::IsChildAllowed().
bool LayoutNGFlexibleBox::IsChildAllowed(LayoutObject* object,
                                         const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  if (UNLIKELY(select && select->UsesMenuList())) {
    // For a size=1 <select>, we only render the active option label through the
    // InnerElement. We do not allow adding layout objects for options and
    // optgroups.
    return object->GetNode() == &select->InnerElement();
  }
  return LayoutNGMixin<LayoutBlock>::IsChildAllowed(object, style);
}

// This is devtools' entry point into layout. This function is intended to have
// no side effects on the NGFragment tree or the LayoutObject tree.
//
// Execution time of this function is not critical, but the flex item layouts
// will hit their caches, so it's probably fast anyway.
DevtoolsFlexInfo LayoutNGFlexibleBox::LayoutForDevtools() {
  DCHECK(!NeedsLayout());
  DevtoolsReadonlyLayoutScope fragments_and_tree_are_now_readonly;
  const NGLayoutResult* old_layout_result = GetCachedLayoutResult();
#if DCHECK_IS_ON()
  MinMaxSizes old_min_max_sizes = IntrinsicLogicalWidths();
  String old_string = old_layout_result->PhysicalFragment().DumpFragmentTree(
      NGPhysicalLineBoxFragment::DumpAll);
#endif

  DCHECK(old_layout_result);
  const NGConstraintSpace& constraint_space =
      old_layout_result->GetConstraintSpaceForCaching();

  NGBlockNode node(this);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(constraint_space, node);
  NGLayoutAlgorithmParams params(node, fragment_geometry, constraint_space);
  DevtoolsFlexInfo flex_info;
  NGFlexLayoutAlgorithm flex_algorithm(params, &flex_info);
  auto new_result = flex_algorithm.Layout();

#if DCHECK_IS_ON()
  MinMaxSizes new_min_max_sizes = IntrinsicLogicalWidths();
  DCHECK_EQ(old_min_max_sizes, new_min_max_sizes)
      << "Legacy min_max_sizes changed!";

  String new_string = new_result->PhysicalFragment().DumpFragmentTree(
      NGPhysicalLineBoxFragment::DumpAll);
  DCHECK_EQ(old_string, new_string) << "Fragment tree changed!";
#endif

  return flex_info;
}

void LayoutNGFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
}

}  // namespace blink
