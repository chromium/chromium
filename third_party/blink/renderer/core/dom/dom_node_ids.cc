// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

DEFINE_WEAK_IDENTIFIER_MAP(Node, DOMNodeId)

// static
DOMNodeId DOMNodeIds::ExistingIdForNode(Node* node) {
  return node ? WeakIdentifierMap<Node, DOMNodeId>::ExistingIdentifier(node)
              : kInvalidDOMNodeId;
}

// static
DOMNodeId DOMNodeIds::ExistingIdForNode(const Node* node) {
  return ExistingIdForNode(const_cast<Node*>(node));
}

// static
DOMNodeId DOMNodeIds::IdForNode(Node* node) {
  return node ? WeakIdentifierMap<Node, DOMNodeId>::Identifier(node)
              : kInvalidDOMNodeId;
}

// static
Node* DOMNodeIds::NodeForId(DOMNodeId id) {
  return id == kInvalidDOMNodeId
             ? nullptr
             : WeakIdentifierMap<Node, DOMNodeId>::Lookup(id);
}

}  // namespace blink
