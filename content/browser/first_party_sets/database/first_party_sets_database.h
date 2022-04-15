// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace content {

// Wraps its own `sql::Database` instance on behalf of the First-Party Sets
// database implementation. This class must be accessed and destroyed on the
// same sequence. The sequence must outlive |this|.
//
// Note that the current implementation relies on DB being accessed by a
// singleton only and is already sequence-safe.
class CONTENT_EXPORT FirstPartySetsDatabase {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InitStatus {
    // `LazyInit()` has not yet been called.
    kUnattempted = 0,
    // `LazyInit()` was successful.
    kSuccess = 1,
    // `LazyInit()` failed and a more specific error wasn't diagnosed.
    kError = 2,
    // `LazyInit()` failed due to a compatible version number being too high.
    kTooNew = 3,
    // `LazyInit()` failed due to a version number being too low.
    kTooOld = 4,
    // `LazyInit()` was successful but data is considered corrupted.
    kCorrupted = 5,

    kMaxValue = kCorrupted,
  };

  explicit FirstPartySetsDatabase(base::FilePath db_path);

  FirstPartySetsDatabase(const FirstPartySetsDatabase&) = delete;
  FirstPartySetsDatabase& operator=(const FirstPartySetsDatabase&) = delete;
  FirstPartySetsDatabase(const FirstPartySetsDatabase&&) = delete;
  FirstPartySetsDatabase& operator=(const FirstPartySetsDatabase&&) = delete;
  ~FirstPartySetsDatabase();

  // Stores the `sites` to be cleared into database, and returns true on
  // success.
  [[nodiscard]] bool InsertSitesToClear(
      const std::vector<net::SchemefulSite>& sites);

  // Stores the `profile` into database, and returns true on success.
  [[nodiscard]] bool InsertProfileCleared(const std::string& profile);

  // Gets the list of sites to clear for `profile`. Returns an empty vector if
  // `profile` does not exist in the database before.
  [[nodiscard]] std::vector<net::SchemefulSite> FetchSitesToClear(
      const std::string& profile);

 private:
  // Called at the start of each public operation, and initializes the database
  // if it isn't already initialized.
  [[nodiscard]] bool LazyInit() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Opens a persistent database with the absolute path `db_path_`, creating the
  // file if it does not yet exist. Returns whether opening was successful.
  [[nodiscard]] bool OpenDatabase() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Helper function to implement internals of `LazyInit()`.
  [[nodiscard]] InitStatus InitializeTables()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Increase the `run_count` stored in the meta table by 1. Should only be
  // called once during DB initialization.  The value of `run_count` should
  // never be negative.
  void IncreaseRunCount() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether an entry exists for `profile`.
  [[nodiscard]] bool HasEntryFor(const std::string& profile) const
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the database and returns whether the operation was successful.
  //
  // It is OK to call `Destroy()` regardless of whether db init was successful.
  [[nodiscard]] bool Destroy() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // The path to the database.
  base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The database containing the actual data. May be null if the database:
  //  - could not be opened
  //  - table/index initialization failed
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the version information and `run_count`.
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Initialization status of `db_`.
  InitStatus db_status_ GUARDED_BY_CONTEXT(sequence_checker_){
      InitStatus::kUnattempted};

  // Contains the count of the current browser run after database is initialized
  // successfully, which should be a positive number and should only be set
  // once.
  int64_t run_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_