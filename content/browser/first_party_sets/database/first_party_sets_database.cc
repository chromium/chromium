// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/database/first_party_sets_database.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/first_party_sets_validator.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 5;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
const int kDeprecatedVersionNumber = 1;

const char kRunCountKey[] = "run_count";

[[nodiscard]] bool InitSchema(sql::Database& db) {
  static constexpr char kPublicSetsSql[] =
      "CREATE TABLE IF NOT EXISTS public_sets("
      "version TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "primary_site TEXT NOT NULL,"
      "site_type INTEGER NOT NULL,"
      "PRIMARY KEY(version,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kPublicSetsSql))
    return false;

  static constexpr char kBrowserContextSetsVersionSql[] =
      "CREATE TABLE IF NOT EXISTS browser_context_sets_version("
      "browser_context_id TEXT PRIMARY KEY NOT NULL,"
      "public_sets_version TEXT NOT NULL"
      ")WITHOUT ROWID";
  if (!db.Execute(kBrowserContextSetsVersionSql))
    return false;

  static constexpr char kPublicSetsVersionBrowserContextsSql[] =
      "CREATE INDEX IF NOT EXISTS idx_public_sets_version_browser_contexts "
      "ON browser_context_sets_version(public_sets_version)";
  if (!db.Execute(kPublicSetsVersionBrowserContextsSql))
    return false;

  static constexpr char kBrowserContextSitesToClearSql[] =
      "CREATE TABLE IF NOT EXISTS browser_context_sites_to_clear("
      "browser_context_id TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "marked_at_run INTEGER NOT NULL,"
      "PRIMARY KEY(browser_context_id,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kBrowserContextSitesToClearSql))
    return false;

  static constexpr char kMarkedAtRunSitesSql[] =
      "CREATE INDEX IF NOT EXISTS idx_marked_at_run_sites "
      "ON browser_context_sites_to_clear(marked_at_run)";
  if (!db.Execute(kMarkedAtRunSitesSql))
    return false;

  static constexpr char kBrowserContextsClearedSql[] =
      "CREATE TABLE IF NOT EXISTS browser_contexts_cleared("
      "browser_context_id TEXT PRIMARY KEY NOT NULL,"
      "cleared_at_run INTEGER NOT NULL"
      ")WITHOUT ROWID";
  if (!db.Execute(kBrowserContextsClearedSql))
    return false;

  static constexpr char kClearedAtRunBrowserContextsSql[] =
      "CREATE INDEX IF NOT EXISTS idx_cleared_at_run_browser_contexts "
      "ON browser_contexts_cleared(cleared_at_run)";
  if (!db.Execute(kClearedAtRunBrowserContextsSql))
    return false;

  static constexpr char kPolicyConfigurationsSql[] =
      "CREATE TABLE IF NOT EXISTS policy_configurations("
      "browser_context_id TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "primary_site TEXT,"  // May be NULL if this row represents a deletion.
      "PRIMARY KEY(browser_context_id,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kPolicyConfigurationsSql))
    return false;

  static constexpr char kManualConfigurationsSql[] =
      "CREATE TABLE IF NOT EXISTS manual_configurations("
      "browser_context_id TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "primary_site TEXT,"  // May be NULL if this row represents a deletion.
      "site_type INTEGER,"  // May be NULL if this row represents a deletion.
      "PRIMARY KEY(browser_context_id,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kManualConfigurationsSql))
    return false;

  return true;
}

void RecordInitializationStatus(FirstPartySetsDatabase::InitStatus status) {
  base::UmaHistogramEnumeration("FirstPartySets.Database.InitStatus", status);
}

}  // namespace

FirstPartySetsDatabase::FirstPartySetsDatabase(base::FilePath db_path)
    : db_path_(std::move(db_path)) {
  CHECK(db_path_.IsAbsolute());
}

