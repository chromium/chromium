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
  static Element* getElementById(Document& document,
                                 const MaybeAtomicString& id) {
    // If id is null, it indicates that the id does not currently exist. Hence
    // the corresponding element should not exist, and we can return null.
    if (id.IsNull())
      return nullptr;
    return document.getElementById(id.ToAtomicString());
  }

  static Element* getElementById(DocumentFragment& fragment,
                                 const MaybeAtomicString& id) {
    // If id is null, it indicates that the id does not currently exist. Hence
    // the corresponding element should not exist, and we can return null.
    if (id.IsNull())
      return nullptr;
    return fragment.getElementById(id.ToAtomicString());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NON_ELEMENT_PARENT_NODE_H_
