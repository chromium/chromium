// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"

namespace base {
class GUID;
}  // namespace base

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace content {

class AttributionStorageDelegate;

// Provides an implementation of AttributionStorage that is backed by SQLite.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT AttributionStorageSql : public AttributionStorage {
 public:
  // Exposed for testing.
  static const int kCurrentVersionNumber;
  static const int kCompatibleVersionNumber;
  static const int kDeprecatedVersionNumber;

  static void RunInMemoryForTesting();

  AttributionStorageSql(const base::FilePath& path_to_database,
                        std::unique_ptr<AttributionStorageDelegate> delegate);
  AttributionStorageSql(const AttributionStorageSql& other) = delete;
  AttributionStorageSql& operator=(const AttributionStorageSql& other) = delete;
  AttributionStorageSql(AttributionStorageSql&& other) = delete;
  AttributionStorageSql& operator=(AttributionStorageSql&& other) = delete;
  ~AttributionStorageSql() override;

  void set_ignore_errors_for_testing(bool ignore_for_testing) {
    ignore_errors_for_testing_ = ignore_for_testing;
  }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InitStatus {
    kSuccess = 0,
    kFailedToOpenDbInMemory = 1,
    kFailedToOpenDbFile = 2,
    kFailedToCreateDir = 3,
    kFailedToInitializeSchema = 4,
    kMaxValue = kFailedToInitializeSchema,
  };

 private:
  enum class DbStatus {
    // The database has never been created, i.e. there is no database file at
    // all.
    kDeferringCreation,
    // The database exists but is not open yet.
    kDeferringOpen,
    // The database initialization failed, or the db suffered from an
    // unrecoverable error.
    kClosed,
    kOpen,
  };

  enum class DbCreationPolicy {
    // Create the db if it does not exist.
    kCreateIfAbsent,
    // Do not create the db if it does not exist.
    kIgnoreIfAbsent,
  };

  // AttributionStorage:
  StoreSourceResult StoreSource(
      const StorableSource& source,
      int deactivated_source_return_limit = -1) override;
  CreateReportResult MaybeCreateAndStoreReport(
      const AttributionTrigger& trigger) override;
  std::vector<AttributionReport> GetAttributionsToReport(
      base::Time max_report_time,
      int limit = -1) override;
  absl::optional<base::Time> GetNextReportTime(base::Time time) override;
  std::vector<AttributionReport> GetReports(
      const std::vector<AttributionReport::EventLevelData::Id>& ids) override;
  std::vector<StoredSource> GetActiveSources(int limit = -1) override;
  bool DeleteReport(AttributionReport::Id report_id) override;
  bool UpdateReportForSendFailure(
      AttributionReport::EventLevelData::Id report_id,
      base::Time new_report_time) override;
  absl::optional<base::Time> AdjustOfflineReportTimes() override;
  void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter) override;
  bool AddAggregatableAttributionForTesting(
      const AggregatableAttribution& aggregatable_attribution) override;
  std::vector<AttributionReport> GetAggregatableContributionReportsForTesting(
      base::Time max_report_time,
      int limit = -1) override;

  void ClearAllDataAllTime() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deactivates active, converted sources with the given conversion destination
  // and reporting origin. Returns at most `limit` of those, or null on error.
  [[nodiscard]] absl::optional<std::vector<DeactivatedSource>>
  DeactivateSources(const std::string& serialized_conversion_destination,
                    const std::string& serialized_reporting_origin,
                    int return_limit) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns false on failure.
  [[nodiscard]] bool DeleteSources(
      const std::vector<StoredSource::Id>& source_ids)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes all sources that have expired and have no pending
  // reports. Returns false on failure.
  [[nodiscard]] bool DeleteExpiredSources()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the report with `report_id` without checking the the DB
  // initialization status or the number of deleted rows. Returns false on
  // failure.
  [[nodiscard]] bool DeleteEventLevelReport(
      AttributionReport::EventLevelData::Id report_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  bool HasCapacityForStoringSource(const std::string& serialized_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  enum class ReportAlreadyStoredStatus {
    kNotStored,
    kStored,
    kError,
  };

  ReportAlreadyStoredStatus ReportAlreadyStored(
      StoredSource::Id source_id,
      absl::optional<uint64_t> dedup_key)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  enum class ConversionCapacityStatus {
    kHasCapacity,
    kNoCapacity,
    kError,
  };

  ConversionCapacityStatus CapacityForStoringReport(
      const std::string& serialized_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  enum class MaybeReplaceLowerPriorityEventLevelReportResult {
    kError,
    kAddNewReport,
    kDropNewReport,
    kDropNewReportSourceDeactivated,
    kReplaceOldReport,
  };
  [[nodiscard]] MaybeReplaceLowerPriorityEventLevelReportResult
  MaybeReplaceLowerPriorityEventLevelReport(
      const AttributionReport& report,
      int num_conversions,
      int64_t conversion_priority,
      absl::optional<AttributionReport>& replaced_report)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  absl::optional<AttributionReport> GetReport(
      AttributionReport::EventLevelData::Id report_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  absl::optional<std::vector<uint64_t>> ReadDedupKeys(
      StoredSource::Id source_id) VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool HasCapacityForUniqueDestinationLimitForPendingSource(
      const StorableSource& source) VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool StoreReport(StoredSource::Id source_id,
                                 uint64_t trigger_data,
                                 base::Time trigger_time,
                                 base::Time report_time,
                                 int64_t priority,
                                 const base::GUID& external_report_id,
                                 absl::optional<uint64_t> trigger_debug_key)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Initializes the database if necessary, and returns whether the database is
  // open. |should_create| indicates whether the database should be created if
  // it is not already.
  [[nodiscard]] bool LazyInit(DbCreationPolicy creation_policy)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Returns false on failure.
  [[nodiscard]] bool InitializeSchema(bool db_empty)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Returns false on failure.
  [[nodiscard]] bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void HandleInitializationFailure(const InitStatus status)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // Aggregate Attribution:

  // Deletes all aggregatable attribution data in storage for URLs matching
  // `filter`, between `delete_begin` and `delete_end` time. More specifically,
  // this:
  // 1. Deletes all sources within the time range. If any aggregatable
  // attribution
  //    is attributed to this source it is also deleted.
  // 2. Deletes all aggregatable attributions within the time range. All sources
  //    attributed to the aggregatable attribution are also deleted.
  //
  // All sources to be deleted are updated in `source_ids_to_delete`.
  // Returns false on failure.
  [[nodiscard]] bool ClearAggregatableAttributionForOriginsInRange(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter,
      std::vector<StoredSource::Id>& source_ids_to_delete)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool ClearAggregatableAttribution(
      AggregatableAttribution::Id aggregation_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool ClearAggregatableContributions(
      AggregatableAttribution::Id aggregation_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool ClearAggregatableAttributionForSourceIds(
      const std::vector<StoredSource::Id>& source_ids)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the report with `report_id` without checking the the DB
  // initialization status or the number of deleted rows. Returns false on
  // failure.
  // Note that the `aggregatable_report_metadata` row will be deleted with the
  // last contribution for the corresponding `aggregation_id`.
  [[nodiscard]] bool DeleteAggregatableContributionReport(
      AttributionReport::AggregatableContributionData::Id report_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  static bool g_run_in_memory_;

  // If set, database errors will not crash the client when run in debug mode.
  bool ignore_errors_for_testing_ = false;

  const base::FilePath path_to_database_;

  // Current status of the database initialization. Tracks what stage |this| is
  // at for lazy initialization, and used as a signal for if the database is
  // closed. This is initialized in the first call to LazyInit() to avoid doing
  // additional work in the constructor, see https://crbug.com/1121307.
  absl::optional<DbStatus> db_init_status_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // May be null if the database:
  //  - could not be opened
  //  - table/index initialization failed
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Table which stores timestamps of sent reports, and checks if new reports
  // can be created given API rate limits. The underlying table is created in
  // |db_|, but only accessed within |RateLimitTable|.
  RateLimitTable rate_limit_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<AttributionStorageDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Time at which `DeleteExpiredSources()` was last called. Initialized to
  // the NULL time.
  base::Time last_deleted_expired_sources_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AttributionStorageSql> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_
