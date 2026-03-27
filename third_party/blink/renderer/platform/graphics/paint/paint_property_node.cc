// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

int PaintPropertyNode::NodeDepthOrFoundAncestor(
    const PaintPropertyNode& maybe_ancestor) const {
  int depth = 0;
  for (const auto* n = this; n; n = n->Parent()) {
    if (n == &maybe_ancestor) {
      return -1;
    }
    depth++;
  }
  return depth;
}

const PaintPropertyNode& PaintPropertyNode::LowestCommonAncestorInternal(
    const PaintPropertyNode& other) const {
  // Measure both depths.
  auto depth_a = NodeDepthOrFoundAncestor(other);
  if (depth_a == -1) {
    return other;
  }
  auto depth_b = other.NodeDepthOrFoundAncestor(*this);
  if (depth_b == -1) {
    return *this;
  }

  const auto* a_ptr = this;
  const auto* b_ptr = &other;

  // Make it so depth_a >= depth_b.
  if (depth_a < depth_b) {
    std::swap(a_ptr, b_ptr);
    std::swap(depth_a, depth_b);
  }

  // Make it so depth_a == depth_b.
  while (depth_a > depth_b) {
    a_ptr = a_ptr->Parent();
    depth_a--;
  }

  // Walk up until we find the ancestor.
  while (a_ptr != b_ptr) {
    a_ptr = a_ptr->Parent();
    b_ptr = b_ptr->Parent();
  }

  DCHECK(a_ptr) << "Malformed property tree. All nodes must be descendant of "
                   "the same root.";
  return *a_ptr;
}

String PaintPropertyNode::ToString() const {
  String s = ToJSON()->ToJSONString();
#if DCHECK_IS_ON()
  return StrCat({debug_name_, String::Format(" %p ", this), s});
#else
  return s;
#endif
}

std::unique_ptr<JSONObject> PaintPropertyNode::ToJSON() const {
  auto json = std::make_unique<JSONObject>();
  if (Parent()) {
    json->SetString("parent", String::Format("%p", Parent()));
  }
  if (IsParentAlias()) {
    json->SetBoolean("is_alias", true);
  }
  if (NodeChanged() != PaintPropertyChangeType::kUnchanged) {
    json->SetString("changed", PaintPropertyChangeTypeToString(NodeChanged()));
  }
  return json;
}

const char* PaintPropertyChangeTypeToString(PaintPropertyChangeType change) {
  switch (change) {
    case PaintPropertyChangeType::kUnchanged:
      return "unchanged";
    case PaintPropertyChangeType::kChangedOnlyCompositedValues:
      return "composited-values";
    case PaintPropertyChangeType::kChangedOnlySimpleValues:
      return "simple-values";
    case PaintPropertyChangeType::kChangedOnlyValues:
      return "values";
    case PaintPropertyChangeType::kNodeAddedOrRemoved:
      return "node-add-remove";
  }
}

#if DCHECK_IS_ON()

String PaintPropertyNode::ToTreeString() const {
  return PropertyTreePrinter().PathAsString(*this);
}

void PropertyTreePrinter::AddNode(const PaintPropertyNode* node) {
  if (node) {
    nodes_.insert(node);
  }
}

String PropertyTreePrinter::NodesAsTreeString() {
  if (nodes_.empty()) {
    return "";
  }
  StringBuilder string_builder;
  HeapHashSet<Member<const PaintPropertyNode>> visited;
  size_t roots = 0;
  for (const auto& n : nodes_) {
    if (visited.Contains(n)) {
      continue;
    }
    ++roots;
    const PaintPropertyNode& unvisited_root = RootNode(*n.Get());
    CHECK(!visited.Contains(&unvisited_root));
    if (roots == 1 && unvisited_root.DebugName().empty()) {
      const_cast<PaintPropertyNode*>(&unvisited_root)->SetDebugName("root");
    } else if (roots == 2) {  // Only need to print disconnected subtrees once.
      string_builder.Append("disconnected trees:\n");
    }
    BuildTreeString(string_builder, unvisited_root, 0, visited);
  }

  return string_builder.ToString();
}

String PropertyTreePrinter::PathAsString(const PaintPropertyNode& last_node) {
  for (const auto* n = &last_node; n; n = n->Parent()) {
    wtf_size_t old_size = nodes_.size();
    AddNode(n);
    if (nodes_.size() == old_size) {
      break;
    }
  }
  return NodesAsTreeString();
}

void PropertyTreePrinter::BuildTreeString(
    StringBuilder& string_builder,
    const PaintPropertyNode& node,
    unsigned indent,
    HeapHashSet<Member<const PaintPropertyNode>>& visited) {
  for (unsigned i = 0; i < indent; i++) {
    string_builder.Append(' ');
  }

  visited.insert(&node);

  string_builder.Append(node.DebugName());
  string_builder.Append(String::Format(" %p ", &node));
  auto json = node.ToJSON();
  json->Remove("parent");
  string_builder.Append(json->ToJSONString());
  string_builder.Append("\n");

  for (const auto& child_node : nodes_) {
    if (child_node->Parent() == &node) {
      if (visited.Contains(child_node)) {
        for (unsigned i = 0; i < indent + 2; i++) {
          string_builder.Append(' ');
        }
        string_builder.Append(child_node->DebugName());
        string_builder.Append(String::Format(" %p [LOOP]\n", child_node.Get()));
      } else {
        BuildTreeString(string_builder, *child_node, indent + 2, visited);
      }
    }
  }
}

const PaintPropertyNode& PropertyTreePrinter::RootNode(
    const PaintPropertyNode& start_node) {
  const PaintPropertyNode* root_node = &start_node;
  HeapHashSet<Member<const PaintPropertyNode>> path;
  while (!root_node->IsRoot() && !path.Contains(root_node)) {
    path.insert(root_node);
    root_node = root_node->Parent();
  }
  return *root_node;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
