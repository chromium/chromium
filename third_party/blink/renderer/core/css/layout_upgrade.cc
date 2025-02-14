// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/layout_upgrade.h"

#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

bool DocumentLayoutUpgrade::ShouldUpgrade() {
  StyleEngine& style_engine = document_.GetStyleEngine();
  return style_engine.SkippedContainerRecalc() ||
         style_engine.StyleAffectedByLayout();
}

bool ParentLayoutUpgrade::ShouldUpgrade() {
  StyleEngine& style_engine = document_.GetStyleEngine();
  return style_engine.HasViewportDependentMediaQueries() ||
         style_engine.HasViewportDependentPropertyRegistrations() ||
         ElementLayoutUpgrade(owner_).ShouldUpgrade();
}

bool ElementLayoutUpgrade::ShouldUpgrade() {
  if (!element_.isConnected()) {
    return false;
  }
  // We do not allow any elements to remain in a skipped state after a style
  // update, therefore we always upgrade whenever we've skipped something, even
  // if the current ancestors chain does not depend on layout.
  StyleEngine& style_engine = element_.GetDocument().GetStyleEngine();
  if (style_engine.SkippedContainerRecalc()) {
    return true;
  }

  bool maybe_affected_by_layout =
      style_engine.StyleMaybeAffectedByLayout(element_);

  if (!maybe_affected_by_layout) {
    return false;
  }

  if (PostStyleUpdateScope::AnimationData* data =
          PostStyleUpdateScope::CurrentAnimationData()) {
    // If an old style is stored, it means a style recalc has happened for an
    // element which may need layout for computing the final style for the style
    // change event. An upgrade is required, otherwise the style change for the
    // element will be split across two style change events and observable
    // through transitions.
    if (data->HasOldStyles()) {
      return true;
    }
  }

  // For pseudo-style requests, we may have to update pseudo-elements of the
  // interleaving root itself. Hence we use inclusive ancestors here.
  for (const Element* ancestor = &element_; ancestor;
       ancestor = LayoutTreeBuilderTraversal::ParentElement(*ancestor)) {
    if (ComputedStyle::IsInterleavingRoot(ancestor->GetComputedStyle())) {
      return true;
    }
  }

  return false;
}

}  // namespace blink
