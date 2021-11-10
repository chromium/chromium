// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_ELEMENT_PARENT_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_ELEMENT_PARENT_NODE_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"

namespace blink {

class NonElementParentNode {
 public:
  static Element* getElementById(Document& document, const AtomicString& id) {
    return document.getElementById(id);
  }

  static Element* getElementById(DocumentFragment& fragment,
                                 const AtomicString& id) {
    return fragment.getElementById(id);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_ELEMENT_PARENT_NODE_H_
