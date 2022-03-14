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

namespace blink {

bool DocumentLayoutUpgrade::ShouldUpgrade() {
  return document_.GetStyleEngine().StyleMayRequireLayout();
}

bool ParentLayoutUpgrade::ShouldUpgrade() {
  return document_.GetStyleEngine().HasViewportDependentMediaQueries();
}

NodeLayoutUpgrade::Reasons NodeLayoutUpgrade::GetReasons(const Node& node) {
  Reasons reasons = 0;

  const ComputedStyle* style =
      ComputedStyle::NullifyEnsured(node.GetComputedStyle());
  if (style && style->DependsOnContainerQueries())
    reasons |= kDependsOnContainerQueries;
  if (const auto* element = DynamicTo<Element>(node)) {
    ContainerQueryData* data = element->GetContainerQueryData();
    if (data && data->SkippedStyleRecalc()) {
      // We can only skip style recalc on interleaving roots, so if we did
      // skip, this is definitely an interleaving root.
      reasons |= (kSkippedStyleRecalc | kInterleavingRoot);
    } else if (ComputedStyle::IsInterleavingRoot(node.GetComputedStyle())) {
      // However, we are not guaranteed to actually skip on *all* interleaving
      // roots. (See `Element::SkipStyleRecalcForContainer`).
      reasons |= kInterleavingRoot;
    }
  }
  return reasons;
}

bool NodeLayoutUpgrade::ShouldUpgrade() {
  if (!node_.GetDocument().GetStyleEngine().StyleMayRequireLayout())
    return false;

  Reasons mask = kDependsOnContainerQueries | kSkippedStyleRecalc;

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
