/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xpath_node_set.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"

namespace blink {
namespace xpath {

// When a node set is large, sorting it by traversing the whole document is
// better (we can assume that we aren't dealing with documents that we cannot
// even traverse in reasonable time).
const unsigned kTraversalSortCutoff = 10000;

typedef HeapVector<Member<Node>> NodeSetVector;

NodeSet* NodeSet::Create(const NodeSet& other) {
  NodeSet* node_set = NodeSet::Create();
  node_set->is_sorted_ = other.is_sorted_;
  node_set->subtrees_are_disjoint_ = other.subtrees_are_disjoint_;
  node_set->nodes_.AppendVector(other.nodes_);
  return node_set;
}

static inline Node* ParentWithDepth(unsigned depth,
                                    const NodeSetVector& parents) {
  DCHECK_GE(parents.size(), depth + 1);
  return parents[parents.size() - 1 - depth];
}

static void SortBlock(unsigned from,
                      unsigned to,
                      HeapVector<NodeSetVector>& parent_matrix,
                      bool may_contain_attribute_nodes) {
  // Should not call this function with less that two nodes to sort.
  DCHECK_LT(from + 1, to);
  unsigned min_depth = UINT_MAX;
  for (unsigned i = from; i < to; ++i) {
    unsigned depth = parent_matrix[i].size() - 1;
    if (min_depth > depth)
      min_depth = depth;
  }

  // Find the common ancestor.
  unsigned common_ancestor_depth = min_depth;
  Node* common_ancestor;
  while (true) {
    common_ancestor =
        ParentWithDepth(common_ancestor_depth, parent_matrix[from]);
    if (common_ancestor_depth == 0)
      break;

    bool all_equal = true;
    for (unsigned i = from + 1; i < to; ++i) {
      if (common_ancestor !=
          ParentWithDepth(common_ancestor_depth, parent_matrix[i])) {
        all_equal = false;
        break;
      }
    }
    if (all_equal)
      break;

    --common_ancestor_depth;
  }

  if (common_ancestor_depth == min_depth) {
    // One of the nodes is the common ancestor => it is the first in
    // document order. Find it and move it to the beginning.
    for (unsigned i = from; i < to; ++i) {
      if (common_ancestor == parent_matrix[i][0]) {
        parent_matrix[i].swap(parent_matrix[from]);
        if (from + 2 < to)
          SortBlock(from + 1, to, parent_matrix, may_contain_attribute_nodes);
        return;
      }
    }
  }

  if (may_contain_attribute_nodes && common_ancestor->IsElementNode()) {
    // The attribute nodes and namespace nodes of an element occur before
    // the children of the element. The namespace nodes are defined to occur
    // before the attribute nodes. The relative order of namespace nodes is
    // implementation-dependent. The relative order of attribute nodes is
    // implementation-dependent.
    unsigned sorted_end = from;
    // FIXME: namespace nodes are not implemented.
    for (unsigned i = sorted_end; i < to; ++i) {
      Node* n = parent_matrix[i][0];
      auto* attr = DynamicTo<Attr>(n);
      if (attr && attr->ownerElement() == common_ancestor)
        parent_matrix[i].swap(parent_matrix[sorted_end++]);
    }
    if (sorted_end != from) {
      if (to - sorted_end > 1)
        SortBlock(sorted_end, to, parent_matrix, may_contain_attribute_nodes);
      return;
    }
  }

  // Children nodes of the common ancestor induce a subdivision of our
  // node-set. Sort it according to this subdivision, and recursively sort
  // each group.
  HeapHashSet<Member<Node>> parent_nodes;
  for (unsigned i = from; i < to; ++i)
    parent_nodes.insert(
        ParentWithDepth(common_ancestor_depth + 1, parent_matrix[i]));

  unsigned previous_group_end = from;
  unsigned group_end = from;
  for (Node* n = common_ancestor->firstChild(); n; n = n->nextSibling()) {
    // If parentNodes contains the node, perform a linear search to move its
    // children in the node-set to the beginning.
    if (parent_nodes.Contains(n)) {
      for (unsigned i = group_end; i < to; ++i) {
        if (ParentWithDepth(common_ancestor_depth + 1, parent_matrix[i]) == n)
          parent_matrix[i].swap(parent_matrix[group_end++]);
      }

      if (group_end - previous_group_end > 1)
        SortBlock(previous_group_end, group_end, parent_matrix,
                  may_contain_attribute_nodes);

      DCHECK_NE(previous_group_end, group_end);
      previous_group_end = group_end;
#if DCHECK_IS_ON()
      parent_nodes.erase(n);
#endif
    }
  }

  DCHECK(parent_nodes.IsEmpty());
}

void NodeSet::Sort() const {
  if (is_sorted_)
    return;

  unsigned node_count = nodes_.size();
  if (node_count < 2) {
    const_cast<bool&>(is_sorted_) = true;
    return;
  }

  if (node_count > kTraversalSortCutoff) {
    TraversalSort();
    return;
  }

  bool contains_attribute_nodes = false;

  HeapVector<NodeSetVector> parent_matrix(node_count);
  for (unsigned i = 0; i < node_count; ++i) {
    NodeSetVector& parents_vector = parent_matrix[i];
    Node* n = nodes_[i].Get();
    parents_vector.push_back(n);
    if (auto* attr = DynamicTo<Attr>(n)) {
      n = attr->ownerElement();
      parents_vector.push_back(n);
      contains_attribute_nodes = true;
    }
    for (n = n->parentNode(); n; n = n->parentNode())
      parents_vector.push_back(n);
  }
  SortBlock(0, node_count, parent_matrix, contains_attribute_nodes);

  // It is not possible to just assign the result to m_nodes, because some
  // nodes may get dereferenced and destroyed.
  HeapVector<Member<Node>> sorted_nodes;
  sorted_nodes.ReserveInitialCapacity(node_count);
  for (unsigned i = 0; i < node_count; ++i)
    sorted_nodes.push_back(parent_matrix[i][0]);

  const_cast<HeapVector<Member<Node>>&>(nodes_).swap(sorted_nodes);
}

static Node* FindRootNode(Node* node) {
  if (auto* attr = DynamicTo<Attr>(node))
    node = attr->ownerElement();
  if (node->isConnected()) {
    node = &node->GetDocument();
  } else {
    while (Node* parent = node->parentNode())
      node = parent;
  }
  return node;
}

void NodeSet::TraversalSort() const {
  HeapHashSet<Member<Node>> nodes;
  bool contains_attribute_nodes = false;

  unsigned node_count = nodes_.size();
  DCHECK_GT(node_count, 1u);
  for (unsigned i = 0; i < node_count; ++i) {
    Node* node = nodes_[i].Get();
    nodes.insert(node);
    if (node->IsAttributeNode())
      contains_attribute_nodes = true;
  }

  HeapVector<Member<Node>> sorted_nodes;
  sorted_nodes.ReserveInitialCapacity(node_count);

  for (Node& n : NodeTraversal::StartsAt(*FindRootNode(nodes_.front()))) {
    if (nodes.Contains(&n))
      sorted_nodes.push_back(&n);

    auto* element = DynamicTo<Element>(&n);
    if (!element || !contains_attribute_nodes)
      continue;

    AttributeCollection attributes = element->Attributes();
    for (auto& attribute : attributes) {
      Attr* attr = element->AttrIfExists(attribute.GetName());
      if (attr && nodes.Contains(attr))
        sorted_nodes.push_back(attr);
    }
  }

  DCHECK_EQ(sorted_nodes.size(), node_count);
  const_cast<HeapVector<Member<Node>>&>(nodes_).swap(sorted_nodes);
}

void NodeSet::Reverse() {
  if (nodes_.IsEmpty())
    return;

  unsigned from = 0;
  unsigned to = nodes_.size() - 1;
  while (from < to) {
    nodes_[from].Swap(nodes_[to]);
    ++from;
    --to;
  }
}

Node* NodeSet::FirstNode() const {
  if (IsEmpty())
    return nullptr;

  // FIXME: fully sorting the node-set just to find its first node is
  // wasteful.
  Sort();
  return nodes_.at(0).Get();
}

Node* NodeSet::AnyNode() const {
  if (IsEmpty())
    return nullptr;

  return nodes_.at(0).Get();
}

}  // namespace xpath
}  // namespace blink
