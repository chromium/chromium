// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/barcode_detector.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_barcode_detector_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_barcode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/shapedetection/barcode_detector_statics.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

shape_detection::mojom::blink::BarcodeFormat IdlEnumToBarcodeFormat(
    V8BarcodeFormat format) {
  using IdlFormat = V8BarcodeFormat::Enum;
  using MojoFormat = shape_detection::mojom::blink::BarcodeFormat;
  switch (format.AsEnum()) {
    case IdlFormat::kAztec:
      return MojoFormat::AZTEC;
    case IdlFormat::kCode128:
      return MojoFormat::CODE_128;
    case IdlFormat::kCode39:
      return MojoFormat::CODE_39;
    case IdlFormat::kCode93:
      return MojoFormat::CODE_93;
    case IdlFormat::kCodabar:
      return MojoFormat::CODABAR;
    case IdlFormat::kDataMatrix:
      return MojoFormat::DATA_MATRIX;
    case IdlFormat::kEan13:
      return MojoFormat::EAN_13;
    case IdlFormat::kEan8:
      return MojoFormat::EAN_8;
    case IdlFormat::kItf:
      return MojoFormat::ITF;
    case IdlFormat::kPdf417:
      return MojoFormat::PDF417;
    case IdlFormat::kQrCode:
      return MojoFormat::QR_CODE;
    case IdlFormat::kUpcA:
      return MojoFormat::UPC_A;
    case IdlFormat::kUpcE:
      return MojoFormat::UPC_E;
    case IdlFormat::kUnknown:
      return MojoFormat::UNKNOWN;
  }
}

}  // anonymous namespace

BarcodeDetector* BarcodeDetector::Create(ExecutionContext* context,
                                         const BarcodeDetectorOptions* options,
                                         ExceptionState& exception_state) {
  return MakeGarbageCollected<BarcodeDetector>(context, options,
                                               exception_state);
}

BarcodeDetector::BarcodeDetector(ExecutionContext* context,
                                 const BarcodeDetectorOptions* options,
                                 ExceptionState& exception_state)
    : service_(context) {
  auto barcode_detector_options =
      shape_detection::mojom::blink::BarcodeDetectorOptions::New();

  if (options->hasFormats()) {
    // TODO(https://github.com/WICG/shape-detection-api/issues/66):
    // potentially process UNKNOWN as platform-specific formats.
    for (const auto& format_string : options->formats()) {
      auto format = IdlEnumToBarcodeFormat(format_string);
      if (format != shape_detection::mojom::blink::BarcodeFormat::UNKNOWN)
        barcode_detector_options->formats.push_back(format);
    }

    if (barcode_detector_options->formats.empty()) {
      exception_state.ThrowTypeError("Hint option provided, but is empty.");
      return;
    }
  }

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);

  BarcodeDetectorStatics::From(context)->CreateBarcodeDetection(
      service_.BindNewPipeAndPassReceiver(task_runner),
      std::move(barcode_detector_options));
  service_.set_disconnect_handler(
      BindOnce(&BarcodeDetector::OnConnectionError, WrapWeakPersistent(this)));
}

// static
ScriptPromise<IDLSequence<V8BarcodeFormat>>
BarcodeDetector::getSupportedFormats(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  return BarcodeDetectorStatics::From(context)->EnumerateSupportedFormats(
      script_state);
}

