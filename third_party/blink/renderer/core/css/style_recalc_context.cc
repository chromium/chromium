// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

Element* ClosestInclusiveAncestorContainer(Element& element,
                                           Element* stay_within = nullptr) {
  for (auto* container = &element; container && container != stay_within;
       container = FlatTreeTraversal::ParentElement(*container)) {
    const ComputedStyle* style = container->GetComputedStyle();
    if (!style) {
      // TODO(crbug.com/1400631): Eliminate all invalid calls to
      // StyleRecalcContext::From[Inclusive]Ancestors, then either turn
      // if (!style) into CHECK(style) or simplify into checking:
      // container->GetComputedStyle()->IsContainerForSizeContainerQueries()
      //
      // This used to use base::debug::DumpWithoutCrashing() but generated too
      // many failures in the wild to keep around (would upload too many crash
      // reports). Consider adding UMA stats back if we want to track this or
      // land a strategy to figure it out and fix what's going on.
      return nullptr;
    }
    if (style->IsContainerForSizeContainerQueries()) {
      return container;
    }
  }
  return nullptr;
}

}  // namespace

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& element) {
  return StyleRecalcContext{ClosestInclusiveAncestorContainer(element)};
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container
  if (Element* parent = FlatTreeTraversal::ParentElement(element)) {
    return FromInclusiveAncestors(*parent);
  }
  return StyleRecalcContext();
}

}  // namespace blink
