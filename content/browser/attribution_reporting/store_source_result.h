// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_

#include <optional>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"
#include "content/common/content_export.h"
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
    std::optional<base::Time> min_fake_report_time;
    explicit SuccessNoised(std::optional<base::Time> min_fake_report_time)
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

  struct ExceedsMaxTriggerStateCardinality {};

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
                               ExceedsMaxChannelCapacity,
                               ExceedsMaxTriggerStateCardinality>;

  StoreSourceResult(StorableSource, Result);

  ~StoreSourceResult();

  StoreSourceResult(const StoreSourceResult&);
  StoreSourceResult(StoreSourceResult&&);

  StoreSourceResult& operator=(const StoreSourceResult&);
  StoreSourceResult& operator=(StoreSourceResult&&);

  attribution_reporting::mojom::StoreSourceResult status() const;

  const StorableSource& source() const { return source_; }

  const Result& result() const { return result_; }

 private:
  StorableSource source_;
  Result result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_H_
