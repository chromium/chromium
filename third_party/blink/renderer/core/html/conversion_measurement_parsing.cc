// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/conversion_measurement_parsing.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_impression_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

base::Optional<WebImpression> GetImpression(
    ExecutionContext* execution_context,
    const String& impression_data_string,
    const String& conversion_destination_string,
    const base::Optional<String>& reporting_origin_string,
    base::Optional<uint64_t> impression_expiry_milliseconds) {
  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(execution_context))
    return base::nullopt;

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kConversionMeasurement)) {
    String message =
        "The 'conversion-measurement' feature policy must be enabled to "
        "declare an impression.";
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError, message));
    return base::nullopt;
  }

  // Conversion measurement is only allowed when both the frame and the main
  // frame (if different) have a secure origin.
  LocalFrame* frame = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    frame = window->GetFrame();
  } else {
    return base::nullopt;
  }

  const Frame& main_frame = frame->Tree().Top();
  if (!main_frame.GetSecurityContext()
           ->GetSecurityOrigin()
           ->IsPotentiallyTrustworthy()) {
    return base::nullopt;
  }

  if (!frame->IsMainFrame() && !frame->GetSecurityContext()
                                    ->GetSecurityOrigin()
                                    ->IsPotentiallyTrustworthy()) {
    return base::nullopt;
  }

  scoped_refptr<const SecurityOrigin> conversion_destination =
      SecurityOrigin::CreateFromString(conversion_destination_string);
  if (!conversion_destination->IsPotentiallyTrustworthy())
    return base::nullopt;

  bool impression_data_is_valid = false;
  uint64_t impression_data =
      impression_data_string.HexToUInt64Strict(&impression_data_is_valid);

  // Provide a default of 0 if the impression data was not valid.
  impression_data = impression_data_is_valid ? impression_data : 0UL;

  // Reporting origin is an optional attribute. Reporting origins must be
  // secure.
  base::Optional<WebSecurityOrigin> reporting_origin;
  if (reporting_origin_string) {
    reporting_origin =
        SecurityOrigin::CreateFromString(*reporting_origin_string);

    if (!reporting_origin->IsPotentiallyTrustworthy())
      return base::nullopt;
  }

  base::Optional<base::TimeDelta> expiry;
  if (impression_expiry_milliseconds)
    expiry = base::TimeDelta::FromMilliseconds(*impression_expiry_milliseconds);

  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kConversionAPIAll);
  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kImpressionRegistration);

  return WebImpression{conversion_destination, reporting_origin,
                       impression_data, expiry};
}

}  // namespace

base::Optional<WebImpression> GetImpressionForAnchor(
    HTMLAnchorElement* element) {
  base::Optional<uint64_t> expiry;
  if (element->hasAttribute(html_names::kImpressionexpiryAttr)) {
    bool expiry_is_valid = false;
    uint64_t expiry_milliseconds =
        element->FastGetAttribute(html_names::kImpressionexpiryAttr)
            .GetString()
            .ToUInt64Strict(&expiry_is_valid);
    if (expiry_is_valid)
      expiry = expiry_milliseconds;
  }

  DCHECK(element->hasAttribute(html_names::kConversiondestinationAttr));
  DCHECK(element->hasAttribute(html_names::kImpressiondataAttr));

  return GetImpression(
      element->GetExecutionContext(),
      element->FastGetAttribute(html_names::kImpressiondataAttr).GetString(),
      element->FastGetAttribute(html_names::kConversiondestinationAttr)
          .GetString(),
      element->hasAttribute(html_names::kReportingoriginAttr)
          ? base::make_optional(
                element->FastGetAttribute(html_names::kReportingoriginAttr)
                    .GetString())
          : base::nullopt,
      expiry);
}

base::Optional<WebImpression> GetImpressionForParams(
    ExecutionContext* execution_context,
    const ImpressionParams* params) {
  return GetImpression(execution_context, params->impressionData(),
                       params->conversionDestination(),
                       params->hasReportingOrigin()
                           ? base::make_optional(params->reportingOrigin())
                           : base::nullopt,
                       params->hasImpressionExpiry()
                           ? base::make_optional(params->impressionExpiry())
                           : base::nullopt);
}

}  // namespace blink
