/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

inline static bool HasDisplayContentsStyle(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  return element && element->HasDisplayContentsStyle();
}

static bool IsLayoutObjectReparented(const LayoutObject* layout_object) {
  return layout_object->IsInTopOrViewTransitionLayer();
}

ContainerNode* LayoutTreeBuilderTraversal::Parent(const Node& node) {
  // TODO(hayato): Uncomment this once we can be sure
  // LayoutTreeBuilderTraversal::parent() is used only for a node which is
  // connected.
  // DCHECK(node.isConnected());
  if (IsA<PseudoElement>(node)) {
    DCHECK(node.parentNode());
    return node.parentNode();
  }
  return FlatTreeTraversal::Parent(node);
}

ContainerNode* LayoutTreeBuilderTraversal::LayoutParent(const Node& node) {
  // TODO(crbug.com/332396355): consider to check for ::scroll-marker-group
  // from all call sites of this function, or move the check here.
  ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(node);

  while (parent && HasDisplayContentsStyle(*parent))
    parent = LayoutTreeBuilderTraversal::Parent(*parent);

  return parent;
}

LayoutObject* LayoutTreeBuilderTraversal::ParentLayoutObject(const Node& node) {
  if (node.GetPseudoId() == kPseudoIdViewTransition) {
    // The view-transition pseudo is wrapped by the anonymous
    // LayoutViewTransitionRoot but that's created by adding the
    // view-transition to the LayoutView.
    return node.GetDocument().GetLayoutView();
  }
  const Node* search_start_node = &node;
  // Parent of ::scroll-marker-group should be layout parent of its
  // originating element.
  if (node.IsScrollMarkerGroupPseudoElement()) {
    search_start_node = To<PseudoElement>(node).OriginatingElement();
  }
  ContainerNode* parent =
      LayoutTreeBuilderTraversal::LayoutParent(*search_start_node);
  return parent ? parent->GetLayoutObject() : nullptr;
}

