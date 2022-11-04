// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HEADER_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HEADER_UTILS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class StorableSource;

CONTENT_EXPORT
base::expected<StorableSource,
               attribution_reporting::mojom::SourceRegistrationError>
ParseSourceRegistration(base::Value::Dict registration,
                        base::Time source_time,
                        url::Origin reporting_origin,
                        url::Origin source_origin,
                        AttributionSourceType source_type,
                        bool is_within_fenced_frame);

struct SourceRegistration {
  static base::expected<SourceRegistration,
                        attribution_reporting::mojom::SourceRegistrationError>
  Parse(base::Value::Dict, url::Origin reporting_origin);

  SourceRegistration();

  ~SourceRegistration();

  SourceRegistration(const SourceRegistration&);
  SourceRegistration& operator=(const SourceRegistration&);

  SourceRegistration(SourceRegistration&&);
  SourceRegistration& operator=(SourceRegistration&&);

  uint64_t source_event_id;
  url::Origin destination;
  url::Origin reporting_origin;
  absl::optional<base::TimeDelta> expiry;
  absl::optional<base::TimeDelta> event_report_window;
  absl::optional<base::TimeDelta> aggregatable_report_window;
  int64_t priority;
  AttributionFilterData filter_data;
  absl::optional<uint64_t> debug_key;
  attribution_reporting::AggregationKeys aggregation_keys;
  bool debug_reporting;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_HEADER_UTILS_H_
