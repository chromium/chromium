// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_frame.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

void StyleScopeActivation::Trace(blink::Visitor* visitor) const {
  visitor->Trace(root);
}

StyleScopeFrame* StyleScopeFrame::GetParentFrameOrNull(
    Element& parent_element) {
  if (parent_ && (&parent_->element_ == &parent_element)) {
    return parent_;
  }
  return nullptr;
}

StyleScopeFrame& StyleScopeFrame::GetParentFrameOrThis(
    Element& parent_element) {
  StyleScopeFrame* parent_frame = GetParentFrameOrNull(parent_element);
  return parent_frame ? *parent_frame : *this;
}

}  // namespace blink
