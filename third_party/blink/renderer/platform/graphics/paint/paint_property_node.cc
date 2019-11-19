// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

namespace {

// Returns -1 if |maybe_ancestor| is found in the ancestor chain, or returns
// the depth of the node from the root.
template <typename NodeType>
int NodeDepthOrFoundAncestor(const NodeType& node,
                             const NodeType& maybe_ancestor) {
  int depth = 0;
  for (const NodeType* n = &node; n; n = n->Parent()) {
    if (n == &maybe_ancestor)
      return -1;
    depth++;
  }
  return depth;
}

template <typename NodeType>
const NodeType& LowestCommonAncestorTemplate(const NodeType& a,
                                             const NodeType& b) {
  // Measure both depths.
  auto depth_a = NodeDepthOrFoundAncestor(a, b);
  if (depth_a == -1)
    return b;
  auto depth_b = NodeDepthOrFoundAncestor(b, a);
  if (depth_b == -1)
    return a;

  const auto* a_ptr = &a;
  const auto* b_ptr = &b;

  // Make it so depthA >= depthB.
  if (depth_a < depth_b) {
    std::swap(a_ptr, b_ptr);
    std::swap(depth_a, depth_b);
  }

  // Make it so depthA == depthB.
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

}  // namespace

const TransformPaintPropertyNode& LowestCommonAncestorInternal(
    const TransformPaintPropertyNode& a,
    const TransformPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const ClipPaintPropertyNode& LowestCommonAncestorInternal(
    const ClipPaintPropertyNode& a,
    const ClipPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const EffectPaintPropertyNode& LowestCommonAncestorInternal(
    const EffectPaintPropertyNode& a,
    const EffectPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
}

const ScrollPaintPropertyNode& LowestCommonAncestorInternal(
    const ScrollPaintPropertyNode& a,
    const ScrollPaintPropertyNode& b) {
  return LowestCommonAncestorTemplate(a, b);
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

}  // namespace blink
