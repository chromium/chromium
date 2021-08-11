// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/rate_limit_table.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"

namespace base {
class Clock;
}  // namespace base

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace content {

// Provides an implementation of ConversionStorage that is backed by SQLite.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT ConversionStorageSql : public ConversionStorage {
 public:
  static void RunInMemoryForTesting();

  ConversionStorageSql(const base::FilePath& path_to_database,
                       std::unique_ptr<Delegate> delegate,
                       const base::Clock* clock);
  ConversionStorageSql(const ConversionStorageSql& other) = delete;
  ConversionStorageSql& operator=(const ConversionStorageSql& other) = delete;
  ConversionStorageSql(ConversionStorageSql&& other) = delete;
  ConversionStorageSql& operator=(ConversionStorageSql&& other) = delete;
  ~ConversionStorageSql() override;

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

  // ConversionStorage
  void StoreImpression(const StorableImpression& impression) override;
  bool MaybeCreateAndStoreConversionReport(
      const StorableConversion& conversion) override;
  std::vector<ConversionReport> GetConversionsToReport(base::Time expiry_time,
                                                       int limit = -1) override;
  std::vector<StorableImpression> GetActiveImpressions(int limit = -1) override;
  bool DeleteConversion(int64_t conversion_id) override;
  void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter) override;

  // Variants of ClearData that assume all Origins match the filter.
  void ClearAllDataInRange(base::Time delete_begin, base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void ClearAllDataAllTime() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns false on failure.
  bool DeleteImpressions(const std::vector<int64_t>& impression_ids)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Deletes all impressions that have expired and have no pending conversion
  // reports. Returns false on failure.
  bool DeleteExpiredImpressions()
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Deletes the conversion with `conversion_id` without checking the the DB
  // initialization status or the number of deleted rows. Returns false on
  // failure.
  bool DeleteConversionInternal(int64_t conversion_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  bool HasCapacityForStoringImpression(const std::string& serialized_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;
  bool IsReportAlreadyStored(int64_t impression_id,
                             absl::optional<int64_t> dedup_key)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;
  bool HasCapacityForStoringConversion(const std::string& serialized_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  enum class MaybeReplaceLowerPriorityReportResult {
    kError,
    kAddNewReport,
    kDropNewReport,
    kReplaceOldReport,
  };
  MaybeReplaceLowerPriorityReportResult MaybeReplaceLowerPriorityReport(
      const ConversionReport& report,
      int num_conversions,
      int64_t conversion_priority)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // When storing an event-source impression, deletes active event-source
  // impressions in order by |impression_time| until there are sufficiently few
  // unique conversion destinations for the same |impression_site|.
  bool EnsureCapacityForPendingDestinationLimit(
      const StorableImpression& impression)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Stores |report| in the database, but uses |impression_id| rather than
  // |ConversionReport::impression::impression_id()|, which may be null.
  bool StoreConversionReport(const ConversionReport& report,
                             int64_t impression_id,
                             int64_t priority)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;

  // Initializes the database if necessary, and returns whether the database is
  // open. |should_create| indicates whether the database should be created if
  // it is not already.
  bool LazyInit(DbCreationPolicy creation_policy)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;
  bool InitializeSchema(bool db_empty)
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;
  bool CreateSchema()
      VALID_CONTEXT_REQUIRED(sequence_checker_) WARN_UNUSED_RESULT;
  void HandleInitializationFailure(const InitStatus status)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

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

  // Must outlive |this|.
  const base::Clock* clock_;

  std::unique_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Time at which `DeleteExpiredImpressions()` was last called. Initialized to
  // the NULL time.
  base::Time last_deleted_expired_impressions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ConversionStorageSql> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
