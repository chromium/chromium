// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_

#include "base/time/time.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT StoreSourceResult {
 public:
  explicit StoreSourceResult(
      attribution_reporting::mojom::StoreSourceResult status,
      absl::optional<base::Time> min_fake_report_time = absl::nullopt,
      absl::optional<int> max_destinations_per_source_site_reporting_site =
          absl::nullopt,
      absl::optional<int> max_sources_per_origin = absl::nullopt,
      absl::optional<int>
          max_destinations_per_rate_limit_window_reporting_origin =
              absl::nullopt);

  ~StoreSourceResult();

  StoreSourceResult(const StoreSourceResult&);
  StoreSourceResult(StoreSourceResult&&);

  StoreSourceResult& operator=(const StoreSourceResult&);
  StoreSourceResult& operator=(StoreSourceResult&&);

  attribution_reporting::mojom::StoreSourceResult status() const {
    return status_;
  }

  absl::optional<base::Time> min_fake_report_time() const {
    return min_fake_report_time_;
  }

  absl::optional<int> max_destinations_per_source_site_reporting_site() const {
    return max_destinations_per_source_site_reporting_site_;
  }

  absl::optional<int> max_sources_per_origin() const {
    return max_sources_per_origin_;
  }

  absl::optional<int> max_destinations_per_rate_limit_window_reporting_origin()
      const {
    return max_destinations_per_rate_limit_window_reporting_origin_;
  }

 private:
  attribution_reporting::mojom::StoreSourceResult status_;

  // The earliest report time for any fake reports stored alongside the
  // source, if any.
  absl::optional<base::Time> min_fake_report_time_;

  // Only populated in case of
  // `attribution_reporting::mojom::StoreSourceResult::kInsufficientUniqueDestinationCapacity`.
  absl::optional<int> max_destinations_per_source_site_reporting_site_;

  // Only populated in case of
  // `attribution_reporting::mojom::StoreSourceResult::kInsufficientSourceCapacity`.
  absl::optional<int> max_sources_per_origin_;

  // Populated in the cases of either
  // `attribution_reporting::mojom::StoreSourceResult::kDestinationReportingLimitReached`
  // or
  // `attribution_reporting::mojom::StoreSourceResult::kDestinationBothLimitsReached`
  absl::optional<int> max_destinations_per_rate_limit_window_reporting_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
