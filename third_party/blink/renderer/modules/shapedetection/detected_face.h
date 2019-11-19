// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_FACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_FACE_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DOMRectReadOnly;
class Landmark;

class MODULES_EXPORT DetectedFace final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DetectedFace(DOMRectReadOnly*, const HeapVector<Member<Landmark>>&);

  DOMRectReadOnly* boundingBox() const { return bounding_box_; }
  const HeapVector<Member<Landmark>>& landmarks() const { return landmarks_; }

  void Trace(blink::Visitor*) override;

 private:
  const Member<DOMRectReadOnly> bounding_box_;
  const HeapVector<Member<Landmark>> landmarks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_FACE_H_
