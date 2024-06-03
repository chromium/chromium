// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_RATE_LIMIT_TABLE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_RATE_LIMIT_TABLE_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

namespace base {
class Time;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace content {

class AggregatableDebugReport;
class AttributionResolverDelegate;

class CONTENT_EXPORT AggregatableDebugRateLimitTable {
 public:
  enum class Result {
    kAllowed,
    kHitGlobalLimit,
    kHitReportingLimit,
    kHitBothLimits,
    kError,
  };

  explicit AggregatableDebugRateLimitTable(const AttributionResolverDelegate*);
  AggregatableDebugRateLimitTable(const AggregatableDebugRateLimitTable&) =
      delete;
  AggregatableDebugRateLimitTable& operator=(
      const AggregatableDebugRateLimitTable&) = delete;
  AggregatableDebugRateLimitTable(AggregatableDebugRateLimitTable&&) = delete;
  AggregatableDebugRateLimitTable& operator=(
      AggregatableDebugRateLimitTable&&) = delete;
  ~AggregatableDebugRateLimitTable();

  // Creates the table in `db` if it doesn't exist. Returns false on failure.
  [[nodiscard]] bool CreateTable(sql::Database* db);

  [[nodiscard]] bool AddRateLimit(sql::Database* db,
                                  const AggregatableDebugReport&);

  [[nodiscard]] Result AllowedForRateLimit(sql::Database* db,
                                           const AggregatableDebugReport&);

  [[nodiscard]] bool ClearAllDataAllTime(sql::Database* db);

  [[nodiscard]] bool ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter);

  void SetDelegate(const AttributionResolverDelegate&);

 private:
  [[nodiscard]] bool ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool DeleteExpiredRateLimits(sql::Database* db)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  raw_ref<const AttributionResolverDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Time at which `DeleteExpiredRateLimits()` was last called. Initialized to
  // the NULL time.
  base::Time last_cleared_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_RATE_LIMIT_TABLE_H_
