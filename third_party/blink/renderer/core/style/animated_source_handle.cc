// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/animated_source_handle.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

AnimatedSourceHandle AnimatedSourceHandle::ForElement(Element* element) {
  if (!element) {
    return AnimatedSourceHandle();
  }
  return AnimatedSourceHandle(DOMNodeIds::IdForNode(element));
}

bool AnimatedSourceHandle::IsOwnedBy(const Element& element) const {
  return IsValid() && DOMNodeIds::ExistingIdForNode(&element) == animator_;
}

}  // namespace blink
