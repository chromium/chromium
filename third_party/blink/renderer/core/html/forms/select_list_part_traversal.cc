// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/select_list_part_traversal.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"

namespace blink {

Node* SelectListPartTraversal::NextSibling(const Node& node) {
  Node* next = FlatTreeTraversal::NextSibling(node);
  while (next && SelectListPartTraversal::IsNestedSelectList(*next)) {
    next = FlatTreeTraversal::NextSibling(*next);
  }
  return next;
}

Node* SelectListPartTraversal::PreviousSibling(const Node& node) {
  Node* next = FlatTreeTraversal::PreviousSibling(node);
  while (next && SelectListPartTraversal::IsNestedSelectList(*next)) {
    next = FlatTreeTraversal::PreviousSibling(*next);
  }
  return next;
}

Node* SelectListPartTraversal::FirstChild(const Node& node) {
  Node* first = FlatTreeTraversal::FirstChild(node);
  while (first && SelectListPartTraversal::IsNestedSelectList(*first)) {
    first = SelectListPartTraversal::NextSibling(*first);
  }
  return first;
}

Node* SelectListPartTraversal::LastChild(const Node& node) {
  Node* first = FlatTreeTraversal::LastChild(node);
  while (first && SelectListPartTraversal::IsNestedSelectList(*first)) {
    first = SelectListPartTraversal::PreviousSibling(*first);
  }
  return first;
}

namespace {

static Node* NextAncestorSibling(const Node& node, const Node* stay_within) {
  DCHECK(!SelectListPartTraversal::NextSibling(node));
  DCHECK_NE(node, stay_within);
  for (Node* parent_node = FlatTreeTraversal::Parent(node); parent_node;
       parent_node = FlatTreeTraversal::Parent(*parent_node)) {
    if (parent_node == stay_within)
      return nullptr;
    if (Node* next_node = SelectListPartTraversal::NextSibling(*parent_node))
      return next_node;
  }
  return nullptr;
}

}  // namespace

Node* SelectListPartTraversal::NextSkippingChildren(const Node& node,
                                                    const Node* stay_within) {
  if (node == stay_within)
    return nullptr;
  if (Node* next_node = NextSibling(node))
    return next_node;
  return NextAncestorSibling(node, stay_within);
}

Node* SelectListPartTraversal::Next(const Node& node, const Node* stay_within) {
  if (Node* child = FirstChild(node))
    return child;
  return NextSkippingChildren(node, stay_within);
}

Node* SelectListPartTraversal::Previous(const Node& node,
                                        const Node* stay_within) {
  if (Node* previous = PreviousSibling(node)) {
    while (Node* child = LastChild(*previous))
      previous = child;
    return previous;
  }
  Node* parent = FlatTreeTraversal::Parent(node);
  return parent != stay_within ? parent : nullptr;
}

bool SelectListPartTraversal::IsDescendantOf(const Node& node,
                                             const Node& other) {
  for (const Node* ancestor = FlatTreeTraversal::Parent(node); ancestor;
       ancestor = FlatTreeTraversal::Parent(*ancestor)) {
    if (ancestor == other)
      return true;
    if (IsNestedSelectList(*ancestor))
      return false;
  }
  return false;
}

HTMLSelectListElement* SelectListPartTraversal::NearestSelectListAncestor(
    const Node& node) {
  for (Node* ancestor = FlatTreeTraversal::Parent(node); ancestor;
       ancestor = FlatTreeTraversal::Parent(*ancestor)) {
    if (auto* select_list = DynamicTo<HTMLSelectListElement>(ancestor)) {
      return select_list;
    }
    if (IsA<HTMLSelectElement>(ancestor))
      return nullptr;
  }

  return nullptr;
}

bool SelectListPartTraversal::IsNestedSelectList(const Node& node) {
  // When searching for parts of a given <selectlist>, don't look
  // inside nested <selectlist> or <select> elements.
  return IsA<HTMLSelectListElement>(node) || IsA<HTMLSelectElement>(node);
}

}  // namespace blink
