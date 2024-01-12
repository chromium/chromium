// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift_attribution.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
LayoutShiftAttribution* LayoutShiftAttribution::Create(
    Node* node,
    DOMRectReadOnly* previous,
    DOMRectReadOnly* current) {
  return MakeGarbageCollected<LayoutShiftAttribution>(node, previous, current);
}

LayoutShiftAttribution::LayoutShiftAttribution(Node* node,
                                               DOMRectReadOnly* previous,
                                               DOMRectReadOnly* current)
    : node_(node), previous_rect_(previous), current_rect_(current) {}

LayoutShiftAttribution::~LayoutShiftAttribution() = default;

Node* LayoutShiftAttribution::node() const {
  return Performance::CanExposeNode(node_) ? node_ : nullptr;
}

Node* LayoutShiftAttribution::rawNodeForInspector() const {
  return node_.Get();
}

DOMRectReadOnly* LayoutShiftAttribution::previousRect() const {
  return previous_rect_.Get();
}

DOMRectReadOnly* LayoutShiftAttribution::currentRect() const {
  return current_rect_.Get();
}

ScriptValue LayoutShiftAttribution::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.Add("previousRect", previous_rect_.Get());
  builder.Add("currentRect", current_rect_.Get());
  return builder.GetScriptValue();
}

void LayoutShiftAttribution::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(previous_rect_);
  visitor->Trace(current_rect_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
