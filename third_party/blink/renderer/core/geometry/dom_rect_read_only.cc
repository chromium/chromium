// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

DOMRectReadOnly* DOMRectReadOnly::Create(double x,
                                         double y,
                                         double width,
                                         double height) {
  return MakeGarbageCollected<DOMRectReadOnly>(x, y, width, height);
}

ScriptValue DOMRectReadOnly::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("x", x());
  result.AddNumber("y", y());
  result.AddNumber("width", width());
  result.AddNumber("height", height());
  result.AddNumber("top", top());
  result.AddNumber("right", right());
  result.AddNumber("bottom", bottom());
  result.AddNumber("left", left());
  return result.GetScriptValue();
}

DOMRectReadOnly* DOMRectReadOnly::FromRect(const gfx::Rect& rect) {
  return MakeGarbageCollected<DOMRectReadOnly>(rect.x(), rect.y(), rect.width(),
                                               rect.height());
}

DOMRectReadOnly* DOMRectReadOnly::FromRectF(const gfx::RectF& rect) {
  return MakeGarbageCollected<DOMRectReadOnly>(rect.x(), rect.y(), rect.width(),
                                               rect.height());
}

DOMRectReadOnly* DOMRectReadOnly::fromRect(const DOMRectInit* other) {
  return MakeGarbageCollected<DOMRectReadOnly>(other->x(), other->y(),
                                               other->width(), other->height());
}

DOMRectReadOnly::DOMRectReadOnly(double x,
                                 double y,
                                 double width,
                                 double height)
    : x_(x), y_(y), width_(width), height_(height) {}

}  // namespace blink
