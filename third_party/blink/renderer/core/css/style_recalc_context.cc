// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& element) {
  if (element.GetContainerQueryEvaluator())
    return StyleRecalcContext{&element};
  return FromAncestors(element);
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  Element* ancestor = &element;
  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container.
  while ((ancestor = DynamicTo<Element>(
              LayoutTreeBuilderTraversal::Parent(*ancestor)))) {
    if (ancestor->GetContainerQueryEvaluator())
      return StyleRecalcContext{ancestor};
  }

  return StyleRecalcContext();
}

}  // namespace blink
