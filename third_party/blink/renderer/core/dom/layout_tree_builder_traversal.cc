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
  return layout_object->IsInTopLayer();
}

ContainerNode* LayoutTreeBuilderTraversal::Parent(const Node& node) {
  // TODO(hayato): Uncomment this once we can be sure
  // LayoutTreeBuilderTraversal::parent() is used only for a node which is
  // connected.
  // DCHECK(node.isConnected());
  if (auto* element = DynamicTo<PseudoElement>(node)) {
    DCHECK(node.parentNode());
    return node.parentNode();
  }
  return FlatTreeTraversal::Parent(node);
}

ContainerNode* LayoutTreeBuilderTraversal::LayoutParent(const Node& node) {
  ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(node);

  while (parent && HasDisplayContentsStyle(*parent))
    parent = LayoutTreeBuilderTraversal::Parent(*parent);

  return parent;
}

LayoutObject* LayoutTreeBuilderTraversal::ParentLayoutObject(const Node& node) {
  ContainerNode* parent = LayoutTreeBuilderTraversal::LayoutParent(node);
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
    case kPseudoIdMarker:
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
      NOTREACHED();
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
      if (Node* previous = parent_element->GetPseudoElement(kPseudoIdMarker))
        return previous;
      [[fallthrough]];
    case kPseudoIdMarker:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

Node* LayoutTreeBuilderTraversal::LastChild(const Node& node) {
  const auto* current_element = DynamicTo<Element>(node);
  if (!current_element)
    return FlatTreeTraversal::LastChild(node);

  if (Node* last = current_element->GetPseudoElement(kPseudoIdAfter))
    return last;
  if (Node* last = FlatTreeTraversal::LastChild(*current_element))
    return last;
  if (Node* last = current_element->GetPseudoElement(kPseudoIdBefore))
    return last;
  return current_element->GetPseudoElement(kPseudoIdMarker);
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

  if (Node* first = current_element->GetPseudoElement(kPseudoIdMarker))
    return first;
  if (Node* first = current_element->GetPseudoElement(kPseudoIdBefore))
    return first;
  if (Node* first = FlatTreeTraversal::FirstChild(node))
    return first;
  return current_element->GetPseudoElement(kPseudoIdAfter);
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

static Node* NextLayoutSiblingInternal(Node* node, int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = LayoutTreeBuilderTraversal::NextSibling(*sibling)) {
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
  if (Node* sibling = NextLayoutSiblingInternal(NextSibling(node), limit))
    return sibling;

  Node* parent = LayoutTreeBuilderTraversal::Parent(node);
  while (limit != -1 && parent && HasDisplayContentsStyle(*parent)) {
    if (Node* sibling = NextLayoutSiblingInternal(NextSibling(*parent), limit))
      return sibling;
    parent = LayoutTreeBuilderTraversal::Parent(*parent);
  }

  return nullptr;
}

static Node* PreviousLayoutSiblingInternal(Node* node, int32_t& limit) {
  for (Node* sibling = node; sibling && limit-- != 0;
       sibling = LayoutTreeBuilderTraversal::PreviousSibling(*sibling)) {
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
  if (Node* sibling =
          PreviousLayoutSiblingInternal(PreviousSibling(node), limit))
    return sibling;

  Node* parent = LayoutTreeBuilderTraversal::Parent(node);
  while (limit != -1 && parent && HasDisplayContentsStyle(*parent)) {
    if (Node* sibling =
            PreviousLayoutSiblingInternal(PreviousSibling(*parent), limit))
      return sibling;
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
  DCHECK(element.ComputedStyleRef().IsInTopLayer(element))
      << "This method should only be called with an element in the top layer "
         "candidate list which is rendered in the top layer";
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
        layout_object->StyleRef().TopLayer() == ETopLayer::kBrowser &&
        IsA<LayoutView>(layout_object->Parent())) {
      return layout_object;
    }
  }
  return nullptr;
}

}  // namespace blink
