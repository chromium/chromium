// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_DATABASE_FIRST_PARTY_SETS_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"

namespace net {
class FirstPartySetsCacheFilter;
class FirstPartySetsContextConfig;
class GlobalFirstPartySets;
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
    // kTooNew = 3, // No longer used
    // `LazyInit()` failed due to a version number being too low.
    // kTooOld = 4,  // No longer used
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

  // Stores the overall First-Party Sets for the given `browser_context_id` into
  // database in one transaction.
  [[nodiscard]] bool PersistSets(
      const std::string& browser_context_id,
      const net::GlobalFirstPartySets& sets,
      const net::FirstPartySetsContextConfig& config);

  // Stores the `sites` to be cleared for the `browser_context_id` into
  // database, and returns true on success.
  [[nodiscard]] bool InsertSitesToClear(
      const std::string& browser_context_id,
      const base::flat_set<net::SchemefulSite>& sites);

  // Stores the `browser_context_id` that has performed clearing into
  // browser_contexts_cleared table, and returns true on success.
  [[nodiscard]] bool InsertBrowserContextCleared(
      const std::string& browser_context_id);

  // Gets the global First-Party Sets and the config used by
  // `browser_context_id`.
  [[nodiscard]] std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetGlobalSetsAndConfig(const std::string& browser_context_id);

  // Gets the sites to clear filters. The first filter holds the list of sites
  // that haven't had their cookies/storage cleared, the second filter is the
  // cache filter that holds the current `run_count_` and a map of sites to
  // their `marked_at_run`, containing all the sites that were added into DB to
  // be cleared in a certain browser run, for the `browser_context_id`.
  [[nodiscard]] std::optional<std::pair<std::vector<net::SchemefulSite>,
                                        net::FirstPartySetsCacheFilter>>
  GetSitesToClearFilters(const std::string& browser_context_id);

  // Check whether the `browser_context_id`  has performed clearing.
  [[nodiscard]] bool HasEntryInBrowserContextsClearedForTesting(
      const std::string& browser_context_id);

 private:
  // Returns true if there is no active transaction or `db_status` is not
  // `kSuccess`. Should only be called within a transaction.
  [[nodiscard]] bool TransactionFailed();

  // Stores the public First-Party Sets into database, and keeps track of the
  // the sets version used by `browser_context_id`. `sets_version` must be
  // valid. Returns true on success.
  [[nodiscard]] bool SetPublicSets(const std::string& browser_context_id,
                                   const net::GlobalFirstPartySets& sets);

  // Stores the manual configuration into manual_configurations table, and
  // returns true on success. Inserting new manual configuration will wipe out
  // pre-existing entries for the given 'browser_context_id'
  [[nodiscard]] bool InsertManualConfiguration(
      const std::string& browser_context_id,
      const net::GlobalFirstPartySets& global_first_party_sets);

  // Stores the policy configurations into policy_configurations table, and
  // returns true on success. Note that inserting new configurations will
  // wipe out the pre-existing ones for the given `browser_context_id`.
  [[nodiscard]] bool InsertPolicyConfigurations(
      const std::string& browser_context_id,
      const net::FirstPartySetsContextConfig& policy_config);

  // Gets the global First-Party Sets used by `browser_context_id`.
  [[nodiscard]] std::optional<net::GlobalFirstPartySets> GetGlobalSets(
      const std::string& browser_context_id);

  // Gets the previously-stored manual configuration for the
  // `browser_context_id`.
  [[nodiscard]] std::optional<net::FirstPartySetsContextConfig>
  FetchManualConfiguration(const std::string& browser_context_id);

  // Gets the previously-stored policy configuration for the
  // `browser_context_id`.
  [[nodiscard]] std::optional<net::FirstPartySetsContextConfig>
  FetchPolicyConfigurations(const std::string& browser_context_id);

  // Gets the list of sites to clear for the `browser_context_id`.
  [[nodiscard]] std::optional<std::vector<net::SchemefulSite>>
  FetchSitesToClear(const std::string& browser_context_id);

  // Gets all the sites and mapped to the value of `run_count_`, which
  // represents the site was added into DB to be cleared in a certain browser
  // run, for the `browser_context_id`.
  [[nodiscard]] std::optional<base::flat_map<net::SchemefulSite, int64_t>>
  FetchAllSitesToClearFilter(const std::string& browser_context_id);

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

  // Upgrades `db_` to the latest schema, and updates the version stored in
  // `meta_table_` accordingly. Returns false on failure.
  [[nodiscard]] bool UpgradeSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool MigrateToVersion3()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool MigrateToVersion4()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool MigrateToVersion5()
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