FirstPartySetsDatabase::~FirstPartySetsDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FirstPartySetsDatabase::PersistSets(
    const std::string& browser_context_id,
    const net::GlobalFirstPartySets& sets,
    const net::FirstPartySetsContextConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // Only persist public sets if the version is valid.
  if (sets.public_sets_version().IsValid() &&
      !SetPublicSets(browser_context_id, sets)) {
    return false;
  }

  if (!InsertManualConfiguration(browser_context_id, sets)) {
    return false;
  }

  if (!InsertPolicyConfigurations(browser_context_id, config)) {
    return false;
  }

  return transaction.Commit();
}

bool FirstPartySetsDatabase::SetPublicSets(
    const std::string& browser_context_id,
    const net::GlobalFirstPartySets& sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(db_->HasActiveTransactions());
  CHECK(sets.public_sets_version().IsValid());

  const std::string& version = sets.public_sets_version().GetString();
  // Checks if the version of the current public sets is referenced by *any*
  // browser context in the public_sets_version table. If so, that means the
  // sets already exist in public_sets table and we don't need to write them to
  // public_sets table again.
  static constexpr char kCheckSql[] =
      "SELECT 1 FROM browser_context_sets_version WHERE public_sets_version=?"
      "LIMIT 1";
  sql::Statement check_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCheckSql));
  check_statement.BindString(0, version);
  const bool has_matching_version = check_statement.Step();
  if (!check_statement.Succeeded())
    return false;

  if (!has_matching_version) {
    if (!sets.ForEachPublicSetEntry(
            [&](const net::SchemefulSite& site,
                const net::FirstPartySetEntry& entry) -> bool {
              CHECK(!site.opaque());
              CHECK(!entry.primary().opaque());
              DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
              static constexpr char kInsertSql[] =
                  "INSERT INTO public_sets(version,site,primary_site,site_type)"
                  "VALUES(?,?,?,?)";
              sql::Statement insert_statement(
                  db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
              insert_statement.BindString(0, version);
              insert_statement.BindString(1, site.Serialize());
              insert_statement.BindString(2, entry.primary().Serialize());
              insert_statement.BindInt(3, static_cast<int>(entry.site_type()));

              return insert_statement.Run();
            })) {
      return false;
    }
  }

  // Keeps track of the version used by the given `browser_context_id` in
  // browser_context_sets_version table.
  static constexpr char kInsertSql[] =
      "INSERT OR REPLACE INTO browser_context_sets_version"
      "(browser_context_id,public_sets_version)VALUES(?,?)";
  sql::Statement insert_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  insert_statement.BindString(0, browser_context_id);
  insert_statement.BindString(1, version);

  if (!insert_statement.Run())
    return false;

  // TODO(shuuran): Garbage collect the public sets no longer used by any
  // browser_context_id.

  return !TransactionFailed();
}

bool FirstPartySetsDatabase::InsertSitesToClear(
    const std::string& browser_context_id,
    const base::flat_set<net::SchemefulSite>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  for (const auto& site : sites) {
    CHECK(!site.opaque());
    static constexpr char kInsertSql[] =
        // clang-format off
        "INSERT OR REPLACE INTO browser_context_sites_to_clear"
        "(browser_context_id,site,marked_at_run)"
        "VALUES(?,?,?)";
    // clang-format on
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
    statement.BindString(0, browser_context_id);
    statement.BindString(1, site.Serialize());
    statement.BindInt64(2, run_count_);

    if (!statement.Run())
      return false;
  }
  return transaction.Commit();
}

bool FirstPartySetsDatabase::InsertBrowserContextCleared(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());

  if (!LazyInit())
    return false;

  static constexpr char kInsertBrowserContextsClearedSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO browser_contexts_cleared(browser_context_id,cleared_at_run)"
      "VALUES(?,?)";
  // clang-format on
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertBrowserContextsClearedSql));
  statement.BindString(0, browser_context_id);
  statement.BindInt64(1, run_count_);

  return statement.Run();
}

