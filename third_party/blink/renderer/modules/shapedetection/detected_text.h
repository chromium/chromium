// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_TEXT_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMRectReadOnly;
class Point2D;

class MODULES_EXPORT DetectedText final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DetectedText(String, DOMRectReadOnly*, HeapVector<Member<Point2D>>);

  const String& rawValue() const { return raw_value_; }
  DOMRectReadOnly* boundingBox() const { return bounding_box_; }
  const HeapVector<Member<Point2D>>& cornerPoints() const {
    return corner_points_;
  }

  void Trace(blink::Visitor*) override;

 private:
  const String raw_value_;
  const Member<DOMRectReadOnly> bounding_box_;
  const HeapVector<Member<Point2D>> corner_points_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_DETECTED_TEXT_H_
