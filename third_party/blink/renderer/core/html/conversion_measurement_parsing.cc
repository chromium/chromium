// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/conversion_measurement_parsing.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_source_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

absl::optional<uint64_t> ParseExpiry(const String& expiry) {
  bool expiry_is_valid = false;
  uint64_t parsed_expiry = expiry.ToUInt64Strict(&expiry_is_valid);
  return expiry_is_valid ? absl::make_optional(parsed_expiry) : absl::nullopt;
}

absl::optional<int64_t> ParsePriority(const String& priority) {
  bool priority_is_valid = false;
  int64_t parsed_priority = priority.ToInt64Strict(&priority_is_valid);
  return priority_is_valid ? absl::make_optional(parsed_priority)
                           : absl::nullopt;
}

absl::optional<WebImpression> GetImpression(
    ExecutionContext* execution_context,
    const String& impression_data_string,
    const String& conversion_destination_string,
    const absl::optional<String>& reporting_origin_string,
    absl::optional<uint64_t> impression_expiry_milliseconds,
    absl::optional<int64_t> attribution_source_priority,
    HTMLAnchorElement* element) {
  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(execution_context))
    return absl::nullopt;

  LocalFrame* frame = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    frame = window->GetFrame();
  } else {
    return absl::nullopt;
  }

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kAttributionReporting)) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kPermissionPolicyDisabled,
        frame->GetDevToolsFrameToken(), element);

    // TODO(crbug.com/1178400): Remove console message once the issue reported
    //     above is actually shown in DevTools.
    String message =
        "The 'attribution-reporting' permissions policy must be enabled to "
        "declare an attribution source.";
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError, message));
    return absl::nullopt;
  }

  // Conversion measurement is only allowed when both the frame and the main
  // frame (if different) have a secure origin.
  const Frame& main_frame = frame->Tree().Top();
  if (!main_frame.GetSecurityContext()
           ->GetSecurityOrigin()
           ->IsPotentiallyTrustworthy()) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        main_frame.GetDevToolsFrameToken(), element, absl::nullopt,
        main_frame.GetSecurityContext()->GetSecurityOrigin()->ToString());
    return absl::nullopt;
  }

  if (!frame->IsMainFrame() && !frame->GetSecurityContext()
                                    ->GetSecurityOrigin()
                                    ->IsPotentiallyTrustworthy()) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        frame->GetDevToolsFrameToken(), element, absl::nullopt,
        frame->GetSecurityContext()->GetSecurityOrigin()->ToString());
    return absl::nullopt;
  }

  scoped_refptr<const SecurityOrigin> conversion_destination =
      SecurityOrigin::CreateFromString(conversion_destination_string);
  if (!conversion_destination->IsPotentiallyTrustworthy()) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
        absl::nullopt, element, absl::nullopt, conversion_destination_string);
    return absl::nullopt;
  }

  bool impression_data_is_valid = false;
  uint64_t impression_data =
      impression_data_string.ToUInt64Strict(&impression_data_is_valid);

  if (!impression_data_is_valid) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kInvalidAttributionSourceEventId,
        frame->GetDevToolsFrameToken(), element, absl::nullopt,
        impression_data_string);
  }

  // Provide a default of 0 if the impression data was not valid.
  impression_data = impression_data_is_valid ? impression_data : 0UL;

  // Reporting origin is an optional attribute. Reporting origins must be
  // secure.
  absl::optional<WebSecurityOrigin> reporting_origin;
  if (reporting_origin_string) {
    reporting_origin =
        SecurityOrigin::CreateFromString(*reporting_origin_string);

    if (!reporting_origin->IsPotentiallyTrustworthy()) {
      AuditsIssue::ReportAttributionIssue(
          frame->DomWindow(),
          AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
          absl::nullopt, element, absl::nullopt, *reporting_origin_string);
      return absl::nullopt;
    }
  }

  absl::optional<base::TimeDelta> expiry;
  if (impression_expiry_milliseconds)
    expiry = base::TimeDelta::FromMilliseconds(*impression_expiry_milliseconds);

  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kConversionAPIAll);
  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kImpressionRegistration);

  int64_t priority =
      attribution_source_priority ? *attribution_source_priority : 0;

  return WebImpression{conversion_destination, reporting_origin,
                       impression_data, expiry, priority};
}

}  // namespace

absl::optional<WebImpression> GetImpressionForAnchor(
    HTMLAnchorElement* element) {
  absl::optional<uint64_t> expiry;
  if (element->hasAttribute(html_names::kAttributionexpiryAttr)) {
    expiry = ParseExpiry(
        element->FastGetAttribute(html_names::kAttributionexpiryAttr)
            .GetString());
  }

  absl::optional<int64_t> priority;
  if (element->hasAttribute(html_names::kAttributionsourcepriorityAttr)) {
    priority = ParsePriority(
        element->FastGetAttribute(html_names::kAttributionsourcepriorityAttr)
            .GetString());
  }

  DCHECK(element->hasAttribute(html_names::kAttributiondestinationAttr));
  DCHECK(element->hasAttribute(html_names::kAttributionsourceeventidAttr));

  return GetImpression(
      element->GetExecutionContext(),
      element->FastGetAttribute(html_names::kAttributionsourceeventidAttr)
          .GetString(),
      element->FastGetAttribute(html_names::kAttributiondestinationAttr)
          .GetString(),
      element->hasAttribute(html_names::kAttributionreporttoAttr)
          ? absl::make_optional(
                element->FastGetAttribute(html_names::kAttributionreporttoAttr)
                    .GetString())
          : absl::nullopt,
      expiry, priority, element);
}

absl::optional<WebImpression> GetImpressionFromWindowFeatures(
    ExecutionContext* execution_context,
    const ImpressionFeatures& features) {
  if (features.impression_data.IsNull() ||
      features.conversion_destination.IsNull())
    return absl::nullopt;

  return GetImpression(
      execution_context, features.impression_data,
      features.conversion_destination,
      !features.reporting_origin.IsNull()
          ? absl::make_optional(features.reporting_origin)
          : absl::nullopt,
      !features.expiry.IsNull() ? ParseExpiry(features.expiry) : absl::nullopt,
      !features.priority.IsNull() ? ParsePriority(features.priority)
                                  : absl::nullopt,
      nullptr);
}

absl::optional<WebImpression> GetImpressionForParams(
    ExecutionContext* execution_context,
    const AttributionSourceParams* params) {
  return GetImpression(
      execution_context, params->attributionSourceEventId(),
      params->attributionDestination(),
      params->hasAttributionReportTo()
          ? absl::make_optional(params->attributionReportTo())
          : absl::nullopt,
      params->hasAttributionExpiry()
          ? absl::make_optional(params->attributionExpiry())
          : absl::nullopt,
      params->hasAttributionSourcePriority()
          ? absl::make_optional(params->attributionSourcePriority())
          : absl::nullopt,
      nullptr);
}

}  // namespace blink
