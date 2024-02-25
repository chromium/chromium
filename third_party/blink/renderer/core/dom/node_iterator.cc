/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/node_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

NodeIterator::NodePointer::NodePointer() = default;

NodeIterator::NodePointer::NodePointer(Node* n, bool b)
    : node(n), is_pointer_before_node(b) {}

void NodeIterator::NodePointer::Clear() {
  node.Clear();
}

bool NodeIterator::NodePointer::MoveToNext(Node* root) {
  if (!node)
    return false;
  if (is_pointer_before_node) {
    is_pointer_before_node = false;
    return true;
  }
  node = NodeTraversal::Next(*node, root);
  return node != nullptr;
}

bool NodeIterator::NodePointer::MoveToPrevious(Node* root) {
  if (!node)
    return false;
  if (!is_pointer_before_node) {
    is_pointer_before_node = true;
    return true;
  }
  node = NodeTraversal::Previous(*node, root);
  return node != nullptr;
}

NodeIterator::NodeIterator(Node* root_node,
                           unsigned what_to_show,
                           V8NodeFilter* filter)
    : NodeIteratorBase(root_node, what_to_show, filter),
      reference_node_(root(), true) {
  // If NodeIterator target is Attr node, don't subscribe for nodeWillBeRemoved,
  // as it would never have child nodes.
  if (!root()->IsAttributeNode())
    root()->GetDocument().AttachNodeIterator(this);
}

Node* NodeIterator::nextNode(ExceptionState& exception_state) {
  Node* result = nullptr;

  candidate_node_ = reference_node_;
  while (candidate_node_.MoveToNext(root())) {
    // NodeIterators treat the DOM tree as a flat list of nodes.
    // In other words, kFilterReject does not pass over descendants
    // of the rejected node. Hence, kFilterReject is the same as kFilterSkip.
    Node* provisional_result = candidate_node_.node;
    bool node_was_accepted = AcceptNode(provisional_result, exception_state) ==
                             V8NodeFilter::FILTER_ACCEPT;
    if (exception_state.HadException())
      break;
    if (node_was_accepted) {
      reference_node_ = candidate_node_;
      result = provisional_result;
      break;
    }
  }

  candidate_node_.Clear();
  return result;
}

Node* NodeIterator::previousNode(ExceptionState& exception_state) {
  Node* result = nullptr;

  candidate_node_ = reference_node_;
  while (candidate_node_.MoveToPrevious(root())) {
    // NodeIterators treat the DOM tree as a flat list of nodes.
    // In other words, kFilterReject does not pass over descendants
    // of the rejected node. Hence, kFilterReject is the same as kFilterSkip.
    Node* provisional_result = candidate_node_.node;
    bool node_was_accepted = AcceptNode(provisional_result, exception_state) ==
                             V8NodeFilter::FILTER_ACCEPT;
    if (exception_state.HadException())
      break;
    if (node_was_accepted) {
      reference_node_ = candidate_node_;
      result = provisional_result;
      break;
    }
  }

  candidate_node_.Clear();
  return result;
}

void NodeIterator::detach() {
  // This is now a no-op as per the DOM specification.
}

void NodeIterator::NodeWillBeRemoved(Node& removed_node) {
  UpdateForNodeRemoval(removed_node, candidate_node_);
  UpdateForNodeRemoval(removed_node, reference_node_);
}

void NodeIterator::UpdateForNodeRemoval(Node& removed_node,
                                        NodePointer& reference_node) const {
  DCHECK_EQ(root()->GetDocument(), removed_node.GetDocument());

  // Iterator is not affected if the removed node is the reference node and is
  // the root.  or if removed node is not the reference node, or the ancestor of
  // the reference node.
  if (!removed_node.IsDescendantOf(root()))
    return;
  bool will_remove_reference_node = removed_node == reference_node.node.Get();
  bool will_remove_reference_node_ancestor =
      reference_node.node && reference_node.node->IsDescendantOf(&removed_node);
  if (!will_remove_reference_node && !will_remove_reference_node_ancestor)
    return;

  if (reference_node.is_pointer_before_node) {
    Node* node = NodeTraversal::Next(removed_node, root());
    if (node) {
      // Move out from under the node being removed if the new reference
      // node is a descendant of the node being removed.
      while (node && node->IsDescendantOf(&removed_node))
        node = NodeTraversal::Next(*node, root());
      if (node)
        reference_node.node = node;
    } else {
      node = NodeTraversal::Previous(removed_node, root());
      if (node) {
        // Move out from under the node being removed if the reference node is
        // a descendant of the node being removed.
        if (will_remove_reference_node_ancestor) {
          while (node && node->IsDescendantOf(&removed_node))
            node = NodeTraversal::Previous(*node, root());
        }
        if (node) {
          // Removing last node.
          // Need to move the pointer after the node preceding the
          // new reference node.
          reference_node.node = node;
          reference_node.is_pointer_before_node = false;
        }
      }
    }
  } else {
    Node* node = NodeTraversal::Previous(removed_node, root());
    if (node) {
      // Move out from under the node being removed if the reference node is
      // a descendant of the node being removed.
      if (will_remove_reference_node_ancestor) {
        while (node && node->IsDescendantOf(&removed_node))
          node = NodeTraversal::Previous(*node, root());
      }
      if (node)
        reference_node.node = node;
    } else {
      // FIXME: This branch doesn't appear to have any web tests.
      node = NodeTraversal::Next(removed_node, root());
      // Move out from under the node being removed if the reference node is
      // a descendant of the node being removed.
      if (will_remove_reference_node_ancestor) {
        while (node && node->IsDescendantOf(&removed_node))
          node = NodeTraversal::Previous(*node, root());
      }
      if (node)
        reference_node.node = node;
    }
  }
}

void NodeIterator::Trace(Visitor* visitor) const {
  visitor->Trace(reference_node_);
  visitor->Trace(candidate_node_);
  ScriptWrappable::Trace(visitor);
  NodeIteratorBase::Trace(visitor);
}

}  // namespace blink
