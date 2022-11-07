// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <utility>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

base::expected<StorableSource,
               attribution_reporting::mojom::SourceRegistrationError>
ParseSourceRegistration(base::Value::Dict registration,
                        base::Time source_time,
                        url::Origin reporting_origin,
                        url::Origin source_origin,
                        AttributionSourceType source_type,
                        bool is_within_fenced_frame) {
  base::expected<attribution_reporting::SourceRegistration,
                 attribution_reporting::mojom::SourceRegistrationError>
      reg = attribution_reporting::SourceRegistration::Parse(
          std::move(registration), std::move(reporting_origin));
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
