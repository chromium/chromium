// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/barcode_detector.h"

#include <utility>

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"
#include "third_party/blink/renderer/modules/shapedetection/barcode_detector_options.h"
#include "third_party/blink/renderer/modules/shapedetection/barcode_detector_statics.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

shape_detection::mojom::blink::BarcodeFormat StringToBarcodeFormat(
    const WebString& format_string) {
  if (format_string == "aztec")
    return shape_detection::mojom::blink::BarcodeFormat::AZTEC;
  if (format_string == "code_128")
    return shape_detection::mojom::blink::BarcodeFormat::CODE_128;
  if (format_string == "code_39")
    return shape_detection::mojom::blink::BarcodeFormat::CODE_39;
  if (format_string == "code_93")
    return shape_detection::mojom::blink::BarcodeFormat::CODE_93;
  if (format_string == "codabar")
    return shape_detection::mojom::blink::BarcodeFormat::CODABAR;
  if (format_string == "data_matrix")
    return shape_detection::mojom::blink::BarcodeFormat::DATA_MATRIX;
  if (format_string == "ean_13")
    return shape_detection::mojom::blink::BarcodeFormat::EAN_13;
  if (format_string == "ean_8")
    return shape_detection::mojom::blink::BarcodeFormat::EAN_8;
  if (format_string == "itf")
    return shape_detection::mojom::blink::BarcodeFormat::ITF;
  if (format_string == "pdf417")
    return shape_detection::mojom::blink::BarcodeFormat::PDF417;
  if (format_string == "qr_code")
    return shape_detection::mojom::blink::BarcodeFormat::QR_CODE;
  if (format_string == "upc_a")
    return shape_detection::mojom::blink::BarcodeFormat::UPC_A;
  if (format_string == "upc_e")
    return shape_detection::mojom::blink::BarcodeFormat::UPC_E;
  return shape_detection::mojom::blink::BarcodeFormat::UNKNOWN;
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
    : ShapeDetector() {
  auto barcode_detector_options =
      shape_detection::mojom::blink::BarcodeDetectorOptions::New();

  if (options->hasFormats()) {
    // TODO(https://github.com/WICG/shape-detection-api/issues/66):
    // potentially process UNKNOWN as platform-specific formats.
    for (const auto& format_string : options->formats()) {
      auto format = StringToBarcodeFormat(format_string);
      if (format != shape_detection::mojom::blink::BarcodeFormat::UNKNOWN)
        barcode_detector_options->formats.push_back(format);
    }

    if (barcode_detector_options->formats.IsEmpty()) {
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
      WTF::Bind(&BarcodeDetector::OnConnectionError, WrapWeakPersistent(this)));
}

ScriptPromise BarcodeDetector::getSupportedFormats(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  return BarcodeDetectorStatics::From(context)->EnumerateSupportedFormats(
      script_state);
}

ScriptPromise BarcodeDetector::DoDetect(ScriptPromiseResolver* resolver,
                                        SkBitmap bitmap) {
  ScriptPromise promise = resolver->Promise();
  if (!service_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Barcode detection service unavailable."));
    return promise;
  }
  detect_requests_.insert(resolver);
  service_->Detect(std::move(bitmap),
                   WTF::Bind(&BarcodeDetector::OnDetectBarcodes,
                             WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetector::OnDetectBarcodes(
    ScriptPromiseResolver* resolver,
    Vector<shape_detection::mojom::blink::BarcodeDetectionResultPtr>
        barcode_detection_results) {
  DCHECK(detect_requests_.Contains(resolver));
  detect_requests_.erase(resolver);

  HeapVector<Member<DetectedBarcode>> detected_barcodes;
  for (const auto& barcode : barcode_detection_results) {
    HeapVector<Member<Point2D>> corner_points;
    for (const auto& corner_point : barcode->corner_points) {
      Point2D* point = Point2D::Create();
      point->setX(corner_point.x);
      point->setY(corner_point.y);
      corner_points.push_back(point);
    }
    detected_barcodes.push_back(MakeGarbageCollected<DetectedBarcode>(
        barcode->raw_value,
        DOMRectReadOnly::Create(
            barcode->bounding_box.x, barcode->bounding_box.y,
            barcode->bounding_box.width, barcode->bounding_box.height),
        barcode->format, corner_points));
  }

  resolver->Resolve(detected_barcodes);
}

void BarcodeDetector::OnConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolver>> resolvers;
  resolvers.swap(detect_requests_);
  for (const auto& resolver : resolvers) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Barcode Detection not implemented."));
  }
}

void BarcodeDetector::Trace(blink::Visitor* visitor) {
  ShapeDetector::Trace(visitor);
  visitor->Trace(detect_requests_);
}

}  // namespace blink
