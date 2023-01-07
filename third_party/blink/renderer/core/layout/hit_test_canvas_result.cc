// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"

namespace blink {

HitTestCanvasResult::HitTestCanvasResult(String id, Member<Element> control)
    : id_(id), control_(control) {}

String HitTestCanvasResult::GetId() const {
  return id_;
}

Element* HitTestCanvasResult::GetControl() const {
  return control_.Get();
}

void HitTestCanvasResult::Trace(Visitor* visitor) const {
  visitor->Trace(control_);
}

}  // namespace blink
