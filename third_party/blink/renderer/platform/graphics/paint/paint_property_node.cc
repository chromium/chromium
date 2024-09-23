// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

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
  return debug_name_ + String::Format(" %p ", this) + s;
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
    case PaintPropertyChangeType::kChangedOnlyNonRerasterValues:
      return "non-reraster";
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
  BuildTreeString(string_builder, RootNode(), 0);
  return string_builder.ToString();
}

String PropertyTreePrinter::PathAsString(const PaintPropertyNode& last_node) {
  for (const auto* n = &last_node; n; n = n->Parent()) {
    AddNode(n);
  }
  return NodesAsTreeString();
}

void PropertyTreePrinter::BuildTreeString(StringBuilder& string_builder,
                                          const PaintPropertyNode& node,
                                          unsigned indent) {
  for (unsigned i = 0; i < indent; i++) {
    string_builder.Append(' ');
  }

  string_builder.Append(node.DebugName());
  string_builder.Append(String::Format(" %p ", &node));
  auto json = node.ToJSON();
  json->Remove("parent");
  string_builder.Append(json->ToJSONString());
  string_builder.Append("\n");

  for (const auto& child_node : nodes_) {
    if (child_node->Parent() == &node) {
      BuildTreeString(string_builder, *child_node, indent + 2);
    }
  }
}

const PaintPropertyNode& PropertyTreePrinter::RootNode() {
  const auto* node = nodes_.back().Get();
  while (!node->IsRoot()) {
    node = node->Parent();
  }
  if (node->DebugName().empty()) {
    const_cast<PaintPropertyNode*>(node)->SetDebugName("root");
  }
  nodes_.insert(node);
  return *node;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
