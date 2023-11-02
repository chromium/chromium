// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/select_menu_part_traversal.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

namespace blink {

Node* SelectMenuPartTraversal::NextSibling(const Node& node) {
  Node* next = FlatTreeTraversal::NextSibling(node);
  while (next && SelectMenuPartTraversal::IsNestedSelectMenu(*next)) {
    next = FlatTreeTraversal::NextSibling(*next);
  }
  return next;
}

Node* SelectMenuPartTraversal::PreviousSibling(const Node& node) {
  Node* next = FlatTreeTraversal::PreviousSibling(node);
  while (next && SelectMenuPartTraversal::IsNestedSelectMenu(*next)) {
    next = FlatTreeTraversal::PreviousSibling(*next);
  }
  return next;
}

Node* SelectMenuPartTraversal::FirstChild(const Node& node) {
  Node* first = FlatTreeTraversal::FirstChild(node);
  while (first && SelectMenuPartTraversal::IsNestedSelectMenu(*first)) {
    first = SelectMenuPartTraversal::NextSibling(*first);
  }
  return first;
}

Node* SelectMenuPartTraversal::LastChild(const Node& node) {
  Node* first = FlatTreeTraversal::LastChild(node);
  while (first && SelectMenuPartTraversal::IsNestedSelectMenu(*first)) {
    first = SelectMenuPartTraversal::PreviousSibling(*first);
  }
  return first;
}

namespace {

static Node* NextAncestorSibling(const Node& node, const Node* stay_within) {
  DCHECK(!SelectMenuPartTraversal::NextSibling(node));
  DCHECK_NE(node, stay_within);
  for (Node* parent_node = FlatTreeTraversal::Parent(node); parent_node;
       parent_node = FlatTreeTraversal::Parent(*parent_node)) {
    if (parent_node == stay_within)
      return nullptr;
    if (Node* next_node = SelectMenuPartTraversal::NextSibling(*parent_node))
      return next_node;
  }
  return nullptr;
}

}  // namespace

Node* SelectMenuPartTraversal::NextSkippingChildren(const Node& node,
                                                    const Node* stay_within) {
  if (node == stay_within)
    return nullptr;
  if (Node* next_node = NextSibling(node))
    return next_node;
  return NextAncestorSibling(node, stay_within);
}

Node* SelectMenuPartTraversal::Next(const Node& node, const Node* stay_within) {
  if (Node* child = FirstChild(node))
    return child;
  return NextSkippingChildren(node, stay_within);
}

Node* SelectMenuPartTraversal::Previous(const Node& node,
                                        const Node* stay_within) {
  if (Node* previous = PreviousSibling(node)) {
    while (Node* child = LastChild(*previous))
      previous = child;
    return previous;
  }
  Node* parent = FlatTreeTraversal::Parent(node);
  return parent != stay_within ? parent : nullptr;
}

bool SelectMenuPartTraversal::IsDescendantOf(const Node& node,
                                             const Node& other) {
  for (const Node* ancestor = FlatTreeTraversal::Parent(node); ancestor;
       ancestor = FlatTreeTraversal::Parent(*ancestor)) {
    if (ancestor == other)
      return true;
    if (IsNestedSelectMenu(*ancestor))
      return false;
  }
  return false;
}

HTMLSelectMenuElement* SelectMenuPartTraversal::NearestSelectMenuAncestor(
    const Node& node) {
  for (Node* ancestor = FlatTreeTraversal::Parent(node); ancestor;
       ancestor = FlatTreeTraversal::Parent(*ancestor)) {
    if (IsA<HTMLSelectMenuElement>(ancestor))
      return DynamicTo<HTMLSelectMenuElement>(ancestor);
    if (IsA<HTMLSelectElement>(ancestor))
      return nullptr;
  }

  return nullptr;
}

bool SelectMenuPartTraversal::IsNestedSelectMenu(const Node& node) {
  // When searching for parts of a given <selectmenu>, don't look
  // inside nested <selectmenu> or <select> elements.
  return IsA<HTMLSelectMenuElement>(node) || IsA<HTMLSelectElement>(node);
}

}  // namespace blink
