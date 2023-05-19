// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sql {
class Statement;
class StatementID;
}  // namespace sql

namespace content {

class AttributionStorageDelegate;
struct AttributionInfo;

enum class RateLimitResult : int;

// Provides an implementation of AttributionStorage that is backed by SQLite.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT AttributionStorageSql : public AttributionStorage {
 public:
  // Version number of the database.
  static constexpr int kCurrentVersionNumber = 54;

  // Earliest version which can use a `kCurrentVersionNumber` database
  // without failing.
  static constexpr int kCompatibleVersionNumber = 54;

  // Latest version of the database that cannot be upgraded to
  // `kCurrentVersionNumber` without razing the database.
  static constexpr int kDeprecatedVersionNumber = 51;

  static_assert(kCompatibleVersionNumber <= kCurrentVersionNumber);
  static_assert(kDeprecatedVersionNumber < kCompatibleVersionNumber);

  // If `user_data_directory` is empty, the DB is created in memory and no data
  // is persisted to disk.
  AttributionStorageSql(const base::FilePath& user_data_directory,
                        std::unique_ptr<AttributionStorageDelegate> delegate);
  AttributionStorageSql(const AttributionStorageSql&) = delete;
  AttributionStorageSql& operator=(const AttributionStorageSql&) = delete;
  AttributionStorageSql(AttributionStorageSql&&) = delete;
  AttributionStorageSql& operator=(AttributionStorageSql&&) = delete;
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
  StoreSourceResult StoreSource(const StorableSource& source) override;
  CreateReportResult MaybeCreateAndStoreReport(
      const AttributionTrigger& trigger) override;
  std::vector<AttributionReport> GetAttributionReports(
      base::Time max_report_time,
      int limit = -1) override;
  absl::optional<base::Time> GetNextReportTime(base::Time time) override;
  std::vector<AttributionReport> GetReports(
      const std::vector<AttributionReport::Id>& ids) override;
  std::vector<StoredSource> GetActiveSources(int limit = -1) override;
  std::set<AttributionDataModel::DataKey> GetAllDataKeys() override;
  void DeleteByDataKey(const AttributionDataModel::DataKey& datakey) override;
  bool DeleteReport(AttributionReport::Id report_id) override;
  bool UpdateReportForSendFailure(AttributionReport::Id report_id,
                                  base::Time new_report_time) override;
  absl::optional<base::Time> AdjustOfflineReportTimes() override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 bool delete_rate_limit_data) override;

  void ClearAllDataAllTime(bool delete_rate_limit_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deactivates the given sources. Returns false on error.
  [[nodiscard]] bool DeactivateSources(
      const std::vector<StoredSource::Id>& sources)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns false on failure.
  [[nodiscard]] bool DeleteSources(
      const std::vector<StoredSource::Id>& source_ids)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes all sources that have expired and have no pending
  // reports. Returns false on failure.
  [[nodiscard]] bool DeleteExpiredSources()
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
      absl::optional<uint64_t> dedup_key,
      AttributionReport::Type report_type)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  enum class ConversionCapacityStatus {
    kHasCapacity,
    kNoCapacity,
    kError,
  };

  ConversionCapacityStatus CapacityForStoringReport(const AttributionTrigger&,
                                                    AttributionReport::Type)
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

  absl::optional<AttributionReport> GetReport(AttributionReport::Id report_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  absl::optional<std::vector<uint64_t>> ReadDedupKeys(
      StoredSource::Id source_id,
      AttributionReport::Type report_type)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  bool StoreDedupKey(StoredSource::Id source_id,
                     uint64_t dedup_key,
                     AttributionReport::Type report_type)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  absl::optional<AttributionReport> ReadReportFromStatement(sql::Statement&)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  std::vector<AttributionReport> GetReportsInternal(base::Time max_report_time,
                                                    int limit)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  absl::optional<base::Time> GetNextReportTime(sql::StatementID id,
                                               const char* sql,
                                               base::Time time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool AdjustOfflineReportTimes(sql::StatementID id,
                                              const char* sql,
                                              base::TimeDelta min_delay,
                                              base::TimeDelta max_delay,
                                              base::Time now)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether the database execution was successful.
  // `source_id_to_attribute` and `source_ids_to_delete` would be populated if
  // matching sources were found.
  bool FindMatchingSourceForTrigger(
      const AttributionTrigger& trigger,
      base::Time trigger_time,
      absl::optional<StoredSource::Id>& source_id_to_attribute,
      std::vector<StoredSource::Id>& source_ids_to_delete,
      std::vector<StoredSource::Id>& source_ids_to_deactivate)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  AttributionTrigger::EventLevelResult MaybeCreateEventLevelReport(
      const AttributionInfo& attribution_info,
      const StoredSource&,
      const AttributionTrigger& trigger,
      absl::optional<AttributionReport>& report,
      absl::optional<uint64_t>& dedup_key,
      absl::optional<int>& max_event_level_reports_per_destination)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  AttributionTrigger::EventLevelResult MaybeStoreEventLevelReport(
      AttributionReport& report,
      absl::optional<uint64_t> dedup_key,
      int num_conversions,
      absl::optional<AttributionReport>& replaced_report,
      absl::optional<AttributionReport>& dropped_report)
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

  // Deletes all event-level and aggregatable reports in storage for storage
  // keys matching `filter`, between `delete_begin` and `delete_end` time. More
  // specifically, this:
  // 1. Deletes all sources within the time range. If any report is attributed
  // to this source it is also deleted.
  // 2. Deletes all reports within the time range. All sources these reports
  // attributed to are also deleted.
  //
  // All sources to be deleted are updated in `source_ids_to_delete`.
  // Returns false on failure.
  [[nodiscard]] bool ClearReportsForOriginsInRange(
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter,
      std::vector<StoredSource::Id>& source_ids_to_delete,
      int& num_event_reports_deleted,
      int& num_aggregatable_reports_deleted)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool ClearReportsForSourceIds(
      const std::vector<StoredSource::Id>& source_ids,
      int& num_event_reports_deleted,
      int& num_aggregatable_reports_deleted)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Aggregate Attribution:

  // Checks if the given aggregatable attribution is allowed according to the
  // L1 budget policy specified by the delegate.
  RateLimitResult AggregatableAttributionAllowedForBudgetLimit(
      const AttributionReport::AggregatableAttributionData&
          aggregatable_attribution,
      int64_t aggregatable_budget_consumed)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Adjusts the aggregatable budget for the source event by
  // `additional_budget_consumed`.
  [[nodiscard]] bool AdjustBudgetConsumedForSource(
      StoredSource::Id source_id,
      int64_t additional_budget_consumed)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  AttributionTrigger::AggregatableResult
  MaybeCreateAggregatableAttributionReport(
      const AttributionInfo& attribution_info,
      const StoredSource&,
      const AttributionTrigger& trigger,
      absl::optional<AttributionReport>& report,
      absl::optional<uint64_t>& dedup_key,
      absl::optional<int>& max_aggregatable_reports_per_destination)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Stores the data associated with the aggregatable report, e.g. budget
  // consumed and dedup keys. The report itself will be stored in
  // `GenerateNullAggregatableReportsAndStoreReports()`.
  AttributionTrigger::AggregatableResult
  MaybeStoreAggregatableAttributionReportData(
      AttributionReport& report,
      int64_t aggregatable_budget_consumed,
      int num_aggregatable_reports,
      absl::optional<uint64_t> dedup_key,
      absl::optional<int64_t>& aggregatable_budget_per_source,
      absl::optional<int>& max_aggregatable_reports_per_source)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool StoreAttributionReport(AttributionReport& report)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Generates null aggregatable reports for the given trigger, assigns
  // verification data to null aggregatable reports and the real aggregatable
  // report if created, and stores all those reports.
  [[nodiscard]] bool GenerateNullAggregatableReportsAndStoreReports(
      const AttributionTrigger&,
      const AttributionInfo&,
      absl::optional<AttributionReport>& new_aggregatable_report,
      absl::optional<base::Time>& min_null_aggregatable_report_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Randomly assigns trigger verification data to the given reports.
  void AssignTriggerVerificationData(std::vector<AttributionReport>&,
                                     const AttributionTrigger&)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If set, database errors will not crash the client when run in debug mode.
  bool ignore_errors_for_testing_ = false;

  const base::FilePath path_to_database_;

  // Current status of the database initialization. Tracks what stage |this| is
  // at for lazy initialization, and used as a signal for if the database is
  // closed. This is initialized in the first call to LazyInit() to avoid doing
  // additional work in the constructor, see https://crbug.com/1121307.
  absl::optional<DbStatus> db_init_status_
      GUARDED_BY_CONTEXT(sequence_checker_);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<AttributionStorageDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Table which stores timestamps of sent reports, and checks if new reports
  // can be created given API rate limits. The underlying table is created in
  // |db_|, but only accessed within |RateLimitTable|.
  // `rate_limit_table_` references `delegate_` So it must be declared last and
  // deleted first.
  RateLimitTable rate_limit_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Time at which `DeleteExpiredSources()` was last called. Initialized to
  // the NULL time.
  base::Time last_deleted_expired_sources_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_H_