bool FirstPartySetsDatabase::InsertPolicyConfigurations(
    const std::string& browser_context_id,
    const net::FirstPartySetsContextConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(db_->HasActiveTransactions());

  static constexpr char kDeleteSql[] =
      "DELETE FROM policy_configurations WHERE browser_context_id=?";
  sql::Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  delete_statement.BindString(0, browser_context_id);
  if (!delete_statement.Run())
    return false;

  return config.ForEachCustomizationEntry(
             [&](const net::SchemefulSite& site,
                 const net::FirstPartySetEntryOverride& entry_override)
                 -> bool {
               DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
               CHECK(!site.opaque());
               static constexpr char kInsertSql[] =
                   "INSERT INTO "
                   "policy_configurations(browser_context_id,site,primary_site)"
                   "VALUES(?,?,?)";
               sql::Statement insert_statement(
                   db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
               insert_statement.BindString(0, browser_context_id);
               insert_statement.BindString(1, site.Serialize());
               if (!entry_override.IsDeletion()) {
                 insert_statement.BindString(
                     2, entry_override.GetEntry().primary().Serialize());
               } else {
                 insert_statement.BindNull(2);
               }
               return insert_statement.Run();
             }) &&
         !TransactionFailed();
}

bool FirstPartySetsDatabase::InsertManualConfiguration(
    const std::string& browser_context_id,
    const net::GlobalFirstPartySets& global_first_party_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(db_->HasActiveTransactions());

  static constexpr char kDeleteSql[] =
      "DELETE FROM manual_configurations WHERE browser_context_id=?";
  sql::Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  delete_statement.BindString(0, browser_context_id);
  if (!delete_statement.Run())
    return false;

  global_first_party_sets.ForEachManualConfigEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntryOverride& entry_override) -> bool {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        CHECK(!site.opaque());
        static constexpr char kInsertSql[] =
            "INSERT INTO manual_configurations"
            "(browser_context_id,site,primary_site,site_type)"
            "VALUES(?,?,?,?)";
        sql::Statement insert_statement(
            db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
        insert_statement.BindString(0, browser_context_id);
        insert_statement.BindString(1, site.Serialize());
        if (!entry_override.IsDeletion()) {
          insert_statement.BindString(
              2, entry_override.GetEntry().primary().Serialize());
          insert_statement.BindInt(
              3, static_cast<int>(entry_override.GetEntry().site_type()));
        } else {
          insert_statement.BindNull(2);
          insert_statement.BindNull(3);
        }
        return insert_statement.Run();
      });
  return !TransactionFailed();
}

std::optional<
    std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
FirstPartySetsDatabase::GetGlobalSetsAndConfig(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (!LazyInit())
    return std::nullopt;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return std::nullopt;

  std::optional<net::GlobalFirstPartySets> global_sets =
      GetGlobalSets(browser_context_id);
  if (!global_sets.has_value()) {
    return std::nullopt;
  }

  std::optional<net::FirstPartySetsContextConfig> config =
      FetchPolicyConfigurations(browser_context_id);
  if (!config.has_value()) {
    return std::nullopt;
  }

  if (!transaction.Commit())
    return std::nullopt;

  return std::make_pair(std::move(global_sets).value(),
                        std::move(config).value());
}

