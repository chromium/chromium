// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_pseudo_element.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

SkeletonPseudoElement::SkeletonPseudoElement(Element* originating_element)
    : PseudoElement(originating_element, kPseudoIdSkeleton) {
  CHECK_EQ(originating_element,
           originating_element->GetDocument().documentElement());
}

bool SkeletonPseudoElement::LayoutObjectIsNeeded(const DisplayStyle&) const {
  // The ::skeleton pseudo element does not generate a box. It is a container
  // for the skeleton document tree inside its UA shadow root. The skeleton root
  // element under the shadow root will generate a box whose parent box is the
  // LayoutView.
  return false;
}

const ComputedStyle* SkeletonPseudoElement::CustomStyleForLayoutObject(
    const StyleRecalcContext&) {
  // Use initial styles without inheriting from the originating element so that
  // any implicitly or explicitly inherited properties in the root element of
  // the shadow tree will inherit the initial value.
  return GetDocument().GetStyleResolver().InitialStyleForElement();
}

void SkeletonPseudoElement::AttachLayoutTree(AttachContext& context) {
  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    AttachContext children_context(context);
    children_context.parent = GetDocument().GetLayoutView();
    shadow_root->AttachLayoutTree(context);
  }
  Node::AttachLayoutTree(context);
  ClearChildNeedsReattachLayoutTree();
}

void SkeletonPseudoElement::DetachLayoutTree(bool performing_reattach) {
  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    shadow_root->DetachLayoutTree(performing_reattach);
  }
  Node::DetachLayoutTree(performing_reattach);
}

}  // namespace blink
