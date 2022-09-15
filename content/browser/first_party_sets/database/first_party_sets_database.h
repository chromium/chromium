// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
class FirstPartySetEntry;
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
  using FlattenedSets = FirstPartySetParser::SetsMap;

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

  // Stores the public First-Party Sets into database, and returns true on
  // success.  Note that calling this method will wipe out the pre-existing
  // data in the table.
  [[nodiscard]] bool SetPublicSets(const FlattenedSets& sets);

  // Stores the `sites` to be cleared for the `browser_context_id` into
  // database, and returns true on success.
  [[nodiscard]] bool InsertSitesToClear(
      const std::string& browser_context_id,
      const base::flat_set<net::SchemefulSite>& sites);

  // Stores the `browser_context_id` that has performed clearing into
  // browser_contexts_cleared table, and returns true on success.
  [[nodiscard]] bool InsertBrowserContextCleared(
      const std::string& browser_context_id);

  // Stores the policy modifications into policy_modifications table, and
  // returns true on success. Note that inserting new modifications will
  // wipe out the pre-existing ones for the given `browser_context_id`.
  [[nodiscard]] bool InsertPolicyModifications(
      const std::string& browser_context_id,
      const base::flat_map<net::SchemefulSite,
                           absl::optional<net::FirstPartySetEntry>>&
          modificatons);

  // TODO(crbug.com/1219656): Consider returning absl::nullopt for all the
  // fetching methods when having query errors

  [[nodiscard]] FlattenedSets GetPublicSets();

  // Gets the list of sites to clear for the `browser_context_id`.
  [[nodiscard]] std::vector<net::SchemefulSite> FetchSitesToClear(
      const std::string& browser_context_id);

  // Gets all the sites and mapped to the value of `run_count_`, which
  // represents the site was added into DB to be cleared in a certain browser
  // run, for the `browser_context_id`.
  [[nodiscard]] base::flat_map<net::SchemefulSite, int64_t>
  FetchAllSitesToClearFilter(const std::string& browser_context_id);

  // Gets the previously-stored policy modifications for the
  // `browser_context_id`.
  [[nodiscard]] base::flat_map<net::SchemefulSite,
                               absl::optional<net::FirstPartySetEntry>>
  FetchPolicyModifications(const std::string& browser_context_id);

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