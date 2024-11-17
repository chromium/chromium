// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "services/network/attribution/request_headers_internal.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

namespace {

using ::network::mojom::AttributionReportingEligibility;

}  // namespace

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateIfNeeded(
    AttributionReportingEligibility eligibility) {
  return nullptr;
}

std::unique_ptr<AttributionRequestHelper>
AttributionRequestHelper::CreateForTesting() {
  return base::WrapUnique(new AttributionRequestHelper());
}

AttributionRequestHelper::AttributionRequestHelper() = default;

AttributionRequestHelper::~AttributionRequestHelper() = default;

void AttributionRequestHelper::Begin(net::URLRequest& request,
                                     base::OnceClosure done) {
  std::move(done).Run();
}

void AttributionRequestHelper::OnReceiveRedirect(
    net::URLRequest& request,
    mojom::URLResponseHeadPtr response,
    const net::RedirectInfo& redirect_info,
    base::OnceCallback<void(mojom::URLResponseHeadPtr response)> done) {
  std::move(done).Run(std::move(response));
}

void AttributionRequestHelper::Finalize(mojom::URLResponseHead& response,
                                        base::OnceClosure done) {
  std::move(done).Run();
}

// https://wicg.github.io/attribution-reporting-api/#mark-a-request-for-attribution-reporting-eligibility
net::HttpRequestHeaders ComputeAttributionReportingHeaders(
    const ResourceRequest& request) {
  net::HttpRequestHeaders headers;
  if (request.attribution_reporting_eligibility ==
      AttributionReportingEligibility::kUnset) {
    return headers;
  }

  const bool is_attribution_reporting_support_set =
      request.attribution_reporting_support !=
      network::mojom::AttributionSupport::kUnset;

  AttributionReportingEligibility effective_eligibility =
      is_attribution_reporting_support_set &&
              !HasAttributionSupport(request.attribution_reporting_support)
          ? AttributionReportingEligibility::kEmpty
          : request.attribution_reporting_eligibility;

  uint64_t grease_bits = base::RandUint64();

  std::string eligible_header = SerializeAttributionReportingEligibleHeader(
      effective_eligibility,
      AttributionReportingHeaderGreaseOptions::FromBits(grease_bits & 0xff));
  grease_bits >>= 8;

  headers.SetHeader("Attribution-Reporting-Eligible",
                    std::move(eligible_header));

  if (base::FeatureList::IsEnabled(
          features::kAttributionReportingCrossAppWeb)) {
    base::UmaHistogramEnumeration("Conversions.RequestSupportHeader",
                                  request.attribution_reporting_support);

    if (is_attribution_reporting_support_set) {
      headers.SetHeader("Attribution-Reporting-Support",
                        GetAttributionSupportHeader(
                            request.attribution_reporting_support,
                            AttributionReportingHeaderGreaseOptions::FromBits(
                                grease_bits & 0xff)));
      grease_bits >>= 8;
    }
  }

  return headers;
}

}  // namespace network
