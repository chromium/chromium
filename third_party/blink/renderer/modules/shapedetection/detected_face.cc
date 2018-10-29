// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/detected_face.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"

namespace blink {

DetectedFace* DetectedFace::Create() {
  return new DetectedFace(DOMRectReadOnly::Create(0, 0, 0, 0));
}

DetectedFace* DetectedFace::Create(DOMRectReadOnly* bounding_box) {
  return new DetectedFace(bounding_box);
}

DetectedFace* DetectedFace::Create(DOMRectReadOnly* bounding_box,
                                   const HeapVector<Landmark>& landmarks) {
  return new DetectedFace(bounding_box, landmarks);
}

DOMRectReadOnly* DetectedFace::boundingBox() const {
  return bounding_box_.Get();
}

const HeapVector<Landmark>& DetectedFace::landmarks() const {
  return landmarks_;
}

DetectedFace::DetectedFace(DOMRectReadOnly* bounding_box)
    : bounding_box_(bounding_box) {}

DetectedFace::DetectedFace(DOMRectReadOnly* bounding_box,
                           const HeapVector<Landmark>& landmarks)
    : bounding_box_(bounding_box), landmarks_(landmarks) {}

ScriptValue DetectedFace::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.Add("boundingBox", boundingBox()->toJSONForBinding(script_state));
  Vector<ScriptValue> landmarks;
  for (const auto& landmark : landmarks_) {
    V8ObjectBuilder landmark_builder(script_state);
    landmark_builder.AddString("type", landmark.type());
    Vector<ScriptValue> locations;
    for (const auto& location : landmark.locations()) {
      V8ObjectBuilder location_builder(script_state);
      location_builder.AddNumber("x", location.x());
      location_builder.AddNumber("y", location.y());
      locations.push_back(location_builder.GetScriptValue());
    }
    landmark_builder.Add("locations", locations);
    landmarks.push_back(landmark_builder.GetScriptValue());
  }
  result.Add("landmarks", landmarks);
  return result.GetScriptValue();
}

void DetectedFace::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounding_box_);
  visitor->Trace(landmarks_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
