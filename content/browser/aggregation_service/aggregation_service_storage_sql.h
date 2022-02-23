// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_

#include "stdint.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/common/content_export.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Clock;
}  // namespace base

namespace sql {
class Statement;
}  // namespace sql

namespace content {

struct PublicKey;
struct PublicKeyset;

// AggregationServiceKeyStorage implementation backed by a SQLite database.
// Instances may be constructed on any sequence but must be accessed and
// destroyed on the same sequence.

// TODO(crbug.com/1232608): Support public key protocol versioning.
class CONTENT_EXPORT AggregationServiceStorageSql
    : public AggregationServiceKeyStorage {
 public:
  // `clock` must be a non-null pointer that is valid as long as this object.
  AggregationServiceStorageSql(bool run_in_memory,
                               const base::FilePath& path_to_database,
                               const base::Clock* clock);
  AggregationServiceStorageSql(const AggregationServiceStorageSql& other) =
      delete;
  AggregationServiceStorageSql& operator=(
      const AggregationServiceStorageSql& other) = delete;
  ~AggregationServiceStorageSql() override;

  // AggregationServiceKeyStorage:
  std::vector<PublicKey> GetPublicKeys(const GURL& url) override;
  void SetPublicKeys(const GURL& url, const PublicKeyset& keyset) override;
  void ClearPublicKeys(const GURL& url) override;
  void ClearPublicKeysFetchedBetween(base::Time delete_begin,
                                     base::Time delete_end) override;
  void ClearPublicKeysExpiredBy(base::Time delete_end) override;

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
    // unrecoverable error.
    kClosed,
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

  // Clears all stored public keys.
  void ClearAllPublicKeys() VALID_CONTEXT_REQUIRED(sequence_checker_);

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

  const base::Clock& clock_;

  // Current status of the database initialization. Tracks what stage `this` is
  // at for lazy initialization, and used as a signal for if the database is
  // closed. This is initialized in the first call to EnsureDatabaseOpen() to
  // avoid doing additional work in the constructor.
  absl::optional<DbStatus> db_init_status_
      GUARDED_BY_CONTEXT(sequence_checker_);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_SQL_H_
