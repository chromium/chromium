// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/text_detector.h"

#include <utility>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_text.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

TextDetector* TextDetector::Create(ExecutionContext* context) {
  return MakeGarbageCollected<TextDetector>(context);
}

TextDetector::TextDetector(ExecutionContext* context) : text_service_(context) {
  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  context->GetBrowserInterfaceBroker().GetInterface(
      text_service_.BindNewPipeAndPassReceiver(task_runner));

  text_service_.set_disconnect_handler(WTF::BindOnce(
      &TextDetector::OnTextServiceConnectionError, WrapWeakPersistent(this)));
}

ScriptPromise<IDLSequence<DetectedText>> TextDetector::detect(
    ScriptState* script_state,
    const V8ImageBitmapSource* image_source,
    ExceptionState& exception_state) {
  std::optional<SkBitmap> bitmap =
      GetBitmapFromSource(script_state, image_source, exception_state);
  if (!bitmap) {
    return ScriptPromise<IDLSequence<DetectedText>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<DetectedText>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (bitmap->isNull()) {
    resolver->Resolve(HeapVector<Member<DetectedText>>());
    return promise;
  }
  if (!text_service_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     "Text detection service unavailable.");
    return promise;
  }
  text_service_requests_.insert(resolver);
  text_service_->Detect(
      std::move(*bitmap),
      WTF::BindOnce(&TextDetector::OnDetectText, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

void TextDetector::OnDetectText(
    ScriptPromiseResolver<IDLSequence<DetectedText>>* resolver,
    Vector<shape_detection::mojom::blink::TextDetectionResultPtr>
        text_detection_results) {
  DCHECK(text_service_requests_.Contains(resolver));
  text_service_requests_.erase(resolver);

  HeapVector<Member<DetectedText>> results;
  for (const auto& text : text_detection_results) {
    HeapVector<Member<Point2D>> corner_points;
    for (const auto& corner_point : text->corner_points) {
      Point2D* point = Point2D::Create();
      point->setX(corner_point.x());
      point->setY(corner_point.y());
      corner_points.push_back(point);
    }

    DetectedText* detected_text = DetectedText::Create();
    detected_text->setRawValue(text->raw_value);
    detected_text->setBoundingBox(DOMRectReadOnly::Create(
        text->bounding_box.x(), text->bounding_box.y(),
        text->bounding_box.width(), text->bounding_box.height()));
    detected_text->setCornerPoints(corner_points);
    results.push_back(detected_text);
  }

  resolver->Resolve(results);
}

void TextDetector::OnTextServiceConnectionError() {
  for (const auto& request : text_service_requests_) {
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
        "Text Detection not implemented."));
  }
  text_service_requests_.clear();
  text_service_.reset();
}

void TextDetector::Trace(Visitor* visitor) const {
  ShapeDetector::Trace(visitor);
  visitor->Trace(text_service_);
  visitor->Trace(text_service_requests_);
}

}  // namespace blink
