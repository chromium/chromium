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
    : node_(node),
      text_control_(EnclosingTextControl(node)),
      offset_(offset) {}

Node* CaretPosition::offsetNode() const {
  Node* node =
      text_control_ ? static_cast<Node*>(text_control_.Get()) : node_.Get();
  while (node) {
    ShadowRoot* shadow_root = node->ContainingShadowRoot();
    if (!shadow_root || !shadow_root->IsUserAgent()) {
      break;
    }
    node = &shadow_root->host();
  }
  return node;
}

unsigned CaretPosition::offset() const {
  if (text_control_) {
    if (!text_control_->IsTextControl()) {
      return 0;
    }
    return text_control_->IndexForPosition(Position(node_, offset_));
  }
  return offset_;
}

DOMRect* CaretPosition::getClientRect() const {
  if (!node_) {
    return nullptr;
  }
  if (text_control_) {
    HTMLElement* inner_editor = text_control_->IsTextControl()
                                    ? text_control_->InnerEditorElement()
                                    : nullptr;
    if (!inner_editor || !inner_editor->contains(node_)) {
      return nullptr;
    }
  }
  auto* range_object = MakeGarbageCollected<Range>(node_->GetDocument(), node_,
                                                   offset_, node_, offset_);
  return range_object->getBoundingClientRect();
}

void CaretPosition::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(text_control_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
