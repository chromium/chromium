// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

absl::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict) {
  const std::string* s = dict.FindString("debug_key");
  if (!s)
    return absl::nullopt;

  uint64_t value;
  return base::StringToUint64(*s, &value) ? absl::make_optional(value)
                                          : absl::nullopt;
}

int64_t ParsePriority(const base::Value::Dict& dict) {
  const std::string* s = dict.FindString("priority");
  if (!s)
    return 0;

  int64_t value;
  return base::StringToInt64(*s, &value) ? value : 0;
}

}  // namespace

base::expected<StorableSource, SourceRegistrationError> ParseSourceRegistration(
    base::Value::Dict registration,
    base::Time source_time,
    url::Origin reporting_origin,
    url::Origin source_origin,
    AttributionSourceType source_type) {
  url::Origin destination;
  {
    const base::Value* v = registration.Find("destination");
    if (!v)
      return base::unexpected(SourceRegistrationError::kDestinationMissing);

    const std::string* s = v->GetIfString();
    if (!s)
      return base::unexpected(SourceRegistrationError::kDestinationWrongType);

    destination = url::Origin::Create(GURL(*s));
    if (!network::IsOriginPotentiallyTrustworthy(destination)) {
      return base::unexpected(
          SourceRegistrationError::kDestinationUntrustworthy);
    }
  }

  uint64_t source_event_id = 0;
  if (const std::string* s = registration.FindString("source_event_id")) {
    if (!base::StringToUint64(*s, &source_event_id))
      source_event_id = 0;
  }

  int64_t priority = ParsePriority(registration);

  absl::optional<base::TimeDelta> expiry;
  if (const std::string* s = registration.FindString("expiry")) {
    int64_t seconds;
    if (base::StringToInt64(*s, &seconds))
      expiry = base::Seconds(seconds);
  }

  absl::optional<uint64_t> debug_key = ParseDebugKey(registration);

  base::expected<AttributionFilterData, SourceRegistrationError> filter_data =
      AttributionFilterData::FromSourceJSON(registration.Find("filter_data"));
  if (!filter_data.has_value())
    return base::unexpected(filter_data.error());

  base::expected<AttributionAggregationKeys, SourceRegistrationError>
      aggregation_keys = AttributionAggregationKeys::FromJSON(
          registration.Find("aggregation_keys"));
  if (!aggregation_keys.has_value())
    return base::unexpected(aggregation_keys.error());

  return StorableSource(CommonSourceInfo(
      source_event_id, std::move(source_origin), std::move(destination),
      std::move(reporting_origin), source_time,
      CommonSourceInfo::GetExpiryTime(expiry, source_time, source_type),
      source_type, priority, std::move(*filter_data), debug_key,
      std::move(*aggregation_keys)));
}

}  // namespace content