std::optional<net::GlobalFirstPartySets> FirstPartySetsDatabase::GetGlobalSets(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_->HasActiveTransactions());
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(!browser_context_id.empty());

  // Query public sets entries.
  static constexpr char kVersionSql[] =
      "SELECT public_sets_version FROM browser_context_sets_version "
      "WHERE browser_context_id=?";
  sql::Statement version_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kVersionSql));
  version_statement.BindString(0, browser_context_id);

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> sets;
  std::string version;
  if (version_statement.Step()) {
    version = version_statement.ColumnString(0);

    static constexpr char kSelectSql[] =
        "SELECT site,primary_site,site_type FROM public_sets WHERE version=?";
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
    statement.BindString(0, version);

    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> entries;
    net::FirstPartySetsValidator validator;

    while (statement.Step()) {
      std::optional<net::SchemefulSite> site =
          FirstPartySetParser::CanonicalizeRegisteredDomain(
              statement.ColumnString(0), /*emit_errors=*/false);

      std::optional<net::SchemefulSite> primary =
          FirstPartySetParser::CanonicalizeRegisteredDomain(
              statement.ColumnString(1), /*emit_errors=*/false);

      std::optional<net::SiteType> site_type =
          net::FirstPartySetEntry::DeserializeSiteType(statement.ColumnInt(2));

      // TODO(crbug.com/40221249): Invalid entries should be rare case but
      // possible. Consider deleting them from DB.
      if (site.has_value() && primary.has_value() && site_type.has_value()) {
        entries.emplace_back(
            site.value(),
            net::FirstPartySetEntry(primary.value(), site_type.value(),
                                    /*site_index=*/std::nullopt));
        validator.Update(site.value(), primary.value());
      }
    }

    sets = base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>(
        std::move(entries));
    // Make sure the global sets read from DB does not have any singleton or
    // orphan.
    if (!validator.IsValid()) {
      base::EraseIf(
          sets, [&validator](const std::pair<net::SchemefulSite,
                                             net::FirstPartySetEntry>& pair) {
            return !validator.IsSitePrimaryValid(pair.second.primary());
          });
    }

    if (!statement.Succeeded())
      return std::nullopt;
  }
  if (!version_statement.Succeeded() || TransactionFailed()) {
    return std::nullopt;
  }

  // Aliases are merged with entries inside of the public sets table so it is
  // sufficient to declare the global sets object with only the entries field.
  net::GlobalFirstPartySets global_sets(base::Version(version), sets,
                                        /*aliases=*/{});

  // Query & apply manual configuration. Safe because this config and this
  // public sets data were written during the same run of Chrome, and the config
  // was computed from that data.
  std::optional<net::FirstPartySetsContextConfig> manual_config =
      FetchManualConfiguration(browser_context_id);
  if (!manual_config.has_value()) {
    return std::nullopt;
  }
  global_sets.UnsafeSetManualConfig(std::move(manual_config).value());

  return global_sets;
}

std::optional<
    std::pair<std::vector<net::SchemefulSite>, net::FirstPartySetsCacheFilter>>
FirstPartySetsDatabase::GetSitesToClearFilters(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (!LazyInit())
    return std::nullopt;

  CHECK_GT(run_count_, 0);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return std::nullopt;

  std::optional<std::vector<net::SchemefulSite>> sites_to_clear =
      FetchSitesToClear(browser_context_id);
  if (!sites_to_clear.has_value()) {
    return std::nullopt;
  }

  std::optional<base::flat_map<net::SchemefulSite, int64_t>>
      all_sites_to_clear = FetchAllSitesToClearFilter(browser_context_id);
  if (!all_sites_to_clear.has_value()) {
    return std::nullopt;
  }

  net::FirstPartySetsCacheFilter cache_filter =
      all_sites_to_clear->empty()
          ? net::FirstPartySetsCacheFilter()
          : net::FirstPartySetsCacheFilter(
                std::move(all_sites_to_clear).value(), run_count_);

  if (!transaction.Commit())
    return std::nullopt;

  return std::make_pair(std::move(sites_to_clear).value(),
                        std::move(cache_filter));
}

std::optional<std::vector<net::SchemefulSite>>
FirstPartySetsDatabase::FetchSitesToClear(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(db_->HasActiveTransactions());

  // Gets the sites that were marked to clear but haven't been cleared yet for
  // the given `browser_context_id`. Use 0 as the default
  // `browser_contexts_cleared.cleared_at_run` value if the `browser_context_id`
  // does not exist in the browser_contexts_cleared table.
  std::vector<net::SchemefulSite> results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT p.site FROM browser_context_sites_to_clear p "
      "LEFT JOIN browser_contexts_cleared c ON p.browser_context_id=c.browser_context_id "
      "WHERE p.marked_at_run>COALESCE(c.cleared_at_run,0)"
      "AND p.browser_context_id=?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    std::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);
    // TODO(crbug.com/40221249): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      results.push_back(std::move(site).value());
    }
  }

  if (!statement.Succeeded() || TransactionFailed()) {
    return std::nullopt;
  }

  return results;
}

