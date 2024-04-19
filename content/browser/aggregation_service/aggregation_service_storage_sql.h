// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"
#include "sql/meta_table.h"

class GURL;

namespace base {
class Clock;
}  // namespace base

namespace sql {
class Statement;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregatableReportRequest;
struct PublicKey;
struct PublicKeyset;

// AggregationServiceStorage implementation backed by a SQLite database.
// Instances may be constructed on any sequence but must be accessed and
// destroyed on the same sequence.

// TODO(crbug.com/40191198): Support public key protocol versioning.
class CONTENT_EXPORT AggregationServiceStorageSql
    : public AggregationServiceStorage {
 public:
  // Exposed for testing.
  static const int kCurrentVersionNumber;
  static const int kCompatibleVersionNumber;
  static const int kDeprecatedVersionNumber;

  // `clock` must be a non-null pointer that is valid as long as this object.
  AggregationServiceStorageSql(
      bool run_in_memory,
      const base::FilePath& path_to_database,
      const base::Clock* clock,
      int max_stored_requests_per_reporting_origin =
          AggregationService::kMaxStoredReportsPerReportingOrigin);
  AggregationServiceStorageSql(const AggregationServiceStorageSql& other) =
      delete;
  AggregationServiceStorageSql& operator=(
      const AggregationServiceStorageSql& other) = delete;
  ~AggregationServiceStorageSql() override;

  // AggregationServiceStorage:
  std::vector<PublicKey> GetPublicKeys(const GURL& url) override;
  void SetPublicKeys(const GURL& url, const PublicKeyset& keyset) override;
  void ClearPublicKeys(const GURL& url) override;
  void ClearPublicKeysExpiredBy(base::Time delete_end) override;
  void StoreRequest(AggregatableReportRequest request) override;
  void DeleteRequest(AggregationServiceStorage::RequestId request_id) override;
  void UpdateReportForSendFailure(
      AggregationServiceStorage::RequestId request_id,
      base::Time new_report_time) override;
  std::optional<base::Time> NextReportTimeAfter(
      base::Time strictly_after_time) override;
  std::vector<AggregationServiceStorage::RequestAndId>
  GetRequestsReportingOnOrBefore(base::Time not_after_time,
                                 std::optional<int> limit) override;
  std::vector<AggregationServiceStorage::RequestAndId> GetRequests(
      const std::vector<AggregationServiceStorage::RequestId>& ids) override;
  std::optional<base::Time> AdjustOfflineReportTimes(
      base::Time now,
      base::TimeDelta min_delay,
      base::TimeDelta max_delay) override;
  void ClearDataBetween(
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter) override;
  std::set<url::Origin> GetReportRequestReportingOrigins() override;

  void set_ignore_errors_for_testing(bool ignore_for_testing)
      VALID_CONTEXT_REQUIRED(sequence_checker_) {
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
    kOpen,
    // The database has never been created, i.e. there is no database file at
    // all.
    kDeferringCreation,
    // The database exists but is not open yet.
    kDeferringOpen,
    // The database initialization failed, or the db suffered from an
    // unrecoverable, but potentially transient, error.
    kClosed,
    // The database initialization failed, or the db suffered from a
    // catastrophic failure.
    kClosedDueToCatastrophicError,
  };

  enum class DbCreationPolicy {
    // Create the db if it does not exist.
    kCreateIfAbsent,
    // Do not create the db if it does not exist.
    kFailIfAbsent,
  };

  // Inserts public keys to database.
  bool InsertPublicKeysImpl(const GURL& url, const PublicKeyset& keyset)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes all stored public keys for `url` from database.
  bool ClearPublicKeysImpl(const GURL& url)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes all stored public keys for `url_id` from database.
  bool ClearPublicKeysByUrlId(int64_t url_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears the stored public keys that were fetched between `delete_begin` and
  // `delete_end` time (inclusive). Null times are treated as unbounded lower or
  // upper range.
  void ClearPublicKeysFetchedBetween(base::Time delete_begin,
                                     base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears all stored public keys.
  void ClearAllPublicKeys() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the stored request with the given report ID.
  bool DeleteRequestImpl(RequestId request_id)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  std::optional<base::Time> NextReportTimeAfterImpl(
      base::Time strictly_after_time) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears the report requests that were stored between `delete_begin` and
  // `delete_end` time (inclusive). Null times are treated as unbounded lower or
  // upper range. If `!filter.is_null()`, only requests with reporting origins
  // matching the `filter` are cleared.
  void ClearRequestsStoredBetween(
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears all stored report requests;
  void ClearAllRequests() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Whether the reporting origin has space for an extra report to be stored,
  // i.e. has not reached the `max_stored_requests_per_reporting_origin_` limit.
  bool ReportingOriginHasCapacity(std::string_view serialized_reporting_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Initializes the database if necessary, and returns whether the database is
  // open. `creation_policy` indicates whether the database should be created if
  // it is not already.
  [[nodiscard]] bool EnsureDatabaseOpen(DbCreationPolicy creation_policy)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool InitializeSchema(bool db_empty)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void HandleInitializationFailure(InitStatus status)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // If set, database errors will not crash the client when run in debug mode.
  bool ignore_errors_for_testing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  const bool run_in_memory_;

  // This is an empty FilePath if the database is being stored in-memory.
  const base::FilePath path_to_database_;

  const raw_ref<const base::Clock> clock_;

  // No more report requests with the same reporting origin can be stored in the
  // database than this. Any additional requests attempted to be stored will
  // silently be dropped until there is more capacity.
  int max_stored_requests_per_reporting_origin_;

  // The current state of `db_`. Lazy-initialized by `EnsureDatabaseOpen()` to
  // avoid touching the filesystem in the constructor. Watch out: any time we
  // use the database, its value may be updated by `DatabaseErrorCallback()`.
  std::optional<DbStatus> db_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_
