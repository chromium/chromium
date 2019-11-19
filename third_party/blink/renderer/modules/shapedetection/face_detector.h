// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_FACE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_FACE_DETECTOR_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/public/mojom/facedetection.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"

namespace blink {

class ExecutionContext;
class FaceDetectorOptions;

class MODULES_EXPORT FaceDetector final : public ShapeDetector {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FaceDetector* Create(ExecutionContext*, const FaceDetectorOptions*);

  FaceDetector(ExecutionContext*, const FaceDetectorOptions*);

  void Trace(blink::Visitor*) override;

 private:
  ~FaceDetector() override = default;

  ScriptPromise DoDetect(ScriptPromiseResolver*, SkBitmap) override;
  void OnDetectFaces(
      ScriptPromiseResolver*,
      Vector<shape_detection::mojom::blink::FaceDetectionResultPtr>);
  void OnFaceServiceConnectionError();

  mojo::Remote<shape_detection::mojom::blink::FaceDetection> face_service_;

  HeapHashSet<Member<ScriptPromiseResolver>> face_service_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_FACE_DETECTOR_H_
