// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"

#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_reporting_request_options.h"

namespace blink {

network::mojom::AttributionReportingEligibility
ConvertAttributionReportingRequestOptionsToMojom(
    const AttributionReportingRequestOptions& options) {
  if (options.eventSourceEligible() && options.triggerEligible()) {
    return network::mojom::AttributionReportingEligibility::
        kEventSourceOrTrigger;
  }

  if (options.eventSourceEligible()) {
    return network::mojom::AttributionReportingEligibility::kEventSource;
  }

  if (options.triggerEligible()) {
    return network::mojom::AttributionReportingEligibility::kTrigger;
  }

  return network::mojom::AttributionReportingEligibility::kEmpty;
}

}  // namespace blink
