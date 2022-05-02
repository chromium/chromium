// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/layout_upgrade.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"

namespace blink {

bool DocumentLayoutUpgrade::ShouldUpgrade() {
  StyleEngine& style_engine = document_.GetStyleEngine();
  return style_engine.SkippedContainerRecalc() ||
         style_engine.StyleAffectedByLayout();
}

bool ParentLayoutUpgrade::ShouldUpgrade() {
  return document_.GetStyleEngine().HasViewportDependentMediaQueries() ||
         NodeLayoutUpgrade(owner_).ShouldUpgrade();
}

NodeLayoutUpgrade::Reasons NodeLayoutUpgrade::GetReasons(const Node& node) {
  Reasons reasons = 0;

  const ComputedStyle* style =
      ComputedStyle::NullifyEnsured(node.GetComputedStyle());
  if (style && style->DependsOnContainerQueries())
    reasons |= kDependsOnContainerQueries;
  if (ComputedStyle::IsInterleavingRoot(node.GetComputedStyle()))
    reasons |= kInterleavingRoot;
  return reasons;
}

bool NodeLayoutUpgrade::ShouldUpgrade() {
  // We do not allow any elements to remain in a skipped state after a style
  // update, therefore we always upgrade whenever we've skipped something, even
  // if the current ancestors chain does not depend on layout.
  StyleEngine& style_engine = node_.GetDocument().GetStyleEngine();
  if (style_engine.SkippedContainerRecalc())
    return true;
  if (!style_engine.StyleAffectedByLayout())
    return false;

  Reasons mask = kDependsOnContainerQueries;

  if (GetReasons(node_) & mask)
    return true;

  // Whether or not `node_` depends on container queries is stored on its
  // `ComputedStyle`. If the node does not have a style, we defensively assume
  // that it *does* depend on container queries, and upgrade whenever we're
  // inside any interleaving root.
  if (ComputedStyle::IsNullOrEnsured(node_.GetComputedStyle()))
    mask |= NodeLayoutUpgrade::kInterleavingRoot;

  for (ContainerNode* ancestor = LayoutTreeBuilderTraversal::Parent(node_);
       ancestor; ancestor = LayoutTreeBuilderTraversal::Parent(*ancestor)) {
    if (GetReasons(*ancestor) & mask)
      return true;
  }

  return false;
}

}  // namespace blink
