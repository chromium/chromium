// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/slot_scoped_traversal.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

namespace {
Element* NextSkippingChildrenOfShadowHost(const Element& start,
                                          const Element& scope) {
  DCHECK(scope.AssignedSlot());
  if (!start.AuthorShadowRoot()) {
    if (Element* first = ElementTraversal::FirstChild(start))
      return first;
  }

  for (const Element* current = &start; current != scope;
       current = current->parentElement()) {
    if (Element* next_sibling = ElementTraversal::NextSibling(*current))
      return next_sibling;
  }
  return nullptr;
}

Element* LastWithinOrSelfSkippingChildrenOfShadowHost(const Element& scope) {
  Element* current = const_cast<Element*>(&scope);
  while (!current->AuthorShadowRoot()) {
    Element* last_child = ElementTraversal::LastChild(*current);
    if (!last_child)
      break;
    current = last_child;
  }
  return current;
}

Element* PreviousSkippingChildrenOfShadowHost(const Element& start,
                                              const Element& scope) {
  DCHECK(scope.AssignedSlot());
  DCHECK_NE(start, &scope);
  if (Element* previous_sibling = ElementTraversal::PreviousSibling(start))
    return LastWithinOrSelfSkippingChildrenOfShadowHost(*previous_sibling);
  return start.parentElement();
}
}  // namespace

HTMLSlotElement* SlotScopedTraversal::FindScopeOwnerSlot(
    const Element& current) {
  if (Element* nearest_inclusive_ancestor_assigned_to_slot =
          SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(current))
    return nearest_inclusive_ancestor_assigned_to_slot->AssignedSlot();
  return nullptr;
}

Element* SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
    const Element& current) {
  Element* element = const_cast<Element*>(&current);
  for (; element; element = element->parentElement()) {
    if (element->AssignedSlot())
      break;
  }
  return element;
}

Element* SlotScopedTraversal::Next(const Element& current) {
  Element* nearest_inclusive_ancestor_assigned_to_slot =
      SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(current);
  DCHECK(nearest_inclusive_ancestor_assigned_to_slot);
  // Search within children of an element which is assigned to a slot.
  if (Element* next = NextSkippingChildrenOfShadowHost(
          current, *nearest_inclusive_ancestor_assigned_to_slot))
    return next;

  // Seek to the next element assigned to the same slot.
  HTMLSlotElement* slot =
      nearest_inclusive_ancestor_assigned_to_slot->AssignedSlot();
  DCHECK(slot);
  const HeapVector<Member<Node>>& assigned_nodes = slot->AssignedNodes();
  wtf_size_t current_index =
      assigned_nodes.Find(*nearest_inclusive_ancestor_assigned_to_slot);
  DCHECK_NE(current_index, kNotFound);
  for (++current_index; current_index < assigned_nodes.size();
       ++current_index) {
    if (auto* element = DynamicTo<Element>(assigned_nodes[current_index].Get()))
      return element;
  }
  return nullptr;
}

Element* SlotScopedTraversal::Previous(const Element& current) {
  Element* nearest_inclusive_ancestor_assigned_to_slot =
      SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(current);
  DCHECK(nearest_inclusive_ancestor_assigned_to_slot);

  if (current != nearest_inclusive_ancestor_assigned_to_slot) {
    // Search within children of an element which is assigned to a slot.
    Element* previous = PreviousSkippingChildrenOfShadowHost(
        current, *nearest_inclusive_ancestor_assigned_to_slot);
    DCHECK(previous);
    return previous;
  }

  // Seek to the previous element assigned to the same slot.
  const HeapVector<Member<Node>>& assigned_nodes =
      nearest_inclusive_ancestor_assigned_to_slot->AssignedSlot()
          ->AssignedNodes();
  wtf_size_t current_index =
      assigned_nodes.ReverseFind(*nearest_inclusive_ancestor_assigned_to_slot);
  DCHECK_NE(current_index, kNotFound);
  for (; current_index > 0; --current_index) {
    const Member<Node> assigned_node = assigned_nodes[current_index - 1];
    auto* element = DynamicTo<Element>(assigned_node.Get());
    if (!element)
      continue;
    return LastWithinOrSelfSkippingChildrenOfShadowHost(*element);
  }
  return nullptr;
}

Element* SlotScopedTraversal::FirstAssignedToSlot(HTMLSlotElement& slot) {
  const HeapVector<Member<Node>>& assigned_nodes = slot.AssignedNodes();
  for (auto assigned_node : assigned_nodes) {
    if (auto* element = DynamicTo<Element>(assigned_node.Get()))
      return element;
  }
  return nullptr;
}

Element* SlotScopedTraversal::LastAssignedToSlot(HTMLSlotElement& slot) {
  const HeapVector<Member<Node>>& assigned_nodes = slot.AssignedNodes();
  for (auto assigned_node = assigned_nodes.rbegin();
       assigned_node != assigned_nodes.rend(); ++assigned_node) {
    auto* element = DynamicTo<Element>(assigned_node->Get());
    if (!element)
      continue;
    return LastWithinOrSelfSkippingChildrenOfShadowHost(*element);
  }
  return nullptr;
}

bool SlotScopedTraversal::IsSlotScoped(const Element& current) {
  return SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(current);
}

}  // namespace blink
