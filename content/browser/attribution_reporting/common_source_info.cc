// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/cxx17_backports.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "net/base/schemeful_site.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

base::flat_set<SuitableOrigin> DestinationSet(SuitableOrigin destination) {
  base::flat_set<SuitableOrigin> set;
  set.reserve(1);
  set.insert(std::move(destination));
  return set;
}

base::Time ComputeReportWindowTime(
    absl::optional<base::Time> report_window_time,
    base::Time expiry_time) {
  return report_window_time.has_value() &&
                 report_window_time.value() <= expiry_time
             ? report_window_time.value()
             : expiry_time;
}

}  // namespace

// static
base::Time CommonSourceInfo::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    AttributionSourceType source_type) {
  constexpr base::TimeDelta kMinImpressionExpiry = base::Days(1);

  // Default to the maximum expiry time.
  base::TimeDelta expiry =
      declared_expiry.value_or(kDefaultAttributionSourceExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == AttributionSourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::Days(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return source_time + base::clamp(expiry, kMinImpressionExpiry,
                                   kDefaultAttributionSourceExpiry);
}

CommonSourceInfo::CommonSourceInfo(
    uint64_t source_event_id,
    SuitableOrigin source_origin,
    SuitableOrigin destination_origin,
    SuitableOrigin reporting_origin,
    base::Time source_time,
    base::Time expiry_time,
    absl::optional<base::Time> event_report_window_time,
    absl::optional<base::Time> aggregatable_report_window_time,
    AttributionSourceType source_type,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys)
    : CommonSourceInfo(source_event_id,
                       std::move(source_origin),
                       DestinationSet(std::move(destination_origin)),
                       std::move(reporting_origin),
                       source_time,
                       expiry_time,
                       event_report_window_time,
                       aggregatable_report_window_time,
                       source_type,
                       priority,
                       std::move(filter_data),
                       debug_key,
                       std::move(aggregation_keys)) {}

CommonSourceInfo::CommonSourceInfo(
    uint64_t source_event_id,
    SuitableOrigin source_origin,
    base::flat_set<SuitableOrigin> destination_origins,
    SuitableOrigin reporting_origin,
    base::Time source_time,
    base::Time expiry_time,
    absl::optional<base::Time> event_report_window_time,
    absl::optional<base::Time> aggregatable_report_window_time,
    AttributionSourceType source_type,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys)
    : source_event_id_(source_event_id),
      source_origin_(std::move(source_origin)),
      destination_origins_(std::move(destination_origins)),
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

  DCHECK(!destination_origins_.empty());
}

CommonSourceInfo::~CommonSourceInfo() = default;

CommonSourceInfo::CommonSourceInfo(const CommonSourceInfo&) = default;

CommonSourceInfo::CommonSourceInfo(CommonSourceInfo&&) = default;

CommonSourceInfo& CommonSourceInfo::operator=(const CommonSourceInfo&) =
    default;

CommonSourceInfo& CommonSourceInfo::operator=(CommonSourceInfo&&) = default;

net::SchemefulSite CommonSourceInfo::DestinationSite() const {
  return net::SchemefulSite(destination_origin());
}

net::SchemefulSite CommonSourceInfo::SourceSite() const {
  return net::SchemefulSite(source_origin_);
}

}  // namespace content
