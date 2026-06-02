// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

// A pseudo-element which hangs off of the HTML root element to hold a UA shadow
// tree with the rendered skeleton document.
//
// HTML
//   ::skeleton
//     ::shadow-root
//       HTML
//         HEAD
//         BODY
//   HEAD
//   BODY
//
// The generated box tree does not have a box for ::skeleton, and uses the main
// document LayoutView as the parent of the box for the HTML root of the shadow
// tree.
//
class SkeletonPseudoElement : public PseudoElement {
 public:
  // The originating_element is always the HTML root element
  explicit SkeletonPseudoElement(Element* originating_element);

  bool LayoutObjectIsNeeded(const DisplayStyle&) const final;

  void AttachLayoutTree(AttachContext&) final;
  void DetachLayoutTree(bool performing_reattach) final;

  const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext&) final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_