std::optional<base::flat_map<net::SchemefulSite, int64_t>>
FirstPartySetsDatabase::FetchAllSitesToClearFilter(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(db_->HasActiveTransactions());

  std::vector<std::pair<net::SchemefulSite, int64_t>> results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT site,marked_at_run FROM browser_context_sites_to_clear "
      "WHERE browser_context_id=?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    std::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);
    // TODO(crbug.com/40221249): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      results.emplace_back(std::move(site).value(), statement.ColumnInt(1));
    }
  }

  if (!statement.Succeeded() || TransactionFailed()) {
    return std::nullopt;
  }

  return results;
}

std::optional<net::FirstPartySetsContextConfig>
FirstPartySetsDatabase::FetchPolicyConfigurations(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_->HasActiveTransactions());
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(!browser_context_id.empty());

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntryOverride>>
      results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT site,primary_site FROM policy_configurations "
      "WHERE browser_context_id=?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    std::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);

    std::optional<net::SchemefulSite> maybe_primary_site;
    if (std::string primary_site = statement.ColumnString(1);
        !primary_site.empty()) {
      maybe_primary_site = FirstPartySetParser::CanonicalizeRegisteredDomain(
          primary_site, /*emit_errors=*/false);
    }

    // TODO(crbug.com/40221249): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      net::FirstPartySetEntryOverride entry_override;
      if (maybe_primary_site.has_value()) {
        entry_override =
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                maybe_primary_site.value(),
                // TODO(crbug.com/40186153): May change to use the
                // real site_type and site_index in the future, depending on
                // the design details. Use kAssociated as default site type
                // and null site index for now.
                net::SiteType::kAssociated, std::nullopt));
      }
      results.emplace_back(std::move(site).value(), std::move(entry_override));
    }
  }
  if (!statement.Succeeded() || TransactionFailed()) {
    return std::nullopt;
  }

  return net::FirstPartySetsContextConfig(std::move(results));
}

bool FirstPartySetsDatabase::HasEntryInBrowserContextsClearedForTesting(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());

  if (!LazyInit())
    return {};

  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT 1 FROM browser_contexts_cleared "
      "WHERE browser_context_id=? LIMIT 1";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  return statement.Step() && statement.Succeeded();
}

std::optional<net::FirstPartySetsContextConfig>
FirstPartySetsDatabase::FetchManualConfiguration(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_->HasActiveTransactions());
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  CHECK(!browser_context_id.empty());

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntryOverride>>
      results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT site,primary_site,site_type FROM manual_configurations "
      "WHERE browser_context_id=?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    std::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);

    std::optional<net::SchemefulSite> maybe_primary_site;
    std::optional<net::SiteType> maybe_site_type;
    // DB entry for "deleted"  site will have null `primary_site` and
    // `site_type`.
    if (std::string primary_site = statement.ColumnString(1);
        !primary_site.empty()) {
      maybe_primary_site = FirstPartySetParser::CanonicalizeRegisteredDomain(
          primary_site, /*emit_errors=*/false);

      maybe_site_type =
          net::FirstPartySetEntry::DeserializeSiteType(statement.ColumnInt(2));
    }

    // TODO(crbug.com/40221249): Invalid entries should be rare case but
    // possible. Consider deleting them from DB.
    if (site.has_value()) {
      net::FirstPartySetEntryOverride entry_override;
      if (maybe_primary_site.has_value() && maybe_site_type.has_value()) {
        entry_override =
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                maybe_primary_site.value(),
                // TODO(crbug.com/40186153): May change to use the
                // real site_index in the future, depending on the design
                // details. Use null site index for now.
                maybe_site_type.value(), std::nullopt));
      }
      results.emplace_back(std::move(site).value(), std::move(entry_override));
    }
  }

  if (!statement.Succeeded() || TransactionFailed()) {
    return std::nullopt;
  }

  return net::FirstPartySetsContextConfig(std::move(results));
}

