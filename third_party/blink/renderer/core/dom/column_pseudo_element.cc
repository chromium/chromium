// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"

#include "third_party/blink/renderer/core/dom/tree_ordered_list.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/tree_traversal_utils.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

const PhysicalBoxFragment* GetColumnFragment(
    const ColumnPseudoElement& column_pseudo,
    wtf_size_t index) {
  const Element& originating_elm = column_pseudo.UltimateOriginatingElement();
  const LayoutBox* multicol =
      originating_elm.GetLayoutBox()->ContentLayoutBox();
  if (!multicol) {
    return nullptr;
  }
  // Fragmented multicol containers are not allowed to ride the carousel, so
  // just pick the first fragment.
  const PhysicalBoxFragment& fragment = *multicol->GetPhysicalFragment(0);
  wtf_size_t columns_to_skip = index;
  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (child->IsColumnBox() && !columns_to_skip--) {
      return To<PhysicalBoxFragment>(child.get());
    }
  }
  NOTREACHED();
}

}  // anonymous namespace

ColumnPseudoElement::ColumnPseudoElement(Element* originating_element,
                                         wtf_size_t index)
    : PseudoElement(originating_element, kPseudoIdColumn), index_(index) {
  UseCounter::Count(GetDocument(), WebFeature::kColumnPseudoElement);
}

Element* ColumnPseudoElement::FirstChildInDOMOrder() const {
  // Columns are created by layout, and the order of the fragments inside may
  // not match DOM order (e.g. out-of-flow positioning, reversed flex items, and
  // so on). Look for any nodes that start in this column, and get them sorted
  // in DOM order.
  const PhysicalBoxFragment* column = GetColumnFragment(*this, index_);
  if (!column) {
    return nullptr;
  }
  TreeOrderedList<Element> sorted_elements;
  ForAllBoxFragmentDescendants(
      *column,
      [&](const PhysicalBoxFragment& descendant) -> FragmentTraversalNextStep {
        // We're only interested in nodes that start in this column. Any node
        // that's resumed from a previous column will seen in its start column.
        if (descendant.IsFirstForNode()) {
          if (auto* element = DynamicTo<Element>(descendant.GetNode())) {
            sorted_elements.Add(element);
            // No need to descend into this fragment. Children cannot precede
            // this element.
            return FragmentTraversalNextStep::kSkipChildren;
          }
        }
        return FragmentTraversalNextStep::kContinue;
      });
  if (sorted_elements.IsEmpty()) {
    return nullptr;
  }
  return *sorted_elements.begin();
}

void ColumnPseudoElement::AttachLayoutTree(AttachContext& context) {
  // A ::column element can not have a box, but it may have a ::scroll-marker
  // child.
  AttachPseudoElement(kPseudoIdScrollMarker, context);
  ContainerNode::AttachLayoutTree(context);
}

void ColumnPseudoElement::DetachLayoutTree(bool performing_reattach) {
  DetachPseudoElement(kPseudoIdScrollMarker, performing_reattach);
  ContainerNode::DetachLayoutTree(performing_reattach);
}

}  // namespace blink
