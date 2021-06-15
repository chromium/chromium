// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_reporting.h"

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/platform/impression_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_source_params.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/conversion_measurement_parsing.h"

namespace blink {

namespace {

void HandleRegisterImpressionError(
    ExceptionState& exception_state,
    mojom::blink::RegisterImpressionError error) {
  switch (error) {
    case mojom::blink::RegisterImpressionError::kNotAllowed:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "Not allowed.");
      break;
    case mojom::blink::RegisterImpressionError::kInsecureContext:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Cannot execute in an insecure context.");
      break;
    case mojom::blink::RegisterImpressionError::kInsecureAttributionDestination:
      exception_state.ThrowTypeError(
          "attributionDestination must be a trustworthy origin.");
      break;
    case mojom::blink::RegisterImpressionError::kInsecureAttributionReportTo:
      exception_state.ThrowTypeError(
          "attributionReportTo must be a trustworthy origin.");
      break;
    case mojom::blink::RegisterImpressionError::
        kInvalidAttributionSourceEventId:
      exception_state.ThrowTypeError(
          "attributionSourceEventId must be parsable as an unsigned 64-bit "
          "base-10 integer.");
      break;
  }
}

}  // namespace

// static
const char AttributionReporting::kSupplementName[] = "AttributionReporting";

// static
AttributionReporting& AttributionReporting::attributionReporting(
    LocalDOMWindow& window) {
  AttributionReporting* attribution_reporting =
      Supplement<LocalDOMWindow>::From<AttributionReporting>(window);
  if (!attribution_reporting) {
    attribution_reporting = MakeGarbageCollected<AttributionReporting>(window);
    Supplement<LocalDOMWindow>::ProvideTo<AttributionReporting>(
        window, attribution_reporting);
  }
  return *attribution_reporting;
}

AttributionReporting::AttributionReporting(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), conversion_host_(&window) {}

void AttributionReporting::Trace(Visitor* visitor) const {
  visitor->Trace(conversion_host_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

ScriptPromise AttributionReporting::registerAttributionSource(
    ScriptState* script_state,
    const AttributionSourceParams* params,
    ExceptionState& exception_state) {
  // The PermissionsPolicy check, etc., occurs in the call to
  // `GetImpressionForParams()`.
  WebImpressionOrError web_impression_or_err =
      GetImpressionForParams(GetSupplementable(), params);

  if (auto* err = absl::get_if<mojom::blink::RegisterImpressionError>(
          &web_impression_or_err)) {
    HandleRegisterImpressionError(exception_state, *err);
    return ScriptPromise();
  }

  const WebImpression* web_impression =
      absl::get_if<WebImpression>(&web_impression_or_err);
  DCHECK(web_impression);
  blink::Impression impression =
      ConvertWebImpressionToImpression(*web_impression);

  if (!conversion_host_.is_bound()) {
    GetSupplementable()
        ->GetFrame()
        ->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(conversion_host_.BindNewEndpointAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  conversion_host_->RegisterImpression(impression);
  return ScriptPromise::CastUndefined(script_state);
}

}  // namespace blink
