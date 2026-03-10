// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& start_element,
    PseudoId pseudo_id) {
  StyleRecalcContext result;
  for (Element* element = &start_element; element;
       element = FlatTreeTraversal::ParentElement(*element)) {
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (result.size_container == nullptr &&
          style->IsContainerForSizeContainerQueries() &&
          (element != start_element ||
           !PseudoElement::IsLayoutSiblingOfOriginatingElement(start_element,
                                                               pseudo_id))) {
        // TODO(crbug.com/40250356): Eliminate all invalid calls to
        // StyleRecalcContext::From[Inclusive]Ancestors, then either turn
        // if (!style) into CHECK(style) or simplify into checking:
        // element->GetComputedStyle()->IsContainerForSizeContainerQueries()
        //
        // This used to use base::debug::DumpWithoutCrashing() but generated too
        // many failures in the wild to keep around (would upload too many crash
        // reports). Consider adding UMA stats back if we want to track this or
        // land a strategy to figure it out and fix what's going on.
        result.size_container = element;
      }
      if (!result.has_scroller_ancestor_with_scroll_marker_group_property &&
          !style->ScrollMarkerGroupNone() &&
          (style->IsScrollContainer() || element->IsDocumentElement())) {
        result.has_scroller_ancestor_with_scroll_marker_group_property = true;
      }
      if (!result.has_anchored_container) {
        result.has_anchored_container =
            style->IsContainerForAnchoredContainerQueries();
      }
      if (style->IsInternalOverscrollAreaAuto()) {
        result.overscroll_container = element;
      }
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
    return FromInclusiveAncestors(*parent, element.GetPseudoId());
  }
  return StyleRecalcContext();
}

StyleRecalcContext StyleRecalcContext::FromPseudoElementAncestors(
    Element& originating_element,
    PseudoId pseudo_id) {
  CHECK(pseudo_id != kPseudoIdNone);
  return FromInclusiveAncestors(originating_element, pseudo_id);
}

StyleRecalcContext StyleRecalcContext::FromParentContext(
    const StyleRecalcContext& parent_context,
    Element& element) {
  StyleRecalcContext result = parent_context;
  // If we're in StyleEngine::UpdateStyleAndLayoutTreeForOutOfFlow, then
  // anchor_evaluator may be non-nullptr to allow evaluation of anchor() and
  // anchor-size() queries, and the try sets may be non-nullptr if we're
  // attempting some position option [1]. These are only supposed to apply to
  // the interleaving root itself (i.e. the out-of-flow element being laid out),
  // and not to descendants.
  //
  // [1] https://drafts.csswg.org/css-anchor-position-1/#fallback
  result.anchor_evaluator = nullptr;
  result.try_set = nullptr;
  result.try_tactics_set = nullptr;

  if (!result.has_content_visibility_auto_locked_ancestor) {
    if (const DisplayLockContext* display_lock_context =
            element.GetDisplayLockContext()) {
      if (display_lock_context->IsAuto() && display_lock_context->IsLocked()) {
        result.has_content_visibility_auto_locked_ancestor = true;
      }
    }
  }

  if (const ComputedStyle* style = element.GetComputedStyle()) {
    result.has_scroller_ancestor_with_scroll_marker_group_property |=
        (style->IsScrollContainer() || element.IsDocumentElement()) &&
        !style->ScrollMarkerGroupNone();
    if (style->IsContainerForSizeContainerQueries()) {
      result.size_container = &element;
    }
    if (style->IsContainerForAnchoredContainerQueries()) {
      result.has_anchored_container = true;
    }

    if (style->IsInternalOverscrollAreaAuto()) {
      result.overscroll_container = &element;
    }
  }

  if (!result.has_animating_ancestor && element.GetElementAnimations()) {
    result.has_animating_ancestor = true;
  }

  return result;
}

}  // namespace blink