Node* LayoutTreeBuilderTraversal::NextSibling(const Node& node) {
  PseudoId pseudo_id = node.GetPseudoId();
  Element* parent_element;
  if (pseudo_id != kPseudoIdNone) {
    parent_element = DynamicTo<Element>(*node.parentNode());
    DCHECK(parent_element);
  }
  switch (pseudo_id) {
    case kPseudoIdScrollPrevButton:
      if (Node* next = parent_element->GetPseudoElement(
              kPseudoIdScrollMarkerGroupBefore)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupBefore:
      if (Node* next = parent_element->GetPseudoElement(kPseudoIdMarker)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdMarker:
      if (Node* next =
              parent_element->GetPseudoElement(kPseudoIdScrollMarker)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarker:
      if (Node* next = parent_element->GetPseudoElement(kPseudoIdBefore))
        return next;
      [[fallthrough]];
    case kPseudoIdBefore:
      if (Node* next = FlatTreeTraversal::FirstChild(*parent_element))
        return next;
      [[fallthrough]];
    case kPseudoIdNone:
      if (pseudo_id == kPseudoIdNone) {  // Not falling through
        if (Node* next = FlatTreeTraversal::NextSibling(node))
          return next;
        parent_element = DynamicTo<Element>(FlatTreeTraversal::Parent(node));
        if (!parent_element)
          return nullptr;
      }
      if (Node* next = parent_element->GetPseudoElement(kPseudoIdAfter))
        return next;
      [[fallthrough]];
    case kPseudoIdAfter:
      if (Node* next = parent_element->GetPseudoElement(
              kPseudoIdScrollMarkerGroupAfter)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupAfter:
      if (Node* next =
              parent_element->GetPseudoElement(kPseudoIdScrollNextButton)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollNextButton:
      return nullptr;
    case kPseudoIdViewTransition:
      return nullptr;
    case kPseudoIdViewTransitionGroup: {
      auto* pseudo_element = DynamicTo<PseudoElement>(node);
      DCHECK(pseudo_element);

      // Iterate the list of IDs until we hit the entry for |node's| ID. The
      // sibling is the next ID in the list which generates a pseudo element.
      bool found = false;
      for (const auto& view_transition_name : parent_element->GetDocument()
                                                  .GetStyleEngine()
                                                  .ViewTransitionTags()) {
        if (!found) {
          if (view_transition_name == pseudo_element->view_transition_name())
            found = true;
          continue;
        }

        if (auto* sibling = parent_element->GetPseudoElement(
                kPseudoIdViewTransitionGroup, view_transition_name)) {
          return sibling;
        }
      }
      return nullptr;
    }
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

Node* LayoutTreeBuilderTraversal::PreviousSibling(const Node& node) {
  PseudoId pseudo_id = node.GetPseudoId();
  Element* parent_element;
  if (pseudo_id != kPseudoIdNone) {
    parent_element = DynamicTo<Element>(*node.parentNode());
    DCHECK(parent_element);
  }
  switch (pseudo_id) {
    case kPseudoIdScrollNextButton:
      if (Node* previous = parent_element->GetPseudoElement(
              kPseudoIdScrollMarkerGroupAfter)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupAfter:
      if (Node* previous = parent_element->GetPseudoElement(kPseudoIdAfter)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdAfter:
      if (Node* previous = FlatTreeTraversal::LastChild(*parent_element))
        return previous;
      [[fallthrough]];
    case kPseudoIdNone:
      if (pseudo_id == kPseudoIdNone) {  // Not falling through
        if (Node* previous = FlatTreeTraversal::PreviousSibling(node))
          return previous;
        parent_element = DynamicTo<Element>(FlatTreeTraversal::Parent(node));
        if (!parent_element)
          return nullptr;
      }
      if (Node* previous = parent_element->GetPseudoElement(kPseudoIdBefore))
        return previous;
      [[fallthrough]];
    case kPseudoIdBefore:
      if (Node* previous =
              parent_element->GetPseudoElement(kPseudoIdScrollMarker)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarker:
      if (Node* previous = parent_element->GetPseudoElement(kPseudoIdMarker))
        return previous;
      [[fallthrough]];
    case kPseudoIdMarker:
      if (Node* previous = parent_element->GetPseudoElement(
              kPseudoIdScrollMarkerGroupBefore)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupBefore:
      if (Node* previous =
              parent_element->GetPseudoElement(kPseudoIdScrollPrevButton)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollPrevButton:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

Node* LayoutTreeBuilderTraversal::LastChild(const Node& node) {
  const auto* current_element = DynamicTo<Element>(node);
  if (!current_element)
    return FlatTreeTraversal::LastChild(node);

  if (Node* last =
          current_element->GetPseudoElement(kPseudoIdScrollNextButton)) {
    return last;
  }
  if (Node* last =
          current_element->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
    return last;
  }
  if (Node* last = current_element->GetPseudoElement(kPseudoIdAfter))
    return last;
  if (Node* last = FlatTreeTraversal::LastChild(*current_element))
    return last;
  if (Node* last = current_element->GetPseudoElement(kPseudoIdBefore))
    return last;
  if (Node* last = current_element->GetPseudoElement(kPseudoIdScrollMarker)) {
    return last;
  }
  if (Node* last = current_element->GetPseudoElement(kPseudoIdMarker)) {
    return last;
  }
  if (Node* last =
          current_element->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
    return last;
  }
  return current_element->GetPseudoElement(kPseudoIdScrollPrevButton);
}

Node* LayoutTreeBuilderTraversal::Previous(const Node& node,
                                           const Node* stay_within) {
  if (node == stay_within)
    return nullptr;

  if (Node* previous_node = PreviousSibling(node)) {
    while (Node* previous_last_child = LastChild(*previous_node))
      previous_node = previous_last_child;
    return previous_node;
  }
  return Parent(node);
}

Node* LayoutTreeBuilderTraversal::FirstChild(const Node& node) {
  const auto* current_element = DynamicTo<Element>(node);
  if (!current_element)
    return FlatTreeTraversal::FirstChild(node);

  if (Node* first =
          current_element->GetPseudoElement(kPseudoIdScrollPrevButton)) {
    return first;
  }
  if (Node* first =
          current_element->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
    return first;
  }
  if (Node* first = current_element->GetPseudoElement(kPseudoIdMarker))
    return first;
  if (Node* first = current_element->GetPseudoElement(kPseudoIdScrollMarker)) {
    return first;
  }
  if (Node* first = current_element->GetPseudoElement(kPseudoIdBefore))
    return first;
  if (Node* first = FlatTreeTraversal::FirstChild(node))
    return first;
  if (Node* first = current_element->GetPseudoElement(kPseudoIdAfter)) {
    return first;
  }
  if (Node* first =
          current_element->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
    return first;
  }
  return current_element->GetPseudoElement(kPseudoIdScrollNextButton);
}

static Node* NextAncestorSibling(const Node& node, const Node* stay_within) {
  DCHECK(!LayoutTreeBuilderTraversal::NextSibling(node));
  DCHECK_NE(node, stay_within);
  for (Node* parent_node = LayoutTreeBuilderTraversal::Parent(node);
       parent_node;
       parent_node = LayoutTreeBuilderTraversal::Parent(*parent_node)) {
    if (parent_node == stay_within)
      return nullptr;
    if (Node* next_node = LayoutTreeBuilderTraversal::NextSibling(*parent_node))
      return next_node;
  }
  return nullptr;
}

Node* LayoutTreeBuilderTraversal::NextSkippingChildren(
    const Node& node,
    const Node* stay_within) {
  if (node == stay_within)
    return nullptr;
  if (Node* next_node = NextSibling(node))
    return next_node;
  return NextAncestorSibling(node, stay_within);
}

Node* LayoutTreeBuilderTraversal::Next(const Node& node,
                                       const Node* stay_within) {
  if (Node* child = FirstChild(node))
    return child;
  return NextSkippingChildren(node, stay_within);
}

// Checks if current or (next/prev) sibling is either ::scroll-marker-group
// or element with scroll-marker-group property set.
static inline bool AreBoxTreeOrderSiblings(const Node& current, Node* sibling) {
  if (current.IsScrollMarkerGroupPseudoElement()) {
    return false;
  }
  const ComputedStyle* style = current.GetComputedStyle();
  if (style && !style->ScrollMarkerGroupNone()) {
    return false;
  }
  if (!sibling) {
    return true;
  }
  if (sibling->IsScrollMarkerGroupPseudoElement()) {
    return false;
  }
  const ComputedStyle* sibling_style = sibling->GetComputedStyle();
  if (sibling_style && !sibling_style->ScrollMarkerGroupNone()) {
    return false;
  }
  return true;
}

// This function correctly performs one move from `node` to next
// layout sibling. We can't just use NextSibling, as ::scroll-marker-group
// layout object is either previous or next sibling of its originating element,
// but still a node child of it, as a pseudo element.
// Layout tree:
//        (PS) (SMGB) (OE) (SMGA) (NS)
//                  (B)  (A)
// OE - originating element
// PS - previous sibling of OE
// NS - next sibling of OE
// SMGB - ::scroll-marker-group of OE with scroll-marker-group: before
// SMGA - ::scroll-marker-group of OE with scroll-marker-group: after
// B - ::before of OE
// A - ::after of OE
// Node tree:
//        (PS) (OE) (NS)
//    (SMGB) (B)  (A) (SMGA)
// Node tree is input (`node`), return output based on layout tree.
static Node* NextLayoutSiblingInBoxTreeOrder(const Node& node) {
  Node* next = LayoutTreeBuilderTraversal::NextSibling(node);
  if (AreBoxTreeOrderSiblings(node, next)) {
    return next;
  }
  // From PS to OE with SMGB, return SMGB.
  if (next && next->GetComputedStyle() &&
      next->GetComputedStyle()->HasScrollMarkerGroupBefore()) {
    if (Element* pseudo = To<Element>(next)->GetPseudoElement(
            kPseudoIdScrollMarkerGroupBefore)) {
      return pseudo;
    }
  }
  // From some pseudo to any SMG, just skip SMG.
  if (next && next->IsScrollMarkerGroupPseudoElement()) {
    return LayoutTreeBuilderTraversal::NextSibling(*next);
  }
  // From OE with SMGA to NS, return SMGA.
  if (node.GetComputedStyle() &&
      node.GetComputedStyle()->HasScrollMarkerGroupAfter()) {
    if (Element* pseudo = To<Element>(node).GetPseudoElement(
            kPseudoIdScrollMarkerGroupAfter)) {
      return pseudo;
    }
  }
  // From SMGB, return OE.
  if (node.IsScrollMarkerGroupBeforePseudoElement()) {
    return To<PseudoElement>(node).OriginatingElement();
  }
  // From SMGA, return NS, but check if NS has SMGB, then return NS's SMGB.
  if (node.IsScrollMarkerGroupAfterPseudoElement()) {
    Node* originating_next = LayoutTreeBuilderTraversal::NextSibling(
        *To<PseudoElement>(node).OriginatingElement());
    if (originating_next && originating_next->GetComputedStyle() &&
        originating_next->GetComputedStyle()->HasScrollMarkerGroupBefore()) {
      if (Element* pseudo =
              To<Element>(originating_next)
                  ->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
        return pseudo;
      }
    }
    return originating_next;
  }
  return next;
}

static Node* NextLayoutSiblingInternal(Node* node, int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = NextLayoutSiblingInBoxTreeOrder(*sibling)) {
    if (!HasDisplayContentsStyle(*sibling))
      return sibling;

    if (Node* inner = NextLayoutSiblingInternal(
            LayoutTreeBuilderTraversal::FirstChild(*sibling), limit))
      return inner;

    if (limit == -1)
      return nullptr;
  }

  return nullptr;
}

Node* LayoutTreeBuilderTraversal::NextLayoutSibling(const Node& node,
                                                    int32_t& limit) {
  DCHECK_NE(limit, -1);
  if (Node* sibling = NextLayoutSiblingInternal(
          NextLayoutSiblingInBoxTreeOrder(node), limit)) {
    return sibling;
  }

  Node* parent = LayoutTreeBuilderTraversal::Parent(node);
  while (limit != -1 && parent && HasDisplayContentsStyle(*parent)) {
    if (Node* sibling = NextLayoutSiblingInternal(
            NextLayoutSiblingInBoxTreeOrder(*parent), limit)) {
      return sibling;
    }
    parent = LayoutTreeBuilderTraversal::Parent(*parent);
  }

  return nullptr;
}

// See comments in NextLayoutSiblingInBoxTreeOrder.
static Node* PreviousLayoutSiblingInBoxTreeOrder(const Node& node) {
  Node* previous = LayoutTreeBuilderTraversal::PreviousSibling(node);
  if (AreBoxTreeOrderSiblings(node, previous)) {
    return previous;
  }
  if (previous && previous->GetComputedStyle() &&
      previous->GetComputedStyle()->HasScrollMarkerGroupAfter()) {
    if (Element* pseudo = To<Element>(previous)->GetPseudoElement(
            kPseudoIdScrollMarkerGroupAfter)) {
      return pseudo;
    }
  }
  if (previous && previous->IsScrollMarkerGroupPseudoElement()) {
    return LayoutTreeBuilderTraversal::PreviousSibling(*previous);
  }
  if (node.GetComputedStyle() &&
      node.GetComputedStyle()->HasScrollMarkerGroupBefore()) {
    if (Element* pseudo = To<Element>(node).GetPseudoElement(
            kPseudoIdScrollMarkerGroupBefore)) {
      return pseudo;
    }
  }
  if (node.IsScrollMarkerGroupAfterPseudoElement()) {
    return To<PseudoElement>(node).OriginatingElement();
  }
  if (node.IsScrollMarkerGroupBeforePseudoElement()) {
    Node* originating_prev = LayoutTreeBuilderTraversal::PreviousSibling(
        *To<PseudoElement>(node).OriginatingElement());
    if (originating_prev && originating_prev->GetComputedStyle() &&
        originating_prev->GetComputedStyle()->HasScrollMarkerGroupAfter()) {
      if (Element* pseudo =
              To<Element>(originating_prev)
                  ->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
        return pseudo;
      }
    }
    return originating_prev;
  }
  return previous;
}

static Node* PreviousLayoutSiblingInternal(Node* node, int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = PreviousLayoutSiblingInBoxTreeOrder(*sibling)) {
    if (!HasDisplayContentsStyle(*sibling))
      return sibling;

    if (Node* inner = PreviousLayoutSiblingInternal(
            LayoutTreeBuilderTraversal::LastChild(*sibling), limit))
      return inner;

    if (limit == -1)
      return nullptr;
  }

  return nullptr;
}

Node* LayoutTreeBuilderTraversal::PreviousLayoutSibling(const Node& node,
                                                        int32_t& limit) {
  DCHECK_NE(limit, -1);
  if (Node* sibling = PreviousLayoutSiblingInternal(
          PreviousLayoutSiblingInBoxTreeOrder(node), limit)) {
    return sibling;
  }

  Node* parent = LayoutTreeBuilderTraversal::Parent(node);
  while (limit != -1 && parent && HasDisplayContentsStyle(*parent)) {
    if (Node* sibling = PreviousLayoutSiblingInternal(
            PreviousLayoutSiblingInBoxTreeOrder(*parent), limit)) {
      return sibling;
    }
    parent = LayoutTreeBuilderTraversal::Parent(*parent);
  }

  return nullptr;
}

Node* LayoutTreeBuilderTraversal::FirstLayoutChild(const Node& node) {
  int32_t limit = kTraverseAllSiblings;
  return NextLayoutSiblingInternal(FirstChild(node), limit);
}

LayoutObject* LayoutTreeBuilderTraversal::NextSiblingLayoutObject(
    const Node& node,
    int32_t limit) {
  DCHECK(limit == kTraverseAllSiblings || limit >= 0) << limit;
  for (Node* sibling = NextLayoutSibling(node, limit); sibling && limit != -1;
       sibling = NextLayoutSibling(*sibling, limit)) {
    LayoutObject* layout_object = sibling->GetLayoutObject();
    if (layout_object && !IsLayoutObjectReparented(layout_object))
      return layout_object;
  }
  return nullptr;
}

LayoutObject* LayoutTreeBuilderTraversal::PreviousSiblingLayoutObject(
    const Node& node,
    int32_t limit) {
  DCHECK(limit == kTraverseAllSiblings || limit >= 0) << limit;
  for (Node* sibling = PreviousLayoutSibling(node, limit);
       sibling && limit != -1;
       sibling = PreviousLayoutSibling(*sibling, limit)) {
    LayoutObject* layout_object = sibling->GetLayoutObject();
    if (layout_object && !IsLayoutObjectReparented(layout_object))
      return layout_object;
  }
  return nullptr;
}

LayoutObject* LayoutTreeBuilderTraversal::NextInTopLayer(
    const Element& element) {
  CHECK(element.ComputedStyleRef().IsRenderedInTopLayer(element))
      << "This method should only be called with an element that is rendered in"
         " the top layer";
  const HeapVector<Member<Element>>& top_layer_elements =
      element.GetDocument().TopLayerElements();
  wtf_size_t position = top_layer_elements.Find(&element);
  DCHECK_NE(position, kNotFound);
  for (wtf_size_t i = position + 1; i < top_layer_elements.size(); ++i) {
    LayoutObject* layout_object = top_layer_elements[i]->GetLayoutObject();
    // If top_layer_elements[i] is not a LayoutView child, its LayoutObject is
    // not re-attached and not in the top layer yet, thus we can not use it as a
    // sibling LayoutObject.
    if (layout_object &&
        layout_object->StyleRef().IsRenderedInTopLayer(
            *top_layer_elements[i]) &&
        IsA<LayoutView>(layout_object->Parent())) {
      return layout_object;
    }
  }
  return nullptr;
}

int LayoutTreeBuilderTraversal::ComparePreorderTreePosition(const Node& node1,
                                                            const Node& node2) {
  if (node1 == node2) {
    return 0;
  }
  const Node* anc1 = &node1;
  const Node* anc2 = &node2;
  if (Parent(*anc1) != Parent(*anc2)) {
    wtf_size_t depth1 = 0u;
    for (; anc1; anc1 = Parent(*anc1)) {
      if (anc1 == anc2) {
        // if node2 is ancestor of node1, return 1.
        return 1;
      }
      ++depth1;
    }
    wtf_size_t depth2 = 0u;
    for (; anc2; anc2 = Parent(*anc2)) {
      if (anc2 == anc1) {
        // if node1 is ancestor of node2, return -1.
        return -1;
      }
      ++depth2;
    }
    // Find LCA.
    anc1 = &node1;
    anc2 = &node2;
    while (depth1 < depth2) {
      anc2 = Parent(*anc2);
      --depth2;
    }
    while (depth1 > depth2) {
      anc1 = Parent(*anc1);
      --depth1;
    }
    while (anc1 && anc2) {
      const Node* parent1 = Parent(*anc1);
      const Node* parent2 = Parent(*anc2);
      if (parent1 == parent2) {
        break;
      }
      anc1 = parent1;
      anc2 = parent2;
    }
  }
  // Do some quick checks.
  const Node* parent = Parent(*anc1);
  DCHECK(parent);
  if (NextSibling(*anc2) == anc1 || FirstChild(*parent) == anc2) {
    return 1;
  }
  if (FirstChild(*parent) == anc1 || LastChild(*parent) == anc2) {
    return -1;
  }
  // Compare the children of the first common ancestor and the current top-most
  // ancestors of the nodes.
  // Note: starting with anc1 here, as in most use cases of this function we
  // want to compare two elements that are close to each other with anc1 usually
  // being previously in pre-order.
  DCHECK(anc1 && anc2);
  for (const Node* child = anc1; child; child = NextSibling(*child)) {
    if (child == anc2) {
      return -1;
    }
  }
  return 1;
}

}  // namespace blink
