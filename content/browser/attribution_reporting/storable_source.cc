// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

StorableSource::StorableSource(CommonSourceInfo common_info,
                               bool is_within_fenced_frame,
                               bool debug_reporting)
    : common_info_(std::move(common_info)),
      is_within_fenced_frame_(is_within_fenced_frame),
      debug_reporting_(debug_reporting) {}

StorableSource::StorableSource(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration reg,
    base::Time source_time,
    attribution_reporting::SuitableOrigin source_origin,
    attribution_reporting::mojom::SourceType source_type,
    bool is_within_fenced_frame)
    : registration_json_(
          SerializeAttributionJson(reg.ToJson(), /*pretty_print=*/true)),
      common_info_(
          reg.source_event_id,
          std::move(source_origin),
          std::move(reg.destination_set),
          std::move(reporting_origin),
          source_time,
          CommonSourceInfo::GetExpiryTime(reg.expiry, source_time, source_type),
          CommonSourceInfo::GetReportWindowTime(reg.event_report_window,
                                                source_time),
          CommonSourceInfo::GetReportWindowTime(reg.aggregatable_report_window,
                                                source_time),
          source_type,
          reg.priority,
          std::move(reg.filter_data),
          reg.debug_key,
          std::move(reg.aggregation_keys)),
      is_within_fenced_frame_(is_within_fenced_frame),
      debug_reporting_(reg.debug_reporting) {}

StorableSource::~StorableSource() = default;

StorableSource::StorableSource(const StorableSource&) = default;

StorableSource::StorableSource(StorableSource&&) = default;

StorableSource& StorableSource::operator=(const StorableSource&) = default;

StorableSource& StorableSource::operator=(StorableSource&&) = default;

}  // namespace content
