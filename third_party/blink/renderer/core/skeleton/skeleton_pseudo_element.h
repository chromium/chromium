// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_

#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
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
class SkeletonPseudoElement : public PseudoElement {
 public:
  // The originating_element is always the HTML root element
  explicit SkeletonPseudoElement(Element* originating_element);

  void AttachLayoutTree(AttachContext&) final;
  void DidRecalcStyle(const StyleRecalcChange) final;

  const ComputedStyle* AdjustedLayoutStyle(
      const ComputedStyle& style,
      const ComputedStyle& layout_parent_style) final;

 private:
  // Return the used color-scheme for the skeleton root element
  mojom::blink::ColorScheme SkeletonUsedColorScheme();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_PSEUDO_ELEMENT_H_
