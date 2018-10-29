// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"

namespace blink {

DetectedBarcode* DetectedBarcode::Create() {
  HeapVector<Point2D> empty_list;
  return new DetectedBarcode(g_empty_string,
                             DOMRectReadOnly::Create(0, 0, 0, 0), empty_list);
}

DetectedBarcode* DetectedBarcode::Create(String raw_value,
                                         DOMRectReadOnly* bounding_box,
                                         HeapVector<Point2D> corner_points) {
  return new DetectedBarcode(raw_value, bounding_box, corner_points);
}

const String& DetectedBarcode::rawValue() const {
  return raw_value_;
}

DOMRectReadOnly* DetectedBarcode::boundingBox() const {
  return bounding_box_.Get();
}

const HeapVector<Point2D>& DetectedBarcode::cornerPoints() const {
  return corner_points_;
}

DetectedBarcode::DetectedBarcode(String raw_value,
                                 DOMRectReadOnly* bounding_box,
                                 HeapVector<Point2D> corner_points)
    : raw_value_(raw_value),
      bounding_box_(bounding_box),
      corner_points_(corner_points) {}

ScriptValue DetectedBarcode::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("rawValue", rawValue());
  result.Add("boundingBox", boundingBox()->toJSONForBinding(script_state));
  Vector<ScriptValue> corner_points;
  for (const auto& corner_point : corner_points_) {
    V8ObjectBuilder builder(script_state);
    builder.AddNumber("x", corner_point.x());
    builder.AddNumber("y", corner_point.y());
    corner_points.push_back(builder.GetScriptValue());
  }
  result.Add("cornerPoints", corner_points);
  return result.GetScriptValue();
}

void DetectedBarcode::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounding_box_);
  visitor->Trace(corner_points_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
