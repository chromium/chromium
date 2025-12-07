// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink::paint_timing {

// Returns the `Node` causing the image to be generated. For pseudo elements,
// this is the parent node; for all other nodes, this is the `node` itself.
inline CORE_EXPORT Node* ImageGeneratingNode(Node* node) {
  return node && node->IsPseudoElement() ? node->ParentOrShadowHostNode()
                                         : node;
}

// Returns true if `object` will cause an image to be rendered, and false
// otherwise.
inline bool CORE_EXPORT IsImageType(const LayoutObject& object) {
  return object.IsImage() || object.IsSVGImage() || object.IsVideo() ||
         object.StyleRef().HasBackgroundImage();
}

// Returns true if `node` is considered text, and false otherwise.
inline bool CORE_EXPORT IsTextType(const Node& node) {
  return node.IsTextNode();
}

}  // namespace blink::paint_timing

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_
