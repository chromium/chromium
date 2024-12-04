// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"

namespace blink {

void ColumnPseudoElement::AttachLayoutTree(AttachContext& context) {
  // A ::column element can not have a box, but it may have a ::scroll-marker
  // child.
  AttachPseudoElement(kPseudoIdScrollMarker, context);
  ContainerNode::AttachLayoutTree(context);
}

void ColumnPseudoElement::DetachLayoutTree(bool performing_reattach) {
  DetachPseudoElement(kPseudoIdScrollMarker, performing_reattach);
  ContainerNode::DetachLayoutTree(performing_reattach);
}

}  // namespace blink
