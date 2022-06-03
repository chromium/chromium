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

#include "third_party/blink/renderer/core/dom/tree_walker.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal_strategy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TreeWalker::TreeWalker(Node* root_node,
                       unsigned what_to_show,
                       V8NodeFilter* filter)
    : NodeIteratorBase(root_node, what_to_show, filter), current_(root()) {}

void TreeWalker::setCurrentNode(Node* node) {
  DCHECK(node);
  current_ = node;
}

inline Node* TreeWalker::SetCurrent(Node* node) {
  current_ = node;
  return current_.Get();
}

Node* TreeWalker::parentNode(ExceptionState& exception_state) {
  Node* node = current_;
  while (node != root()) {
    node = node->parentNode();
    if (!node)
      return nullptr;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (accept_node_result == V8NodeFilter::FILTER_ACCEPT)
      return SetCurrent(node);
  }
  return nullptr;
}

Node* TreeWalker::firstChild(ExceptionState& exception_state) {
  for (Node* node = current_->firstChild(); node;) {
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    switch (accept_node_result) {
      case V8NodeFilter::FILTER_ACCEPT:
        current_ = node;
        return current_.Get();
      case V8NodeFilter::FILTER_SKIP:
        if (node->hasChildren()) {
          node = node->firstChild();
          continue;
        }
        break;
      case V8NodeFilter::FILTER_REJECT:
        break;
    }
    do {
      if (node->nextSibling()) {
        node = node->nextSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == current_)
        return nullptr;
      node = parent;
    } while (node);
  }
  return nullptr;
}

Node* TreeWalker::lastChild(ExceptionState& exception_state) {
  for (Node* node = current_->lastChild(); node;) {
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    switch (accept_node_result) {
      case V8NodeFilter::FILTER_ACCEPT:
        current_ = node;
        return current_.Get();
      case V8NodeFilter::FILTER_SKIP:
        if (node->lastChild()) {
          node = node->lastChild();
          continue;
        }
        break;
      case V8NodeFilter::FILTER_REJECT:
        break;
    }
    do {
      if (node->previousSibling()) {
        node = node->previousSibling();
        break;
      }
      ContainerNode* parent = node->parentNode();
      if (!parent || parent == root() || parent == current_)
        return nullptr;
      node = parent;
    } while (node);
  }
  return nullptr;
}

// https://dom.spec.whatwg.org/#concept-traverse-siblings
template <typename Strategy>
Node* TreeWalker::TraverseSiblings(ExceptionState& exception_state) {
  // 1. Let node be the value of the currentNode attribute.
  Node* node = current_;
  // 2. If node is root, return null.
  if (node == root())
    return nullptr;
  // 3. While true:
  while (true) {
    // 1. Let sibling be node's next sibling if type is next, and node's
    // previous sibling if type is previous.
    Node* sibling = Strategy::NextNode(*node);
    // 2. While sibling is not null:
    while (sibling) {
      // 1. Set node to sibling.
      node = sibling;
      // 2. Filter node and let result be the return value.
      unsigned result = AcceptNode(node, exception_state);
      if (exception_state.HadException())
        return nullptr;
      // 3. If result is FILTER_ACCEPT, then set the currentNode attribute to
      // node and return node.
      if (result == V8NodeFilter::FILTER_ACCEPT)
        return SetCurrent(node);
      // 4. Set sibling to node's first child if type is next, and node's last
      // child if type is previous.
      sibling = Strategy::StartNode(*sibling);
      // 5. If result is FILTER_REJECT or sibling is null, then set sibling to
      // node's next sibling if type is next, and node's previous sibling if
      // type is previous.
      if (result == V8NodeFilter::FILTER_REJECT || !sibling)
        sibling = Strategy::NextNode(*node);
    }
    // 3. Set node to its parent.
    node = node->parentNode();
    // 4. If node is null or is root, return null.
    if (!node || node == root())
      return nullptr;
    // 5. Filter node and if the return value is FILTER_ACCEPT, then return
    // null.
    unsigned result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (result == V8NodeFilter::FILTER_ACCEPT)
      return nullptr;
  }
}

Node* TreeWalker::previousSibling(ExceptionState& exception_state) {
  return TraverseSiblings<PreviousNodeTraversalStrategy>(exception_state);
}

Node* TreeWalker::nextSibling(ExceptionState& exception_state) {
  return TraverseSiblings<NextNodeTraversalStrategy>(exception_state);
}

Node* TreeWalker::previousNode(ExceptionState& exception_state) {
  Node* node = current_;
  while (node != root()) {
    while (Node* previous_sibling = node->previousSibling()) {
      node = previous_sibling;
      unsigned accept_node_result = AcceptNode(node, exception_state);
      if (exception_state.HadException())
        return nullptr;
      if (accept_node_result == V8NodeFilter::FILTER_REJECT)
        continue;
      while (Node* last_child = node->lastChild()) {
        node = last_child;
        accept_node_result = AcceptNode(node, exception_state);
        if (exception_state.HadException())
          return nullptr;
        if (accept_node_result == V8NodeFilter::FILTER_REJECT)
          break;
      }
      if (accept_node_result == V8NodeFilter::FILTER_ACCEPT) {
        current_ = node;
        return current_.Get();
      }
    }
    if (node == root())
      return nullptr;
    ContainerNode* parent = node->parentNode();
    if (!parent)
      return nullptr;
    node = parent;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (accept_node_result == V8NodeFilter::FILTER_ACCEPT)
      return SetCurrent(node);
  }
  return nullptr;
}

Node* TreeWalker::nextNode(ExceptionState& exception_state) {
  Node* node = current_;
Children:
  while (Node* first_child = node->firstChild()) {
    node = first_child;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (accept_node_result == V8NodeFilter::FILTER_ACCEPT)
      return SetCurrent(node);
    if (accept_node_result == V8NodeFilter::FILTER_REJECT)
      break;
  }
  while (Node* next_sibling =
             NodeTraversal::NextSkippingChildren(*node, root())) {
    node = next_sibling;
    unsigned accept_node_result = AcceptNode(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (accept_node_result == V8NodeFilter::FILTER_ACCEPT)
      return SetCurrent(node);
    if (accept_node_result == V8NodeFilter::FILTER_SKIP)
      goto Children;
  }
  return nullptr;
}

void TreeWalker::Trace(Visitor* visitor) const {
  visitor->Trace(current_);
  ScriptWrappable::Trace(visitor);
  NodeIteratorBase::Trace(visitor);
}

}  // namespace blink
