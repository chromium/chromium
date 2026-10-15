// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift_attribution.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
LayoutShiftAttribution* LayoutShiftAttribution::Create(
    Node* node,
    DOMRectReadOnly* previous,
    DOMRectReadOnly* current,
    DOMWindow* source) {
  return MakeGarbageCollected<LayoutShiftAttribution>(node, previous, current,
                                                      source);
}

LayoutShiftAttribution::LayoutShiftAttribution(Node* node,
                                               DOMRectReadOnly* previous,
                                               DOMRectReadOnly* current,
                                               DOMWindow* source)
    : node_(node),
      previous_rect_(previous),
      current_rect_(current),
      source_(source) {}

LayoutShiftAttribution::~LayoutShiftAttribution() = default;

Node* LayoutShiftAttribution::node() const {
  return Performance::CanExposeNode(node_, source_) ? node_ : nullptr;
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

ScriptObject LayoutShiftAttribution::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.Add("previousRect", previous_rect_.Get());
  builder.Add("currentRect", current_rect_.Get());
  return builder.ToScriptObject();
}

void LayoutShiftAttribution::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(previous_rect_);
  visitor->Trace(current_rect_);
  visitor->Trace(source_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