// static
V8BarcodeFormat::Enum BarcodeDetector::BarcodeFormatToEnum(
    const shape_detection::mojom::BarcodeFormat format) {
  switch (format) {
    case shape_detection::mojom::BarcodeFormat::AZTEC:
      return V8BarcodeFormat::Enum::kAztec;
    case shape_detection::mojom::BarcodeFormat::CODE_128:
      return V8BarcodeFormat::Enum::kCode128;
    case shape_detection::mojom::BarcodeFormat::CODE_39:
      return V8BarcodeFormat::Enum::kCode39;
    case shape_detection::mojom::BarcodeFormat::CODE_93:
      return V8BarcodeFormat::Enum::kCode93;
    case shape_detection::mojom::BarcodeFormat::CODABAR:
      return V8BarcodeFormat::Enum::kCodabar;
    case shape_detection::mojom::BarcodeFormat::DATA_MATRIX:
      return V8BarcodeFormat::Enum::kDataMatrix;
    case shape_detection::mojom::BarcodeFormat::EAN_13:
      return V8BarcodeFormat::Enum::kEan13;
    case shape_detection::mojom::BarcodeFormat::EAN_8:
      return V8BarcodeFormat::Enum::kEan8;
    case shape_detection::mojom::BarcodeFormat::ITF:
      return V8BarcodeFormat::Enum::kItf;
    case shape_detection::mojom::BarcodeFormat::PDF417:
      return V8BarcodeFormat::Enum::kPdf417;
    case shape_detection::mojom::BarcodeFormat::QR_CODE:
      return V8BarcodeFormat::Enum::kQrCode;
    case shape_detection::mojom::BarcodeFormat::UNKNOWN:
      return V8BarcodeFormat::Enum::kUnknown;
    case shape_detection::mojom::BarcodeFormat::UPC_A:
      return V8BarcodeFormat::Enum::kUpcA;
    case shape_detection::mojom::BarcodeFormat::UPC_E:
      return V8BarcodeFormat::Enum::kUpcE;
  }
}

ScriptPromise<IDLSequence<DetectedBarcode>> BarcodeDetector::detect(
    ScriptState* script_state,
    const V8ImageBitmapSource* image_source,
    ExceptionState& exception_state) {
  std::optional<SkBitmap> bitmap = GetBitmapFromV8ImageBitmapSource(
      script_state, image_source, exception_state);
  if (!bitmap) {
    return ScriptPromise<IDLSequence<DetectedBarcode>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<DetectedBarcode>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (bitmap->isNull()) {
    resolver->Resolve(HeapVector<Member<DetectedBarcode>>());
    return promise;
  }

  if (!service_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     "Barcode detection service unavailable.");
    return promise;
  }
  detect_requests_.insert(resolver);
  service_->Detect(std::move(*bitmap),
                   BindOnce(&BarcodeDetector::OnDetectBarcodes,
                            WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetector::OnDetectBarcodes(
    ScriptPromiseResolver<IDLSequence<DetectedBarcode>>* resolver,
    Vector<shape_detection::mojom::blink::BarcodeDetectionResultPtr>
        barcode_detection_results) {
  DCHECK(detect_requests_.Contains(resolver));
  detect_requests_.erase(resolver);

  HeapVector<Member<DetectedBarcode>> detected_barcodes;
  for (const auto& barcode : barcode_detection_results) {
    HeapVector<Member<Point2D>> corner_points;
    for (const auto& corner_point : barcode->corner_points) {
      Point2D* point = Point2D::Create();
      point->setX(corner_point.x());
      point->setY(corner_point.y());
      corner_points.push_back(point);
    }

    DetectedBarcode* detected_barcode = DetectedBarcode::Create();
    detected_barcode->setRawValue(barcode->raw_value);
    detected_barcode->setBoundingBox(DOMRectReadOnly::Create(
        barcode->bounding_box.x(), barcode->bounding_box.y(),
        barcode->bounding_box.width(), barcode->bounding_box.height()));
    detected_barcode->setFormat(BarcodeFormatToEnum(barcode->format));
    detected_barcode->setCornerPoints(corner_points);
    detected_barcodes.push_back(detected_barcode);
  }

  resolver->Resolve(detected_barcodes);
}

void BarcodeDetector::OnConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolverBase>> resolvers;
  resolvers.swap(detect_requests_);
  for (const auto& resolver : resolvers) {
    // Check if callback's resolver is still valid.
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver->GetScriptState())) {
      continue;
    }
    // Enter into resolver's context to support creating DOMException.
    ScriptState::Scope script_state_scope(resolver->GetScriptState());

    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotSupportedError,
        "Barcode Detection not implemented."));
  }
}

void BarcodeDetector::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(service_);
  visitor->Trace(detect_requests_);
}

}  // namespace blink
