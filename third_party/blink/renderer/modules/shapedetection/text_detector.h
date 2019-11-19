// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_TEXT_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_TEXT_DETECTOR_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/public/mojom/textdetection.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"

namespace blink {

class ExecutionContext;

class MODULES_EXPORT TextDetector final : public ShapeDetector {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextDetector* Create(ExecutionContext*);

  explicit TextDetector(ExecutionContext*);

  void Trace(blink::Visitor*) override;

 private:
  ~TextDetector() override = default;

  ScriptPromise DoDetect(ScriptPromiseResolver*, SkBitmap) override;
  void OnDetectText(
      ScriptPromiseResolver*,
      Vector<shape_detection::mojom::blink::TextDetectionResultPtr>);
  void OnTextServiceConnectionError();

  mojo::Remote<shape_detection::mojom::blink::TextDetection> text_service_;

  HeapHashSet<Member<ScriptPromiseResolver>> text_service_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_TEXT_DETECTOR_H_
