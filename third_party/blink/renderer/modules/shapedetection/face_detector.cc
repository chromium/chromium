// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/face_detector.h"

#include <utility>

#include "services/shape_detection/public/mojom/facedetection_provider.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_face.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_face_detector_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_landmark.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/shapedetection/shape_detection_type_converter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FaceDetector* FaceDetector::Create(ExecutionContext* context,
                                   const FaceDetectorOptions* options) {
  return MakeGarbageCollected<FaceDetector>(context, options);
}

FaceDetector::FaceDetector(ExecutionContext* context,
                           const FaceDetectorOptions* options)
    : face_service_(context) {
  auto face_detector_options =
      shape_detection::mojom::blink::FaceDetectorOptions::New();
  face_detector_options->max_detected_faces = options->maxDetectedFaces();
  face_detector_options->fast_mode = options->fastMode();

  mojo::Remote<shape_detection::mojom::blink::FaceDetectionProvider> provider;
  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  context->GetBrowserInterfaceBroker().GetInterface(
      provider.BindNewPipeAndPassReceiver(task_runner));

  provider->CreateFaceDetection(
      face_service_.BindNewPipeAndPassReceiver(task_runner),
      std::move(face_detector_options));

  face_service_.set_disconnect_handler(WTF::BindOnce(
      &FaceDetector::OnFaceServiceConnectionError, WrapWeakPersistent(this)));
}

ScriptPromise<IDLSequence<DetectedFace>> FaceDetector::detect(
    ScriptState* script_state,
    const V8ImageBitmapSource* image_source,
    ExceptionState& exception_state) {
  std::optional<SkBitmap> bitmap =
      GetBitmapFromSource(script_state, image_source, exception_state);
  if (!bitmap) {
    return ScriptPromise<IDLSequence<DetectedFace>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<DetectedFace>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (bitmap->isNull()) {
    resolver->Resolve(HeapVector<Member<DetectedFace>>());
    return promise;
  }

  if (!face_service_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     "Face detection service unavailable.");
    return promise;
  }
  face_service_requests_.insert(resolver);
  face_service_->Detect(
      std::move(*bitmap),
      WTF::BindOnce(&FaceDetector::OnDetectFaces, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

void FaceDetector::OnDetectFaces(
    ScriptPromiseResolver<IDLSequence<DetectedFace>>* resolver,
    Vector<shape_detection::mojom::blink::FaceDetectionResultPtr>
        face_detection_results) {
  DCHECK(face_service_requests_.Contains(resolver));
  face_service_requests_.erase(resolver);

  HeapVector<Member<DetectedFace>> detected_faces;
  for (const auto& face : face_detection_results) {
    HeapVector<Member<Landmark>> landmarks;
    for (const auto& landmark : face->landmarks) {
      HeapVector<Member<Point2D>> locations;
      for (const auto& location : landmark->locations) {
        Point2D* web_location = Point2D::Create();
        web_location->setX(location.x());
        web_location->setY(location.y());
        locations.push_back(web_location);
      }

      Landmark* web_landmark = Landmark::Create();
      web_landmark->setLocations(locations);
      web_landmark->setType(mojo::ConvertTo<String>(landmark->type));
      landmarks.push_back(web_landmark);
    }

    DetectedFace* detected_face = DetectedFace::Create();
    detected_face->setBoundingBox(DOMRectReadOnly::Create(
        face->bounding_box.x(), face->bounding_box.y(),
        face->bounding_box.width(), face->bounding_box.height()));
    detected_face->setLandmarks(landmarks);
    detected_faces.push_back(detected_face);
  }

  resolver->Resolve(detected_faces);
}

void FaceDetector::OnFaceServiceConnectionError() {
  for (const auto& request : face_service_requests_) {
    // Check if callback's resolver is still valid.
    if (!IsInParallelAlgorithmRunnable(request->GetExecutionContext(),
                                       request->GetScriptState())) {
      continue;
    }
    // Enter into resolver's context to support creating DOMException.
    ScriptState::Scope script_state_scope(request->GetScriptState());

    request->Reject(V8ThrowDOMException::CreateOrDie(
        request->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotSupportedError,
        "Face Detection not implemented."));
  }
  face_service_requests_.clear();
  face_service_.reset();
}

void FaceDetector::Trace(Visitor* visitor) const {
  ShapeDetector::Trace(visitor);
  visitor->Trace(face_service_);
  visitor->Trace(face_service_requests_);
}

}  // namespace blink
