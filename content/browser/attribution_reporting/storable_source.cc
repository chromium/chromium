// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include <stdint.h>

#include <utility>

#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

StorableSource::StorableSource(CommonSourceInfo common_info,
                               bool is_within_fenced_frame,
                               bool debug_reporting)
    : common_info_(std::move(common_info)),
      is_within_fenced_frame_(is_within_fenced_frame),
      debug_reporting_(debug_reporting) {}

StorableSource::StorableSource(attribution_reporting::SourceRegistration reg,
                               base::Time source_time,
                               url::Origin source_origin,
                               AttributionSourceType source_type,
                               bool is_within_fenced_frame)
    : StorableSource(
          CommonSourceInfo(
              reg.source_event_id_,
              std::move(source_origin),
              std::move(reg.destination_),
              std::move(reg.reporting_origin_),
              source_time,
              CommonSourceInfo::GetExpiryTime(reg.expiry_,
                                              source_time,
                                              source_type),
              reg.event_report_window_
                  ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                        reg.event_report_window_,
                        source_time,
                        source_type))
                  : absl::nullopt,
              reg.aggregatable_report_window_
                  ? absl::make_optional(CommonSourceInfo::GetExpiryTime(
                        reg.aggregatable_report_window_,
                        source_time,
                        source_type))
                  : absl::nullopt,
              source_type,
              reg.priority_,
              std::move(reg.filter_data_),
              reg.debug_key_,
              std::move(reg.aggregation_keys_)),
          is_within_fenced_frame,
          reg.debug_reporting_) {}

StorableSource::~StorableSource() = default;

StorableSource::StorableSource(const StorableSource&) = default;

StorableSource::StorableSource(StorableSource&&) = default;

StorableSource& StorableSource::operator=(const StorableSource&) = default;

StorableSource& StorableSource::operator=(StorableSource&&) = default;

}  // namespace content
