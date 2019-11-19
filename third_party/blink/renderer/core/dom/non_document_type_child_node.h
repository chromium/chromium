// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_DOCUMENT_TYPE_CHILD_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_DOCUMENT_TYPE_CHILD_NODE_H_

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NonDocumentTypeChildNode {
  STATIC_ONLY(NonDocumentTypeChildNode);

 public:
  static Element* previousElementSibling(Node& node) {
    return ElementTraversal::PreviousSibling(node);
  }

  static Element* nextElementSibling(Node& node) {
    return ElementTraversal::NextSibling(node);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_DOCUMENT_TYPE_CHILD_NODE_H_
