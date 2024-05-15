// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/caret_position.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

CaretPosition::CaretPosition(Node* node, unsigned offset)
    : node_(node), offset_(offset) {}

Node* CaretPosition::offsetNode() const {
  if (!node_) {
    return nullptr;
  }

  if (Node* text_control = EnclosingTextControl(node_)) {
    return text_control;
  }
  return node_;
}
unsigned CaretPosition::offset() const {
  return offset_;
}

DOMRect* CaretPosition::getClientRect() const {
  if (!node_) {
    return nullptr;
  }
  auto* range_object = MakeGarbageCollected<Range>(node_->GetDocument(), node_,
                                                   offset_, node_, offset_);
  return range_object->getBoundingClientRect();
}

void CaretPosition::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
