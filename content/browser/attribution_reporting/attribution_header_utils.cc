// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
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

SourceRegistration::SourceRegistration() = default;

SourceRegistration::~SourceRegistration() = default;

SourceRegistration::SourceRegistration(const SourceRegistration&) = default;

SourceRegistration& SourceRegistration::operator=(const SourceRegistration&) =
    default;

SourceRegistration::SourceRegistration(SourceRegistration&&) = default;

SourceRegistration& SourceRegistration::operator=(SourceRegistration&&) =
    default;

// static
base::expected<SourceRegistration, SourceRegistrationError>
SourceRegistration::Parse(base::Value::Dict registration,
                          url::Origin reporting_origin) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  SourceRegistration result;

  {
    const base::Value* v = registration.Find("destination");
    if (!v)
      return base::unexpected(SourceRegistrationError::kDestinationMissing);

    const std::string* s = v->GetIfString();
    if (!s)
      return base::unexpected(SourceRegistrationError::kDestinationWrongType);

    result.destination = url::Origin::Create(GURL(*s));
    if (!network::IsOriginPotentiallyTrustworthy(result.destination)) {
      return base::unexpected(
          SourceRegistrationError::kDestinationUntrustworthy);
    }
  }

  result.source_event_id =
      ParseUint64(registration, "source_event_id").value_or(0);

  result.priority = ParseInt64(registration, "priority").value_or(0);

  result.expiry = ParseTimeDeltaInSeconds(registration, "expiry");

  result.event_report_window =
      ParseTimeDeltaInSeconds(registration, "event_report_window");

  result.aggregatable_report_window =
      ParseTimeDeltaInSeconds(registration, "aggregatable_report_window");

  result.debug_key = ParseUint64(registration, "debug_key");

  base::expected<AttributionFilterData, SourceRegistrationError> filter_data =
      AttributionFilterData::FromJSON(registration.Find("filter_data"));
  if (!filter_data.has_value())
    return base::unexpected(filter_data.error());

  result.filter_data = std::move(*filter_data);

  base::expected<attribution_reporting::AggregationKeys,
                 SourceRegistrationError>
      aggregation_keys = attribution_reporting::AggregationKeys::FromJSON(
          registration.Find("aggregation_keys"));
  if (!aggregation_keys.has_value())
    return base::unexpected(aggregation_keys.error());

  result.aggregation_keys = std::move(*aggregation_keys);

  result.debug_reporting =
      registration.FindBool("debug_reporting").value_or(false);

  result.reporting_origin = std::move(reporting_origin);
  return result;
}

base::expected<StorableSource, SourceRegistrationError> ParseSourceRegistration(
    base::Value::Dict registration,
    base::Time source_time,
    url::Origin reporting_origin,
    url::Origin source_origin,
    AttributionSourceType source_type,
    bool is_within_fenced_frame) {
  base::expected<SourceRegistration,
                 attribution_reporting::mojom::SourceRegistrationError>
      reg = SourceRegistration::Parse(std::move(registration),
                                      std::move(reporting_origin));
  if (!reg.has_value())
    return base::unexpected(reg.error());

  return StorableSource(
      CommonSourceInfo(
          reg->source_event_id, std::move(source_origin),
          std::move(reg->destination), std::move(reg->reporting_origin),
          source_time,
          CommonSourceInfo::GetExpiryTime(reg->expiry, source_time,
                                          source_type),
          reg->event_report_window
              ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                    reg->event_report_window, source_time, source_type))
              : absl::nullopt,
          reg->aggregatable_report_window
              ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                    reg->aggregatable_report_window, source_time, source_type))
              : absl::nullopt,
          source_type, reg->priority, std::move(reg->filter_data),
          reg->debug_key, std::move(reg->aggregation_keys)),
      is_within_fenced_frame, reg->debug_reporting);
}

}  // namespace content
