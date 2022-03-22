// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/conversion_measurement_parsing.h"

#include <stdint.h>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

absl::optional<int64_t> ParseInt64(const String& string,
                                   LocalFrame* frame,
                                   HTMLElement* element,
                                   AttributionReportingIssueType issue_type) {
  if (string.IsNull())
    return absl::nullopt;

  bool valid = false;
  int64_t value = string.ToInt64Strict(&valid);

  if (valid)
    return value;

  if (frame) {
    AuditsIssue::ReportAttributionIssue(frame->DomWindow(), issue_type,
                                        frame->GetDevToolsFrameToken(), element,
                                        absl::nullopt, string);
  }

  return absl::nullopt;
}

absl::optional<int64_t> ParseExpiry(const String& string,
                                    LocalFrame* frame,
                                    HTMLElement* element = nullptr) {
  return ParseInt64(
      string, frame, element,
      AttributionReportingIssueType::kInvalidAttributionSourceExpiry);
}

absl::optional<int64_t> ParsePriority(const String& string,
                                      LocalFrame* frame,
                                      HTMLElement* element = nullptr) {
  return ParseInt64(
      string, frame, element,
      AttributionReportingIssueType::kInvalidAttributionSourcePriority);
}

LocalFrame* GetFrame(ExecutionContext* execution_context) {
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  return window ? window->GetFrame() : nullptr;
}

absl::optional<WebImpression> GetImpression(
    ExecutionContext* execution_context,
    const String& impression_data_string,
    const String& conversion_destination_string,
    const String& reporting_origin_string,
    absl::optional<int64_t> impression_expiry_milliseconds,
    absl::optional<int64_t> attribution_source_priority,
    HTMLAnchorElement* element) {
  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(
          execution_context)) {
    return absl::nullopt;
  }

  LocalFrame* frame = GetFrame(execution_context);
  if (!frame) {
    return absl::nullopt;
  }

  const bool feature_policy_enabled = execution_context->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);
  UMA_HISTOGRAM_BOOLEAN("Conversions.ImpressionIgnoredByFeaturePolicy",
                        !feature_policy_enabled);

  if (!feature_policy_enabled) {
    AuditsIssue::ReportAttributionIssue(
        frame->DomWindow(),
        AttributionReportingIssueType::kPermissionPolicyDisabled,
        frame->GetDevToolsFrameToken(), element);
    return absl::nullopt;
  }

  // Conversion measurement is only allowed in secure context.
  if (!execution_context->IsSecureContext()) {
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

  // For source registrations where there is no mechanism to raise an error,
  // such as on an anchor element, it is more useful to log the source with
  // default data so that a reporting origin can learn the failure mode.
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
  if (!reporting_origin_string.IsNull()) {
    reporting_origin =
        SecurityOrigin::CreateFromString(reporting_origin_string);

    if (!reporting_origin->IsPotentiallyTrustworthy()) {
      AuditsIssue::ReportAttributionIssue(
          frame->DomWindow(),
          AttributionReportingIssueType::kAttributionSourceUntrustworthyOrigin,
          absl::nullopt, element, absl::nullopt, reporting_origin_string);
      return absl::nullopt;
    }
  }

  absl::optional<base::TimeDelta> expiry;
  if (impression_expiry_milliseconds)
    expiry = base::Milliseconds(*impression_expiry_milliseconds);

  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kConversionAPIAll);
  UseCounter::Count(execution_context,
                    mojom::blink::WebFeature::kImpressionRegistration);

  return WebImpression{conversion_destination, reporting_origin,
                       impression_data, expiry,
                       attribution_source_priority.value_or(0)};
}

}  // namespace

absl::optional<WebImpression> GetImpressionForAnchor(
    HTMLAnchorElement* element) {
  DCHECK(element->FastHasAttribute(html_names::kAttributiondestinationAttr));
  DCHECK(element->FastHasAttribute(html_names::kAttributionsourceeventidAttr));

  LocalFrame* frame = GetFrame(element->GetExecutionContext());

  return GetImpression(
      element->GetExecutionContext(),
      element->FastGetAttribute(html_names::kAttributionsourceeventidAttr)
          .GetString(),
      element->FastGetAttribute(html_names::kAttributiondestinationAttr)
          .GetString(),
      element->FastGetAttribute(html_names::kAttributionreporttoAttr)
          .GetString(),
      ParseExpiry(element->FastGetAttribute(html_names::kAttributionexpiryAttr)
                      .GetString(),
                  frame, element),
      ParsePriority(
          element->FastGetAttribute(html_names::kAttributionsourcepriorityAttr)
              .GetString(),
          frame, element),
      element);
}

}  // namespace blink
