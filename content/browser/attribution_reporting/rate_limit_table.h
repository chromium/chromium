// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace net {
class SchemefulSite;
}  // namespace net

namespace sql {
class Database;
}  // namespace sql

namespace content {

struct AttributionInfo;
class AttributionResolverDelegate;
class CommonSourceInfo;
class StorableSource;

enum class RateLimitResult : int;

// Manages storage for rate-limiting sources and attributions.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT RateLimitTable {
 public:
  // We have separate reporting origin rate limits for sources and attributions,
  // and separate attribution rate limits for event-level and aggregatable
  // attributions. This enum helps us differentiate between these cases in the
  // database.
  //
  // These values are persisted to the DB. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Scope {
    kSource = 0,
    kEventLevelAttribution = 1,
    kAggregatableAttribution = 2,
  };

  enum class DestinationRateLimitResult {
    kAllowed = 0,
    kHitGlobalLimit = 1,
    kHitReportingLimit = 2,
    kHitBothLimits = 3,
    kError = 4,
    kMaxValue = kError,
  };

  struct Error {};

  explicit RateLimitTable(const AttributionResolverDelegate*);
  RateLimitTable(const RateLimitTable&) = delete;
  RateLimitTable& operator=(const RateLimitTable&) = delete;
  RateLimitTable(RateLimitTable&&) = delete;
  RateLimitTable& operator=(RateLimitTable&&) = delete;
  ~RateLimitTable();

  // Creates the table in |db| if it doesn't exist.
  // Returns false on failure.
  [[nodiscard]] bool CreateTable(sql::Database* db);

  // Returns false on failure.
  [[nodiscard]] bool AddRateLimitForSource(sql::Database* db,
                                           const StoredSource& source,
                                           int64_t destination_limit_priority);

  // Returns false on failure.
  [[nodiscard]] bool AddRateLimitForAttribution(
      sql::Database* db,
      const AttributionInfo& attribution_info,
      const StoredSource&,
      Scope,
      AttributionReport::Id);

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginLimit(
      sql::Database* db,
      const StorableSource& source,
      base::Time source_time);

  [[nodiscard]] RateLimitResult SourceAllowedForReportingOriginPerSiteLimit(
      sql::Database* db,
      const StorableSource& source,
      base::Time source_time);

  [[nodiscard]] base::expected<std::vector<StoredSource::Id>, Error>
  GetSourcesToDeactivateForDestinationLimit(sql::Database* db,
                                            const StorableSource& source,
                                            base::Time source_time);

  [[nodiscard]] bool DeactivateSourcesForDestinationLimit(
      sql::Database* db,
      base::span<const StoredSource::Id>);

  [[nodiscard]] DestinationRateLimitResult SourceAllowedForDestinationRateLimit(
      sql::Database* db,
      const StorableSource& source,
      base::Time source_time);

  [[nodiscard]] RateLimitResult SourceAllowedForDestinationPerDayRateLimit(
      sql::Database* db,
      const StorableSource& source,
      base::Time source_time);

  [[nodiscard]] RateLimitResult AttributionAllowedForReportingOriginLimit(
      sql::Database* db,
      const AttributionInfo& attribution_info,
      const StoredSource&);

  [[nodiscard]] RateLimitResult AttributionAllowedForAttributionLimit(
      sql::Database* db,
      const AttributionInfo& attribution_info,
      const StoredSource&,
      Scope scope);

  [[nodiscard]] bool DeleteAttributionRateLimit(sql::Database* db,
                                                Scope scope,
                                                AttributionReport::Id);

  // These should be 1:1 with |AttributionStorageSql|'s |ClearData| functions.
  // Returns false on failure.
  [[nodiscard]] bool ClearAllDataAllTime(sql::Database* db);
  // Returns false on failure.
  [[nodiscard]] bool ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter);
  // Returns false on failure.
  [[nodiscard]] bool ClearDataForSourceIds(sql::Database* db,
                                           base::span<const StoredSource::Id>);

  void AppendRateLimitDataKeys(sql::Database* db,
                               std::set<AttributionDataModel::DataKey>& keys);

  void SetDelegate(const AttributionResolverDelegate&);

  static constexpr int64_t kUnsetRecordId = -1;

 private:
  [[nodiscard]] bool AddRateLimit(
      sql::Database* db,
      const StoredSource& source,
      std::optional<base::Time> trigger_time,
      const attribution_reporting::SuitableOrigin& context_origin,
      Scope,
      std::optional<AttributionReport::Id>,
      std::optional<int64_t> destination_limit_priority)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] RateLimitResult AllowedForReportingOriginLimit(
      sql::Database* db,
      bool is_source,
      const CommonSourceInfo& common_info,
      base::Time time,
      const base::flat_set<net::SchemefulSite>& destination_sites)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns false on failure.
  [[nodiscard]] bool ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes data in the table older than the window determined by
  // |delegate_->GetRateLimits()|.
  // Returns false on failure.
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

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
