/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/dom/node_traversal.h"

#include "third_party/blink/renderer/core/dom/container_node.h"

namespace blink {

Node* NodeTraversal::PreviousIncludingPseudo(const Node& current,
                                             const Node* stay_within) {
  if (current == stay_within)
    return nullptr;
  if (Node* previous = current.PseudoAwarePreviousSibling()) {
    while (previous->PseudoAwareLastChild())
      previous = previous->PseudoAwareLastChild();
    return previous;
  }
  return current.parentNode();
}

Node* NodeTraversal::NextIncludingPseudo(const Node& current,
                                         const Node* stay_within) {
  if (Node* next = current.PseudoAwareFirstChild())
    return next;
  for (Node& node : InclusiveAncestorsOf(current)) {
    if (node == stay_within)
      return nullptr;
    if (Node* next = node.PseudoAwareNextSibling())
      return next;
  }
  return nullptr;
}

Node* NodeTraversal::NextIncludingPseudoSkippingChildren(
    const Node& current,
    const Node* stay_within) {
  for (Node& node : InclusiveAncestorsOf(current)) {
    if (node == stay_within)
      return nullptr;
    if (Node* next = node.PseudoAwareNextSibling())
      return next;
  }
  return nullptr;
}

Node* NodeTraversal::NextAncestorSibling(const Node& current) {
  DCHECK(!current.HasNextSibling());
  for (Node& parent : AncestorsOf(current)) {
    if (parent.HasNextSibling()) {
      return parent.nextSibling();
    }
  }
  return nullptr;
}

Node* NodeTraversal::NextAncestorSibling(const Node& current,
                                         const Node* stay_within) {
  DCHECK(!current.HasNextSibling());
  DCHECK_NE(current, stay_within);
  for (Node& parent : AncestorsOf(current)) {
    if (parent == stay_within)
      return nullptr;
    if (parent.HasNextSibling()) {
      return parent.nextSibling();
    }
  }
  return nullptr;
}

Node* NodeTraversal::LastWithin(const ContainerNode& current) {
  Node* descendant = current.lastChild();
  for (Node* child = descendant; child; child = child->lastChild())
    descendant = child;
  return descendant;
}

Node& NodeTraversal::LastWithinOrSelf(Node& current) {
  auto* curr_node = DynamicTo<ContainerNode>(current);
  Node* last_descendant =
      curr_node ? NodeTraversal::LastWithin(*curr_node) : nullptr;
  return last_descendant ? *last_descendant : current;
}

Node* NodeTraversal::Previous(const Node& current, const Node* stay_within) {
  if (current == stay_within)
    return nullptr;
  if (current.HasPreviousSibling()) {
    Node* previous = current.previousSibling();
    while (Node* child = previous->lastChild())
      previous = child;
    return previous;
  }
  return current.parentNode();
}

Node* NodeTraversal::PreviousAbsoluteSiblingIncludingPseudo(
    const Node& current,
    const Node* stay_within) {
  for (Node& iter : InclusiveAncestorsOf(current)) {
    if (iter == stay_within)
      return nullptr;
    if (Node* result = iter.PseudoAwarePreviousSibling())
      return result;
  }
  return nullptr;
}

Node* NodeTraversal::PreviousAbsoluteSibling(const Node& current,
                                             const Node* stay_within) {
  for (Node& node : InclusiveAncestorsOf(current)) {
    if (node == stay_within)
      return nullptr;
    if (node.HasPreviousSibling()) {
      return node.previousSibling();
    }
  }
  return nullptr;
}

Node* NodeTraversal::NextPostOrder(const Node& current,
                                   const Node* stay_within) {
  if (current == stay_within)
    return nullptr;
  if (!current.HasNextSibling()) {
    return current.parentNode();
  }
  Node* next = current.nextSibling();
  while (Node* child = next->firstChild())
    next = child;
  return next;
}

Node* NodeTraversal::PreviousAncestorSiblingPostOrder(const Node& current,
                                                      const Node* stay_within) {
  DCHECK(!current.HasPreviousSibling());
  for (Node& parent : NodeTraversal::AncestorsOf(current)) {
    if (parent == stay_within)
      return nullptr;
    if (parent.HasPreviousSibling()) {
      return parent.previousSibling();
    }
  }
  return nullptr;
}

Node* NodeTraversal::PreviousPostOrder(const Node& current,
                                       const Node* stay_within) {
  if (Node* last_child = current.lastChild())
    return last_child;
  if (current == stay_within)
    return nullptr;
  if (current.HasPreviousSibling()) {
    return current.previousSibling();
  }
  return PreviousAncestorSiblingPostOrder(current, stay_within);
}

Node* NodeTraversal::CommonAncestor(const Node& node_a, const Node& node_b) {
  return node_a.CommonAncestor(node_b, NodeTraversal::Parent);
}

}  // namespace blink
