// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host_utils.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/url_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace attribution_host_utils {

VerifyResult VerifyAndStoreImpression(StorableSource::SourceType source_type,
                                      const url::Origin& impression_origin,
                                      const blink::Impression& impression,
                                      BrowserContext* browser_context,
                                      ConversionManager& conversion_manager) {
  // Convert |impression| into a StorableImpression that can be forwarded to
  // storage. If a reporting origin was not provided, default to the conversion
  // destination for reporting.
  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression_origin
                                            : *impression.reporting_origin;

  const bool allowed =
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          browser_context,
          ContentBrowserClient::ConversionMeasurementOperation::kImpression,
          &impression_origin, /*conversion_origin=*/nullptr, &reporting_origin);
  if (!allowed)
    return VerifyResult{.allowed = false, .stored = false};

  const bool impression_origin_trustworthy =
      network::IsOriginPotentiallyTrustworthy(impression_origin) ||
      IsAndroidAppOrigin(impression_origin);
  // Conversion measurement is only allowed in secure contexts.
  if (!impression_origin_trustworthy ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    return VerifyResult{.allowed = true, .stored = false};
  }

  base::Time impression_time = base::Time::Now();

  const AttributionPolicy& policy = conversion_manager.GetAttributionPolicy();
  StorableSource storable_impression(
      policy.GetSanitizedImpressionData(impression.impression_data),
      impression_origin, impression.conversion_destination, reporting_origin,
      impression_time,
      policy.GetExpiryTimeForImpression(impression.expiry, impression_time,
                                        source_type),
      source_type, impression.priority,
      policy.GetAttributionLogicForImpression(source_type),
      /*impression_id=*/absl::nullopt);

  conversion_manager.HandleImpression(std::move(storable_impression));
  return VerifyResult{.allowed = true, .stored = true};
}

absl::optional<blink::Impression> ParseImpressionFromApp(
    const std::string& source_event_id,
    const std::string& destination,
    const std::string& report_to,
    int64_t expiry) {
  // Java API should have rejected these already.
  DCHECK(!source_event_id.empty() && !destination.empty());

  blink::Impression impression;
  if (!base::StringToUint64(source_event_id, &impression.impression_data))
    return absl::nullopt;

  impression.conversion_destination = url::Origin::Create(GURL(destination));
  if (!network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    return absl::nullopt;
  }

  if (!report_to.empty()) {
    impression.reporting_origin = url::Origin::Create(GURL(report_to));
    if (!network::IsOriginPotentiallyTrustworthy(*impression.reporting_origin))
      return absl::nullopt;
  }

  if (expiry != 0)
    impression.expiry = base::Milliseconds(expiry);

  return impression;
}

}  // namespace attribution_host_utils

}  // namespace content
