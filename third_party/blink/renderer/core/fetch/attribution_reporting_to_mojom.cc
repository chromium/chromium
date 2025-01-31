// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"

#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_reporting_request_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
using ::network::mojom::AttributionReportingEligibility;
}  // namespace

// TODO(crbug.com/1434311): Consider throwing an exception if the URL to be
// fetched is non-HTTP-family or its origin is not potentially trustworthy,
// since Attribution Reporting registration is not supported on such requests.

AttributionReportingEligibility
ConvertAttributionReportingRequestOptionsToMojom(
    const AttributionReportingRequestOptions& options,
    const ExecutionContext& execution_context,
    ExceptionState& exception_state) {
  bool enabled = execution_context.IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kAttributionReporting);
  AttributionSrcLoader::RecordAttributionFeatureAllowed(enabled);
  if (!enabled) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Attribution Reporting operations require that the "
        "attribution-reporting Permissions Policy feature be enabled.");
    return AttributionReportingEligibility::kUnset;
  }

  if (options.eventSourceEligible() && options.triggerEligible()) {
    return AttributionReportingEligibility::kEventSourceOrTrigger;
  }

  if (options.eventSourceEligible()) {
    return AttributionReportingEligibility::kEventSource;
  }

  if (options.triggerEligible()) {
    return AttributionReportingEligibility::kTrigger;
  }

  return AttributionReportingEligibility::kEmpty;
}

}  // namespace blink
