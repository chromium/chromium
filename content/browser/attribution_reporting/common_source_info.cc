// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include <utility>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

base::Time ComputeReportWindowTime(
    absl::optional<base::Time> report_window_time,
    base::Time expiry_time) {
  return report_window_time.has_value() &&
                 report_window_time.value() <= expiry_time
             ? report_window_time.value()
             : expiry_time;
}

base::Time GetClampedTime(base::TimeDelta time_delta, base::Time source_time) {
  constexpr base::TimeDelta kMinDeltaTime = base::Days(1);
  return source_time + base::clamp(time_delta, kMinDeltaTime,
                                   kDefaultAttributionSourceExpiry);
}

}  // namespace

// static
base::Time CommonSourceInfo::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    SourceType source_type) {
  // Default to the maximum expiry time.
  base::TimeDelta expiry =
      declared_expiry.value_or(kDefaultAttributionSourceExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == SourceType::kEvent) {
    expiry = expiry.RoundToMultiple(base::Days(1));
  }

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return GetClampedTime(expiry, source_time);
}

// static
absl::optional<base::Time> CommonSourceInfo::GetReportWindowTime(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  // If the impression specified its own window, clamp it to the minimum and
  // maximum.
  return declared_window.has_value()
             ? absl::make_optional(
                   GetClampedTime(declared_window.value(), source_time))
             : absl::nullopt;
}

CommonSourceInfo::CommonSourceInfo(
    uint64_t source_event_id,
    SuitableOrigin source_origin,
    attribution_reporting::DestinationSet destination_sites,
    SuitableOrigin reporting_origin,
    base::Time source_time,
    base::Time expiry_time,
    absl::optional<base::Time> event_report_window_time,
    absl::optional<base::Time> aggregatable_report_window_time,
    SourceType source_type,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys)
    : source_event_id_(source_event_id),
      source_origin_(std::move(source_origin)),
      destination_sites_(std::move(destination_sites)),
      reporting_origin_(std::move(reporting_origin)),
      source_time_(source_time),
      expiry_time_(expiry_time),
      event_report_window_time_(
          ComputeReportWindowTime(event_report_window_time, expiry_time)),
      aggregatable_report_window_time_(
          ComputeReportWindowTime(aggregatable_report_window_time,
                                  expiry_time)),
      source_type_(source_type),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)) {
  DCHECK_GE(kDefaultAttributionSourceExpiry, expiry_time_ - source_time);
  DCHECK_GE(kDefaultAttributionSourceExpiry,
            event_report_window_time_ - source_time);
  DCHECK_GE(kDefaultAttributionSourceExpiry,
            aggregatable_report_window_time_ - source_time);

  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time_, source_time);
  DCHECK_GT(event_report_window_time_, source_time);
  DCHECK_GT(aggregatable_report_window_time_, source_time);
}

CommonSourceInfo::~CommonSourceInfo() = default;

CommonSourceInfo::CommonSourceInfo(const CommonSourceInfo&) = default;

CommonSourceInfo::CommonSourceInfo(CommonSourceInfo&&) = default;

CommonSourceInfo& CommonSourceInfo::operator=(const CommonSourceInfo&) =
    default;

CommonSourceInfo& CommonSourceInfo::operator=(CommonSourceInfo&&) = default;

net::SchemefulSite CommonSourceInfo::SourceSite() const {
  return net::SchemefulSite(source_origin_);
}

}  // namespace content
