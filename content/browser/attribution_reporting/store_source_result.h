// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_

#include <utility>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"
#include "content/browser/attribution_reporting/store_source_result_internal.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

class CONTENT_EXPORT StoreSourceResult {
 public:
  struct Success {};

  struct InternalError {};

  struct InsufficientSourceCapacity {
    int limit;
    explicit InsufficientSourceCapacity(int limit) : limit(limit) {}
  };

  struct InsufficientUniqueDestinationCapacity {
    int limit;
    explicit InsufficientUniqueDestinationCapacity(int limit) : limit(limit) {}
  };

  struct ExcessiveReportingOrigins {};

  struct ProhibitedByBrowserPolicy {};

  struct SuccessNoised {
    absl::optional<base::Time> min_fake_report_time;
    explicit SuccessNoised(absl::optional<base::Time> min_fake_report_time)
        : min_fake_report_time(min_fake_report_time) {}
  };

  struct DestinationReportingLimitReached {
    int limit;
    explicit DestinationReportingLimitReached(int limit) : limit(limit) {}
  };

  struct DestinationGlobalLimitReached {};

  struct DestinationBothLimitsReached {
    int limit;
    explicit DestinationBothLimitsReached(int limit) : limit(limit) {}
  };

  struct ReportingOriginsPerSiteLimitReached {};

  struct ExceedsMaxChannelCapacity {};

  using Result = absl::variant<Success,
                               InternalError,
                               InsufficientSourceCapacity,
                               InsufficientUniqueDestinationCapacity,
                               ExcessiveReportingOrigins,
                               ProhibitedByBrowserPolicy,
                               SuccessNoised,
                               DestinationReportingLimitReached,
                               DestinationGlobalLimitReached,
                               DestinationBothLimitsReached,
                               ReportingOriginsPerSiteLimitReached,
                               ExceedsMaxChannelCapacity>;

  // Allows implicit conversion from one of the variant types.
  template <typename T,
            internal::EnableIfIsVariantAlternative<T, Result> = true>
  StoreSourceResult(T&& result)  // NOLINT
      : result_(std::forward<T>(result)) {}

  ~StoreSourceResult() = default;

  StoreSourceResult(const StoreSourceResult&) = default;
  StoreSourceResult(StoreSourceResult&&) = default;

  StoreSourceResult& operator=(const StoreSourceResult&) = default;
  StoreSourceResult& operator=(StoreSourceResult&&) = default;

  attribution_reporting::mojom::StoreSourceResult status() const;

  const Result& result() const { return result_; }

 private:
  Result result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