bool FirstPartySetsDatabase::LazyInit() {
  // Early return in case of previous failure, to prevent an unbounded
  // number of re-attempts.
  if (db_status_ != InitStatus::kUnattempted)
    return db_status_ == InitStatus::kSuccess;

  CHECK_EQ(db_.get(), nullptr);
  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions{.page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("FirstPartySets");
  // base::Unretained is safe here because this FirstPartySetsDatabase owns
  // the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_->set_error_callback(base::BindRepeating(
      &FirstPartySetsDatabase::DatabaseErrorCallback, base::Unretained(this)));
  db_status_ = InitializeTables();

  if (db_status_ != InitStatus::kSuccess) {
    db_.reset();
    meta_table_.Reset();
  } else {
    IncreaseRunCount();
  }

  RecordInitializationStatus(db_status_);
  return db_status_ == InitStatus::kSuccess;
}

bool FirstPartySetsDatabase::OpenDatabase() {
  CHECK(db_);
  if (db_->is_open() || db_->Open(db_path_)) {
    db_->Preload();
    return true;
  }
  return false;
}

void FirstPartySetsDatabase::DatabaseErrorCallback(int extended_error,
                                                   sql::Statement* stmt) {
  CHECK(db_);
  // Attempt to recover a corrupt database, if it is eligible to be recovered.
  if (sql::Recovery::RecoverIfPossible(
          db_.get(), extended_error,
          sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze)) {
    // Recovery was attempted. The database handle has been poisoned and the
    // error callback has been reset.

    // Signal the test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);

    // Update db status since `RecoverIfPossible` poisoned the db handle
    // already.
    db_status_ = InitStatus::kError;
    return;
  }

  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(ERROR) << db_->GetErrorMessage();

  // Consider the database closed if we did not attempt to recover so we did not
  // produce further errors.
  db_status_ = InitStatus::kError;
}

FirstPartySetsDatabase::InitStatus FirstPartySetsDatabase::InitializeTables() {
  if (!OpenDatabase())
    return InitStatus::kError;

  // Database should now be open.
  CHECK(db_->is_open());

  // Razes the DB if the version is deprecated or too new to get the feature
  // working.
  CHECK_LT(kDeprecatedVersionNumber, kCurrentVersionNumber);
  if (sql::MetaTable::RazeIfIncompatible(
          db_.get(), /*lowest_supported_version=*/kDeprecatedVersionNumber + 1,
          kCurrentVersionNumber) == sql::RazeIfIncompatibleResult::kFailed) {
    return InitStatus::kError;
  }

  // db could have been razed due to version being deprecated or too new.
  bool db_empty = !sql::MetaTable::DoesTableExist(db_.get());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(WARNING) << "First-Party Sets database begin initialization failed.";
    db_->RazeAndPoison();
    return InitStatus::kError;
  }

  // Use the current version for `compatible_version`.
  if (!meta_table_.Init(db_.get(), /*version=*/kCurrentVersionNumber,
                        /*compatible_version=*/kCurrentVersionNumber)) {
    return InitStatus::kError;
  }

  // Create the tables if db not already exists.
  if (db_empty) {
    if (!InitSchema(*db_))
      return InitStatus::kError;
  } else {
    if (!UpgradeSchema())
      return InitStatus::kError;
  }

  if (!transaction.Commit()) {
    LOG(WARNING) << "First-Party Sets database initialization commit failed.";
    return InitStatus::kError;
  }

  return InitStatus::kSuccess;
}

