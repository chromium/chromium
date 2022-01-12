// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

namespace {

Element* ClosestInclusiveAncestorContainer(Element& element) {
  for (auto* container = &element; container;
       container = container->ParentOrShadowHostElement()) {
    if (container->GetContainerQueryEvaluator())
      return container;
  }
  return nullptr;
}

}  // namespace

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& element) {
  if (!RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
    return StyleRecalcContext();

  return StyleRecalcContext{ClosestInclusiveAncestorContainer(element)};
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  if (!RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
    return StyleRecalcContext();

  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container
  if (Element* shadow_including_parent = element.ParentOrShadowHostElement())
    return FromInclusiveAncestors(*shadow_including_parent);
  return StyleRecalcContext();
}

}  // namespace blink
