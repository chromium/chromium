// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/tree_traversal_utils.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

const PhysicalBoxFragment* GetColumnFragment(const ColumnPseudoElement& column,
                                             wtf_size_t index) {
  const LayoutBox* multicol =
      column.UltimateOriginatingElement().GetLayoutBox()->ContentLayoutBox();
  if (!multicol) {
    return nullptr;
  }
  // Fragmented multicol containers are not allowed to ride the carousel, so
  // just pick the first fragment.
  const PhysicalBoxFragment* multicol_fragment =
      multicol->GetPhysicalFragment(0);

  wtf_size_t columns_to_skip = index;
  for (const PhysicalFragmentLink& child : multicol_fragment->Children()) {
    if (!child->IsColumnBox()) {
      continue;
    }
    if (columns_to_skip) {
      columns_to_skip--;
      continue;
    }
    return To<PhysicalBoxFragment>(child.get());
  }
  return nullptr;
}

}  // namespace

ColumnPseudoElement::ColumnPseudoElement(Element* originating_element,
                                         wtf_size_t index)
    : IndexedPseudoElement(originating_element, kPseudoIdColumn, index) {
  UseCounter::Count(GetDocument(), WebFeature::kColumnPseudoElement);
}

Element* ColumnPseudoElement::FirstChildInDOMOrder() const {
  // Columns are created by layout, and the order of the fragments inside may
  // not match DOM order (e.g. out-of-flow positioning, reversed flex items, and
  // so on). Look for any nodes that start in this column, and get them sorted
  // in DOM order.
  class Listener : public PhysicalFragmentTraversalListener {
    STACK_ALLOCATED();

   public:
    Element* GetEarliestElement() const { return earliest_element_; }

   private:
    NextStep HandleEntry(const PhysicalBoxFragment& descendant,
                         PhysicalOffset,
                         bool is_first_for_node) final {
      // We're only interested in nodes that start in this column. Any node
      // that's resumed from a previous column will seen in its start column.
      if (is_first_for_node) {
        if (auto* element = DynamicTo<Element>(descendant.GetNode())) {
          SetElementIfEarliest(element);
          // No need to descend into this fragment. Children cannot precede this
          // element.
          return kSkipChildren;
        }
      }
      return kContinue;
    }

    void HandleCulledInline(const LayoutInline& culled_inline,
                            bool is_first_for_node) final {
      if (is_first_for_node) {
        if (auto* element = DynamicTo<Element>(culled_inline.GetNode())) {
          // TODO(crbug.com/406288653): Note that we wouldn't have to look for
          // culled inlines, if we instead got all focusable inlines to create
          // fragments. That would cause some problems for LinkHighlightImpl,
          // though, with the root cause being either there, or somewhere inside
          // the outline code.
          SetElementIfEarliest(element);
        }
      }
    }

    void SetElementIfEarliest(Element* element) {
      if (!earliest_element_) {
        earliest_element_ = element;
        return;
      }
      uint16_t position = element->compareDocumentPosition(
          earliest_element_, kTreatShadowTreesAsComposed);
      if (position & kDocumentPositionFollowing) {
        earliest_element_ = element;
      }
    }

    Element* earliest_element_ = nullptr;
  };

  const PhysicalBoxFragment* column_fragment =
      GetColumnFragment(*this, Index());
  if (!column_fragment) {
    return nullptr;
  }
  Listener listener;
  ForAllBoxFragmentDescendants(*column_fragment,
                               kFragmentTraversalOptionCulledInlines, listener);
  return listener.GetEarliestElement();
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

void ColumnPseudoElement::SetIsInsideInactiveColumnTabForDescendants(
    bool is_inactive) const {
  class Listener : public PhysicalFragmentTraversalListener {
    STACK_ALLOCATED();

   public:
    explicit Listener(bool is_inactive) : is_inactive_(is_inactive) {}

   private:
    NextStep HandleEntry(const PhysicalBoxFragment& descendant,
                         PhysicalOffset,
                         bool is_first_for_node) final {
      // Process all fragments for fragmented nodes. A node should be
      // considered active as long as any of its fragments is in the active
      // column.
      if (LayoutObject* layout_object = descendant.GetMutableLayoutObject()) {
        layout_object->SetInsideInactiveColumnTab(is_inactive_);
      }
      return kContinue;
    }

    void HandleCulledInline(const LayoutInline& culled_inline,
                            bool is_first_for_node) final {
      // Culled inlines don't have fragments but still have LayoutObjects.
      const_cast<LayoutInline&>(culled_inline)
          .SetInsideInactiveColumnTab(is_inactive_);
    }

    bool is_inactive_;
  };

  const PhysicalBoxFragment* column_fragment =
      GetColumnFragment(*this, Index());
  if (!column_fragment) {
    return;
  }
  Listener listener(is_inactive);
  ForAllBoxFragmentDescendants(*column_fragment,
                               kFragmentTraversalOptionCulledInlines, listener);
}

}  // namespace blink
