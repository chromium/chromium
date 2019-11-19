// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/barcode_detector_statics.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"

namespace blink {

// static
const char BarcodeDetectorStatics::kSupplementName[] = "BarcodeDetectorStatics";

// static
BarcodeDetectorStatics* BarcodeDetectorStatics::From(
    ExecutionContext* document) {
  DCHECK(document);
  BarcodeDetectorStatics* statics =
      Supplement<ExecutionContext>::From<BarcodeDetectorStatics>(*document);
  if (!statics) {
    statics = MakeGarbageCollected<BarcodeDetectorStatics>(*document);
    Supplement<ExecutionContext>::ProvideTo(*document, statics);
  }
  return statics;
}

BarcodeDetectorStatics::BarcodeDetectorStatics(ExecutionContext& document)
    : Supplement<ExecutionContext>(document) {}

BarcodeDetectorStatics::~BarcodeDetectorStatics() = default;

void BarcodeDetectorStatics::CreateBarcodeDetection(
    mojo::PendingReceiver<shape_detection::mojom::blink::BarcodeDetection>
        receiver,
    shape_detection::mojom::blink::BarcodeDetectorOptionsPtr options) {
  EnsureServiceConnection();
  service_->CreateBarcodeDetection(std::move(receiver), std::move(options));
}

ScriptPromise BarcodeDetectorStatics::EnumerateSupportedFormats(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  get_supported_format_requests_.insert(resolver);
  EnsureServiceConnection();
  service_->EnumerateSupportedFormats(
      WTF::Bind(&BarcodeDetectorStatics::OnEnumerateSupportedFormats,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetectorStatics::Trace(Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
  visitor->Trace(get_supported_format_requests_);
}

void BarcodeDetectorStatics::EnsureServiceConnection() {
  if (service_)
    return;

  ExecutionContext* context = GetSupplementable();

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  context->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(WTF::Bind(
      &BarcodeDetectorStatics::OnConnectionError, WrapWeakPersistent(this)));
}

void BarcodeDetectorStatics::OnEnumerateSupportedFormats(
    ScriptPromiseResolver* resolver,
    const Vector<shape_detection::mojom::blink::BarcodeFormat>& formats) {
  DCHECK(get_supported_format_requests_.Contains(resolver));
  get_supported_format_requests_.erase(resolver);

  Vector<WTF::String> results;
  results.ReserveInitialCapacity(results.size());
  for (const auto& format : formats)
    results.push_back(DetectedBarcode::BarcodeFormatToString(format));
  resolver->Resolve(results);
}

void BarcodeDetectorStatics::OnConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolver>> resolvers;
  resolvers.swap(get_supported_format_requests_);
  for (const auto& resolver : resolvers) {
    // Return an empty list to indicate that no barcode formats are supported
    // since this connection failure indicates barcode detection is, in general,
    // not supported by the platform.
    resolver->Resolve(Vector<String>());
  }
}

}  // namespace blink