bool FirstPartySetsDatabase::UpgradeSchema() {
  if (meta_table_.GetVersionNumber() == 2 && !MigrateToVersion3())
    return false;

  if (meta_table_.GetVersionNumber() == 3 && !MigrateToVersion4())
    return false;

  if (meta_table_.GetVersionNumber() == 4 && !MigrateToVersion5()) {
    return false;
  }

  // Add similar if () blocks for new versions here.

  return true;
}

bool FirstPartySetsDatabase::MigrateToVersion3() {
  CHECK(db_->HasActiveTransactions());
  // Rename the policy_modifications table with policy_configurations.
  static constexpr char kRenamePolicyConfigurationsTableSql[] =
      "ALTER TABLE policy_modifications RENAME TO policy_configurations";
  if (!db_->Execute(kRenamePolicyConfigurationsTableSql))
    return false;

  return meta_table_.SetVersionNumber(3) &&
         meta_table_.SetCompatibleVersionNumber(3);
}

bool FirstPartySetsDatabase::MigrateToVersion4() {
  CHECK(db_->HasActiveTransactions());
  // Create manual_configurations table; transfer data from manual_sets table to
  // manual_configurations table; drop manual_sets table.
  bool success = db_->Execute(
                     "CREATE TABLE manual_configurations("
                     "browser_context_id TEXT NOT NULL,"
                     "site TEXT NOT NULL,"
                     "primary_site TEXT,"  // May be NULL if this row represents
                                           // a deletion.
                     "site_type INTEGER,"  // May be NULL if this row represents
                                           // a deletion.
                     "PRIMARY KEY(browser_context_id,site)"
                     ")WITHOUT ROWID") &&
                 db_->Execute(
                     "INSERT INTO manual_configurations"
                     "(browser_context_id,site,primary_site,site_type) "
                     "SELECT browser_context_id,site,primary_site,site_type "
                     "FROM manual_sets") &&
                 db_->Execute("DROP TABLE manual_sets");

  return success && meta_table_.SetVersionNumber(4) &&
         meta_table_.SetCompatibleVersionNumber(4);
}

bool FirstPartySetsDatabase::MigrateToVersion5() {
  CHECK(db_->HasActiveTransactions());
  // Only updates the versions in the meta table for fixing crbug.com/1409117.
  return meta_table_.SetVersionNumber(5) &&
         meta_table_.SetCompatibleVersionNumber(5);
}

void FirstPartySetsDatabase::IncreaseRunCount() {
  CHECK_EQ(db_status_, InitStatus::kSuccess);
  // 0 is the default value, `run_count_` should only be set once.
  CHECK_EQ(run_count_, 0);

  int64_t count = 0;
  // `count` should be positive if the value exists in the meta table. Consider
  // db data is corrupted and delete db file if that's not the case.
  if (meta_table_.GetValue(kRunCountKey, &count) && count <= 0) {
    db_status_ = InitStatus::kCorrupted;
    // TODO(crbug.com/40222048): Need to resolve how the restarted `run_count_`
    // could affect cache clearing.
    if (!Destroy()) {
      LOG(ERROR) << "First-Party Sets database destruction failed.";
    }
    return;
  }

  run_count_ = count + 1;
  // TODO(crbug.com/40221249): Figure out how to handle run_count update
  // failure.
  if (!meta_table_.SetValue(kRunCountKey, run_count_)) {
    LOG(ERROR) << "First-Party Sets database updating run_count failed.";
  }
}

bool FirstPartySetsDatabase::Destroy() {
  // Reset the value.
  run_count_ = 0;

  if (db_ && db_->is_open() && !db_->RazeAndPoison()) {
    return false;
  }

  // The file already doesn't exist.
  if (db_path_.empty())
    return true;

  return sql::Database::Delete(db_path_);
}

bool FirstPartySetsDatabase::TransactionFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool failed =
      !db_->HasActiveTransactions() || db_status_ != InitStatus::kSuccess;

  base::UmaHistogramBoolean("FirstPartySets.Database.TransactionFailed",
                            failed);
  return failed;
}

}  // namespace content
