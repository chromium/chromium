// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/barcode_detector_statics.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/shapedetection/barcode_detector.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

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
    : Supplement<ExecutionContext>(document), service_(&document) {}

BarcodeDetectorStatics::~BarcodeDetectorStatics() = default;

void BarcodeDetectorStatics::CreateBarcodeDetection(
    mojo::PendingReceiver<shape_detection::mojom::blink::BarcodeDetection>
        receiver,
    shape_detection::mojom::blink::BarcodeDetectorOptionsPtr options) {
  EnsureServiceConnection();
  service_->CreateBarcodeDetection(std::move(receiver), std::move(options));
}

ScriptPromise<IDLSequence<V8BarcodeFormat>>
BarcodeDetectorStatics::EnumerateSupportedFormats(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<V8BarcodeFormat>>>(
          script_state);
  auto promise = resolver->Promise();
  get_supported_format_requests_.insert(resolver);
  EnsureServiceConnection();
  service_->EnumerateSupportedFormats(
      WTF::BindOnce(&BarcodeDetectorStatics::OnEnumerateSupportedFormats,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void BarcodeDetectorStatics::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  visitor->Trace(service_);
  visitor->Trace(get_supported_format_requests_);
}

void BarcodeDetectorStatics::EnsureServiceConnection() {
  if (service_.is_bound())
    return;

  ExecutionContext* context = GetSupplementable();

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  context->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(WTF::BindOnce(
      &BarcodeDetectorStatics::OnConnectionError, WrapWeakPersistent(this)));
}

void BarcodeDetectorStatics::OnEnumerateSupportedFormats(
    ScriptPromiseResolver<IDLSequence<V8BarcodeFormat>>* resolver,
    const Vector<shape_detection::mojom::blink::BarcodeFormat>& formats) {
  DCHECK(get_supported_format_requests_.Contains(resolver));
  get_supported_format_requests_.erase(resolver);

  Vector<WTF::String> results;
  results.ReserveInitialCapacity(results.size());
  for (const auto& format : formats)
    results.push_back(BarcodeDetector::BarcodeFormatToString(format));
  if (IdentifiabilityStudySettings::Get()->ShouldSampleWebFeature(
          WebFeature::kBarcodeDetector_GetSupportedFormats)) {
    IdentifiableTokenBuilder builder;
    for (const auto& format_string : results)
      builder.AddToken(IdentifiabilityBenignStringToken(format_string));

    ExecutionContext* context = GetSupplementable();
    IdentifiabilityMetricBuilder(context->UkmSourceID())
        .AddWebFeature(WebFeature::kBarcodeDetector_GetSupportedFormats,
                       builder.GetToken())
        .Record(context->UkmRecorder());
  }
  resolver->Resolve(std::move(results));
}

void BarcodeDetectorStatics::OnConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolver<IDLSequence<V8BarcodeFormat>>>>
      resolvers;
  resolvers.swap(get_supported_format_requests_);
  for (const auto& resolver : resolvers) {
    // Return an empty list to indicate that no barcode formats are supported
    // since this connection failure indicates barcode detection is, in general,
    // not supported by the platform.
    resolver->Resolve(Vector<String>());
  }
}

}  // namespace blink
