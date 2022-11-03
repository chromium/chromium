// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

absl::optional<uint64_t> ParseUint64(const base::Value::Dict& dict,
                                     base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  uint64_t value;
  return base::StringToUint64(*s, &value) ? absl::make_optional(value)
                                          : absl::nullopt;
}

absl::optional<int64_t> ParseInt64(const base::Value::Dict& dict,
                                   base::StringPiece key) {
  const std::string* s = dict.FindString(key);
  if (!s)
    return absl::nullopt;

  int64_t value;
  return base::StringToInt64(*s, &value) ? absl::make_optional(value)
                                         : absl::nullopt;
}

absl::optional<base::TimeDelta> ParseTimeDeltaInSeconds(
    const base::Value::Dict& registration,
    base::StringPiece key) {
  if (absl::optional<int64_t> seconds = ParseInt64(registration, key))
    return base::Seconds(*seconds);
  return absl::nullopt;
}

}  // namespace

base::expected<StorableSource, SourceRegistrationError> ParseSourceRegistration(
    base::Value::Dict registration,
    base::Time source_time,
    url::Origin reporting_origin,
    url::Origin source_origin,
    AttributionSourceType source_type,
    bool is_within_fenced_frame) {
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

  uint64_t source_event_id =
      ParseUint64(registration, "source_event_id").value_or(0);

  int64_t priority = ParseInt64(registration, "priority").value_or(0);

  absl::optional<base::TimeDelta> expiry =
      ParseTimeDeltaInSeconds(registration, "expiry");

  absl::optional<base::TimeDelta> event_report_window =
      ParseTimeDeltaInSeconds(registration, "event_report_window");

  absl::optional<base::TimeDelta> aggregatable_report_window =
      ParseTimeDeltaInSeconds(registration, "aggregatable_report_window");

  absl::optional<uint64_t> debug_key = ParseUint64(registration, "debug_key");

  base::expected<AttributionFilterData, SourceRegistrationError> filter_data =
      AttributionFilterData::FromJSON(registration.Find("filter_data"));
  if (!filter_data.has_value())
    return base::unexpected(filter_data.error());

  base::expected<AttributionAggregationKeys, SourceRegistrationError>
      aggregation_keys = AttributionAggregationKeys::FromJSON(
          registration.Find("aggregation_keys"));
  if (!aggregation_keys.has_value())
    return base::unexpected(aggregation_keys.error());

  bool debug_reporting =
      registration.FindBool("debug_reporting").value_or(false);

  return StorableSource(
      CommonSourceInfo(
          source_event_id, std::move(source_origin), std::move(destination),
          std::move(reporting_origin), source_time,
          CommonSourceInfo::GetExpiryTime(expiry, source_time, source_type),
          event_report_window
              ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                    event_report_window, source_time, source_type))
              : absl::nullopt,
          aggregatable_report_window
              ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                    aggregatable_report_window, source_time, source_type))
              : absl::nullopt,
          source_type, priority, std::move(*filter_data), debug_key,
          std::move(*aggregation_keys)),
      is_within_fenced_frame, debug_reporting);
}

}  // namespace content
