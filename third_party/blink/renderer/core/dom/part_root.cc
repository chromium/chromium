// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part_root.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/part.h"

namespace blink {

void PartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(parts_unordered_);
  visitor->Trace(cached_ordered_parts_);
  ScriptWrappable::Trace(visitor);
}

void PartRoot::AddPart(Part& new_part) {
  CHECK(!parts_unordered_.Contains(&new_part));
  parts_unordered_.push_back(new_part);
  cached_parts_list_dirty_ = true;
}

void PartRoot::RemovePart(Part& part) {
  auto index = parts_unordered_.Find(&part);
  CHECK_NE(index, kNotFound);
  parts_unordered_.EraseAt(index);
  cached_parts_list_dirty_ = true;
}

namespace {

Node* LowestCommonAncestor(Node* a,
                           int a_depth,
                           Node* b,
                           int b_depth,
                           int& lca_depth) {
  CHECK(a && b);
  CHECK_GE(a_depth, 0);
  CHECK_GE(b_depth, 0);
  if (a == b) {
    return a;
  }
  while (a_depth > b_depth) {
    a = a->parentNode();
    CHECK(a) << "a should be connected";
    --a_depth;
  }
  while (b_depth > a_depth) {
    b = b->parentNode();
    CHECK(b) << "b should be connected";
    --b_depth;
  }
  while (a != b) {
    a = a->parentNode();
    b = b->parentNode();
    --a_depth;
    CHECK(a && b) << "a and b should be in the same tree";
  }
  lca_depth = a_depth;
  CHECK_GE(lca_depth, 0);
  return a;
}

using NodesToParts =
    HeapHashMap<Member<Node>, Member<HeapVector<Member<Part>>>>;

// TODO(crbug.com/1453291) This routine is a performance-sensitive one, and
// is where speed matters for the DOM Parts API. The current algorithm is:
//  - Find the LCA of all of the nodes that need an update, and then walk the
//    entire tree under the LCA. That should be O(k*log(n) + n) where n is the
//    number of nodes in the sub-tree (assuming rough tree symmetry), and k is
//    the number of parts.
// This approach was selected primarily for simplicity.
//
// A few alternative approaches might be:
//  - Loop through the parts, and do some sort of binary insertion sort using
//    something like `compareDocumentPosition`. That should be
//    O((m+log(n)) * log(k) * k), where m is the average fan-out of the tree.
//  - Implement a sort algorithm based on the internals of
//    `compareDocumentPosition`, maintaining the ancestor chain for each node
//    (and a progress marker within it) during the entire sort, and doing a
//    sort-of-quicksort-like splitting whenever there are branches in the
//    ancestor chain.
// It might be worthwhile to switch between these approaches depending on the
// sizes of things, or add additional algorithms.
HeapVector<Member<Part>> SortPartsInTreeOrder(
    NodesToParts unordered_nodes_to_parts) {
  HeapVector<Member<Part>> ordered_parts;
  if (unordered_nodes_to_parts.empty()) {
    return ordered_parts;
  }
  // First find the lowest common ancestor of all of the nodes.
  int lca_depth = -1;
  Node* lca = nullptr;
  for (auto& entry : unordered_nodes_to_parts) {
    Node* node = entry.key;
    int node_depth = 0;
    Node* walk = node;
    while (walk) {
      ++node_depth;
      walk = walk->parentNode();
    }
    if (!lca) {
      lca_depth = node_depth;
      lca = node;
    } else {
      lca = LowestCommonAncestor(lca, lca_depth, node, node_depth, lca_depth);
    }
#if DCHECK_IS_ON()
    for (auto& part : *entry.value) {
      DCHECK_EQ(node, part->RelevantNode());
    }
#endif
  }

  // Then traverse the tree under the LCA and add parts in the order they're
  // found in the tree.
  for (auto& child : NodeTraversal::InclusiveDescendantsOf(*lca)) {
    auto it = unordered_nodes_to_parts.find(&child);
    if (it != unordered_nodes_to_parts.end()) {
      for (auto& part : *it->value) {
        ordered_parts.push_back(part);
      }
    }
  }
  return ordered_parts;
}

}  // namespace

DocumentPartRoot* PartRoot::GetDocumentPartRoot() {
  PartRoot* root = this;
  while (!root->IsDocumentPartRoot()) {
    CHECK(root->IsPart());
    root = static_cast<Part*>(root)->root();
  }
  return static_cast<DocumentPartRoot*>(root);
}

// |getParts| must always return the contained parts list subject to these
// rules:
//  1. parts are returned in DOM tree order. If more than one part refers to the
//     same Node, parts are returned in the order they were constructed.
//  2. parts referring to nodes that aren't in a document, or not in the same
//     document as the owning DocumentPartRoot, are not returned.
//  3. invalid parts are not returned. For example, a ChildNodePart whose
//     previous_node comes after its next_node.
HeapVector<Member<Part>> PartRoot::RebuildPartsList() {
  CHECK(cached_parts_list_dirty_);
  NodesToParts unordered_nodes_to_parts;
  DocumentPartRoot* root = GetDocumentPartRoot();
  if (!root) {
    return HeapVector<Member<Part>>();
  }
  Document* root_document = root->GetDocument();
  CHECK(root_document);
  for (Part* part : parts_unordered_) {
    if (!part->IsValid() || part->GetDocument() != root_document) {
      continue;
    }
    Node* node = part->RelevantNode();
    CHECK(node->isConnected());
    CHECK_EQ(&node->GetDocument(), root_document);
    auto result = unordered_nodes_to_parts.insert(node, nullptr);
    if (result.is_new_entry) {
      result.stored_value->value =
          MakeGarbageCollected<HeapVector<Member<Part>>>();
    }
    result.stored_value->value->push_back(part);
  }
  return SortPartsInTreeOrder(unordered_nodes_to_parts);
}

HeapVector<Member<Part>> PartRoot::getParts() {
  if (cached_parts_list_dirty_) {
    cached_ordered_parts_ = RebuildPartsList();
    // TODO(crbug.com/1453291) Set cached_parts_list_dirty_=false here.
  }
  return cached_ordered_parts_;
}

std::ostream& operator<<(std::ostream& ostream, const PartRoot& part) {
  return ostream << part.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& ostream, const PartRoot* part) {
  if (!part) {
    return ostream << "null";
  }
  return ostream << *part;
}

}  // namespace blink
