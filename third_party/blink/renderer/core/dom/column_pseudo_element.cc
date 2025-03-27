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
  TreeOrderedList<Element> sorted_elements;

  const LayoutBox* multicol =
      UltimateOriginatingElement().GetLayoutBox()->ContentLayoutBox();
  if (!multicol) {
    return nullptr;
  }
  // Fragmented multicol containers are not allowed to ride the carousel, so
  // just pick the first fragment.
  const PhysicalBoxFragment& multicol_fragment =
      *multicol->GetPhysicalFragment(0);

  wtf_size_t columns_to_skip = index_;
  for (const PhysicalFragmentLink& child : multicol_fragment.Children()) {
    if (!child->IsColumnBox()) {
      continue;
    }
    if (columns_to_skip) {
      columns_to_skip--;
      continue;
    }
    const auto& column = *To<PhysicalBoxFragment>(child.get());
    ForAllBoxFragmentDescendants(
        column,
        [&](const PhysicalBoxFragment* descendant,
            const LayoutInline* culled_inline,
            bool is_first_for_node) -> FragmentTraversalNextStep {
          // One, and only one, should be set.
          DCHECK(!descendant != !culled_inline);

          // We're only interested in nodes that start in this column. Any node
          // that's resumed from a previous column will seen in its start
          // column.
          if (is_first_for_node) {
            if (descendant) {
              if (auto* element = DynamicTo<Element>(descendant->GetNode())) {
                sorted_elements.Add(element);
                // No need to descend into this fragment. Children cannot
                // precede this element.
                return FragmentTraversalNextStep::kSkipChildren;
              }
            } else if (auto* element =
                           DynamicTo<Element>(culled_inline->GetNode())) {
              // TODO(crbug.com/406288653): Note that we wouldn't have to look
              // for culled inlines, if we instead got all focusable inlines to
              // create fragments. That would cause some problems for
              // LinkHighlightImpl, though, with the root cause being either
              // there, or somewhere inside the outline code.
              sorted_elements.Add(element);
            }
          }
          return FragmentTraversalNextStep::kContinue;
        });
    if (!sorted_elements.IsEmpty()) {
      return *sorted_elements.begin();
    }
  }
  return nullptr;
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
