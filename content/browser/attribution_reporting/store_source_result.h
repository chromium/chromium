// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_

#include <optional>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

class CONTENT_EXPORT StoreSourceResult {
 public:
  struct Success {
    std::optional<base::Time> min_fake_report_time;
    StoredSource::Id source_id;
    Success(std::optional<base::Time> min_fake_report_time,
            StoredSource::Id source_id)
        : min_fake_report_time(min_fake_report_time), source_id(source_id) {}
  };

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

  struct DestinationReportingLimitReached {
    int limit;
    explicit DestinationReportingLimitReached(int limit) : limit(limit) {}
  };

  struct DestinationPerDayReportingLimitReached {
    int limit;
    explicit DestinationPerDayReportingLimitReached(int limit) : limit(limit) {}
  };

  struct DestinationGlobalLimitReached {};

  struct DestinationBothLimitsReached {
    int limit;
    explicit DestinationBothLimitsReached(int limit) : limit(limit) {}
  };

  struct ReportingOriginsPerSiteLimitReached {
    int limit;
    explicit ReportingOriginsPerSiteLimitReached(int limit) : limit(limit) {}
  };

  struct ExceedsMaxChannelCapacity {
    double limit;
    explicit ExceedsMaxChannelCapacity(double limit) : limit(limit) {}
  };

  struct ExceedsMaxScopesChannelCapacity {
    double limit;
    explicit ExceedsMaxScopesChannelCapacity(double limit) : limit(limit) {}
  };

  struct ExceedsMaxTriggerStateCardinality {
    uint32_t limit;
    explicit ExceedsMaxTriggerStateCardinality(uint32_t limit) : limit(limit) {}
  };

  struct ExceedsMaxEventStatesLimit {
    uint32_t limit;
    explicit ExceedsMaxEventStatesLimit(uint32_t limit) : limit(limit) {}
  };

  using Result = absl::variant<Success,
                               InternalError,
                               InsufficientSourceCapacity,
                               InsufficientUniqueDestinationCapacity,
                               ExcessiveReportingOrigins,
                               ProhibitedByBrowserPolicy,
                               DestinationReportingLimitReached,
                               DestinationGlobalLimitReached,
                               DestinationBothLimitsReached,
                               ReportingOriginsPerSiteLimitReached,
                               ExceedsMaxChannelCapacity,
                               ExceedsMaxScopesChannelCapacity,
                               ExceedsMaxTriggerStateCardinality,
                               ExceedsMaxEventStatesLimit,
                               DestinationPerDayReportingLimitReached>;

  StoreSourceResult(StorableSource,
                    bool is_noised,
                    base::Time source_time,
                    std::optional<int> destination_limit,
                    Result);

  ~StoreSourceResult();

  StoreSourceResult(const StoreSourceResult&);
  StoreSourceResult(StoreSourceResult&&);

  StoreSourceResult& operator=(const StoreSourceResult&);
  StoreSourceResult& operator=(StoreSourceResult&&);

  attribution_reporting::mojom::StoreSourceResult status() const;

  const StorableSource& source() const { return source_; }

  bool is_noised() const { return is_noised_; }

  base::Time source_time() const { return source_time_; }

  std::optional<int> destination_limit() const { return destination_limit_; }

  const Result& result() const { return result_; }

 private:
  StorableSource source_;
  bool is_noised_;
  base::Time source_time_;
  std::optional<int> destination_limit_;
  Result result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
