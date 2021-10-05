// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"

namespace base {
class Clock;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace content {

struct AttributionReport;

struct AggregateHistogramContribution {
  std::string bucket;
  uint32_t value;
};

// Manages storage for rate-limiting reports.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT RateLimitTable {
 public:
  enum class AttributionAllowedStatus {
    kAllowed,
    kNotAllowed,
    kError,
  };

  RateLimitTable(const AttributionStorage::Delegate* delegate,
                 const base::Clock* clock);
  RateLimitTable(const RateLimitTable& other) = delete;
  RateLimitTable& operator=(const RateLimitTable& other) = delete;
  RateLimitTable(RateLimitTable&& other) = delete;
  RateLimitTable& operator=(RateLimitTable&& other) = delete;
  ~RateLimitTable();

  // Creates the table in |db| if it doesn't exist.
  // Returns false on failure.
  bool CreateTable(sql::Database* db) WARN_UNUSED_RESULT;

  // Adds a rate limit to the table for an event-level report.
  // Returns false on failure.
  bool AddRateLimit(sql::Database* db,
                    const AttributionReport& report) WARN_UNUSED_RESULT;

  // Checks if the given attribution is allowed according to the data in the
  // table and policy as specified by the delegate.
  AttributionAllowedStatus AttributionAllowed(sql::Database* db,
                                              const AttributionReport& report,
                                              base::Time now)
      WARN_UNUSED_RESULT;

  // Attempts to add a set of histogram contributions to the rate limit. Returns
  // `kAllowed` if the contributions were added, `kNotAllowed` if the reports
  // would have exceeded the limit, and `kError` otherwise. This API will change
  // as we iterate on the aggregate API, so for now it is only available in
  // tests.
  AttributionAllowedStatus AddAggregateHistogramContributionsForTesting(
      sql::Database* db,
      const StorableSource& impression,
      const std::vector<AggregateHistogramContribution>& contributions)
      WARN_UNUSED_RESULT;

  // These should be 1:1 with |AttributionStorageSql|'s |ClearData| functions.
  // Returns false on failure.
  bool ClearAllDataInRange(sql::Database* db,
                           base::Time delete_begin,
                           base::Time delete_end) WARN_UNUSED_RESULT;
  // Returns false on failure.
  bool ClearAllDataAllTime(sql::Database* db) WARN_UNUSED_RESULT;
  // Returns false on failure.
  bool ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter)
      WARN_UNUSED_RESULT;
  bool ClearDataForImpressionIds(
      sql::Database* db,
      const std::vector<StorableSource::Id>& impression_ids) WARN_UNUSED_RESULT;

 private:
  // Returns the capacity for the given `attribution_type`, `impression_site`,
  // `conversion_destination`, according to `delegate_->GetRateLimits()`.
  // Returns 0 if there is no capacity, -1 on error.
  int64_t GetCapacity(sql::Database* db,
                      AttributionStorage::AttributionType attribution_type,
                      const std::string& serialized_impression_site,
                      const std::string& serialized_conversion_destination,
                      base::Time now)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  bool AddRow(sql::Database* db,
              AttributionStorage::AttributionType attribution_type,
              StorableSource::Id impression_id,
              const std::string& serialized_impression_site,
              const std::string& serialized_impression_origin,
              const std::string& serialized_conversion_destination,
              const std::string& serialized_conversion_origin,
              base::Time time,
              const std::string& bucket,
              uint32_t value)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Deletes data in the table older than the window determined by |clock_| and
  // |delegate_->GetRateLimits()|.
  // Returns false on failure.
  bool DeleteExpiredRateLimits(
      sql::Database* db,
      AttributionStorage::AttributionType attribution_type)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Must outlive |this|.
  const AttributionStorage::Delegate* delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Must outlive |this|.
  const base::Clock* clock_;

  // Time at which `DeleteExpiredRateLimits()` was last called. Initialized to
  // the NULL time.
  base::Time last_cleared_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
