// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& start_element) {
  StyleRecalcContext result;
  for (auto* element = &start_element; element;
       element = FlatTreeTraversal::ParentElement(*element)) {
    if (result.container == nullptr) {
      const ComputedStyle* style = element->GetComputedStyle();
      if (style && style->IsContainerForSizeContainerQueries()) {
        // TODO(crbug.com/40250356): Eliminate all invalid calls to
        // StyleRecalcContext::From[Inclusive]Ancestors, then either turn
        // if (!style) into CHECK(style) or simplify into checking:
        // element->GetComputedStyle()->IsContainerForSizeContainerQueries()
        //
        // This used to use base::debug::DumpWithoutCrashing() but generated too
        // many failures in the wild to keep around (would upload too many crash
        // reports). Consider adding UMA stats back if we want to track this or
        // land a strategy to figure it out and fix what's going on.
        result.container = element;
      }
    }

    const ComputedStyle* style = element->GetComputedStyle();
    if (style && !style->ScrollMarkerGroupNone() &&
        (style->IsScrollContainer() || element->IsDocumentElement())) {
      result.has_scroller_ancestor_with_scroll_marker_group_property = true;
    }

    if (!result.has_content_visibility_auto_locked_ancestor) {
      if (const DisplayLockContext* display_lock_context =
              element->GetDisplayLockContext()) {
        if (display_lock_context->IsAuto() &&
            display_lock_context->IsLocked()) {
          result.has_content_visibility_auto_locked_ancestor = true;
        }
      }
    }

    if (!result.has_animating_ancestor && element->GetElementAnimations()) {
      result.has_animating_ancestor = true;
    }
  }
  return result;
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  if (Element* parent = FlatTreeTraversal::ParentElement(element)) {
    return FromInclusiveAncestors(*parent);
  }
  return StyleRecalcContext();
}

}  // namespace blink
