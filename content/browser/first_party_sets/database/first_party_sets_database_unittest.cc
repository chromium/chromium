// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/database/first_party_sets_database.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Version number of the database.
const int kCurrentVersionNumber = 5;

static const size_t kTableCount = 7u;

int VersionFromMetaTable(sql::Database& db) {
  // Get version.
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step())
    return 0;
  return s.ColumnInt(0);
}

int CompatibleVersionFromMetaTable(sql::Database& db) {
  // Get last_compatible_version.
  sql::Statement s(db.GetUniqueStatement(
      "SELECT value FROM meta WHERE key='last_compatible_version'"));
  if (!s.Step()) {
    return 0;
  }
  return s.ColumnInt(0);
}

}  // namespace

class FirstPartySetsDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestFirstPartySets.db");
  }

  void TearDown() override {
    db_.reset();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void OpenDatabase() {
    db_ = std::make_unique<FirstPartySetsDatabase>(db_path());
  }

  void CloseDatabase() { db_.reset(); }

  static base::FilePath GetSqlFilePath(std::string_view sql_file_name) {
    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    path = path.AppendASCII("content/test/data/first_party_sets/");
    path = path.AppendASCII(sql_file_name);
    EXPECT_TRUE(base::PathExists(path));
    return path;
  }

  static base::FilePath GetCurrentVersionSqlFilePath() {
    return GetSqlFilePath(base::StringPrintf("v%d.sql", kCurrentVersionNumber));
  }

  size_t CountPublicSetsEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "public_sets", &size));
    return size;
  }

  size_t CountBrowserContextSetsVersionEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(
        sql::test::CountTableRows(db, "browser_context_sets_version", &size));
    return size;
  }

  size_t CountBrowserContextSitesToClearEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(
        sql::test::CountTableRows(db, "browser_context_sites_to_clear", &size));
    return size;
  }

  size_t CountBrowserContextsClearedEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(
        sql::test::CountTableRows(db, "browser_contexts_cleared", &size));
    return size;
  }

  size_t CountPolicyConfigurationsEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "policy_configurations", &size));
    return size;
  }

  size_t CountManualConfigurationsEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "manual_configurations", &size));
    return size;
  }

  const base::FilePath& db_path() const { return db_path_; }
  FirstPartySetsDatabase* db() { return db_.get(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<FirstPartySetsDatabase> db_;
};

TEST_F(FirstPartySetsDatabaseTest, CreateDB_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

  OpenDatabase();
  CloseDatabase();
  // An unused FirstPartySetsDatabase instance should not create the database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("FirstPartySets.Database.InitStatus", 0);

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear("b", {}));
  EXPECT_TRUE(base::PathExists(db_path()));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
  CloseDatabase();

  // Create a db handle to the existing db file to verify schemas.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  // [public_sets], [browser_context_sets_version], [policy_configurations],
  // [manual_configurations], [browser_context_sites_to_clear],
  // [browser_contexts_cleared], and [meta].
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
  // [idx_public_sets_version_browser_contexts], [idx_marked_at_run_sites],
  // [idx_cleared_at_run_browser_contexts], and [sqlite_autoindex_meta_1].
  EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));
  // `version`, `site`, `primary`, `site_type`.
  EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "public_sets"));
  // `browser_context_id`, `public_sets_version`.
  EXPECT_EQ(2u,
            sql::test::CountTableColumns(&db, "browser_context_sets_version"));
  // `browser_context_id`, `site`, `marked_at_run`.
  EXPECT_EQ(
      3u, sql::test::CountTableColumns(&db, "browser_context_sites_to_clear"));
  // `browser_context_id`, `cleared_at_run`.
  EXPECT_EQ(2u, sql::test::CountTableColumns(&db, "browser_contexts_cleared"));
  // `browser_context_id`, `site`, `primary_site`.
  EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "policy_configurations"));
  EXPECT_EQ(0u, CountPublicSetsEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextSetsVersionEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  EXPECT_EQ(0u, CountManualConfigurationsEntries(&db));
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_CurrentVersion_Success) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear("b", {}));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  EXPECT_EQ(3u, CountBrowserContextSetsVersionEntries(&db));
  EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
  EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));
  EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_RecreateOnTooOld) {
  base::HistogramTester histograms;
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear(
      "b", {net::SchemefulSite(GURL("https://example.com"))}));
  CloseDatabase();

  // Expect that the original database was razed and the initialization is
  // successful with newly inserted data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountPublicSetsEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextSetsVersionEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  EXPECT_EQ(0u, CountManualConfigurationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_RecreateOnTooNew) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v1.init_too_new.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear(
      "b", {net::SchemefulSite(GURL("https://example.com"))}));
  CloseDatabase();

  // Expect that the original database was razed and the initialization is
  // successful with newly inserted data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountPublicSetsEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextSetsVersionEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  EXPECT_EQ(0u, CountManualConfigurationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_InvalidRunCount_Fail) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v1.init_invalid_run_count.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization. Expect that the initialization was
  // unsuccessful.
  EXPECT_FALSE(db()->InsertSitesToClear("b", {}));
  CloseDatabase();

  // The original database was destroyed.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(0u, sql::test::CountSQLTables(&db));
  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kCorrupted,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, PersistSets_NoPreExistingDB) {
  const base::Version version("0.0.1");
  const std::string browser_context_id = "b";
  const std::string site = "https://aaa.test";
  const std::string primary = "https://bbb.test";
  const std::string manual_site = "https://aaa.test";
  const std::string manual_primary = "https://bbb.test";

  const std::string primary_site = "https://example.test";
  const std::string site_member1 = "https://member1.test";
  const std::string site_member2 = "https://member2.test";

  net::GlobalFirstPartySets global_sets(
      version,
      /*entries=*/
      {{net::SchemefulSite(GURL(site)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kAssociated, std::nullopt)},
       {net::SchemefulSite(GURL(primary)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kPrimary, std::nullopt)}},
      /*aliases=*/{});
  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> manual_sets = {
      {net::SchemefulSite(GURL(manual_site)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kAssociated, std::nullopt)},
      {net::SchemefulSite(GURL(manual_primary)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kPrimary, std::nullopt)}};
  global_sets.ApplyManuallySpecifiedSet(
      net::LocalSetDeclaration(/*set_entries=*/manual_sets, /*aliases=*/{}));

  net::FirstPartySetsContextConfig config(
      {{net::SchemefulSite(GURL(site_member1)),
        net::FirstPartySetEntryOverride(
            net::FirstPartySetEntry(net::SchemefulSite(GURL(primary_site)),
                                    net::SiteType::kAssociated, std::nullopt))},
       {net::SchemefulSite(GURL(site_member2)),
        net::FirstPartySetEntryOverride()}});

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->PersistSets(browser_context_id, global_sets, config));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));
  EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));

  // ============ Verify persisting public sets
  static constexpr char kSelectPublicSetsSql[] =
      "SELECT version,site,primary_site,site_type FROM public_sets";
  sql::Statement s_public_sets(db.GetUniqueStatement(kSelectPublicSetsSql));
  EXPECT_TRUE(s_public_sets.Step());
  EXPECT_EQ(version.GetString(), s_public_sets.ColumnString(0));
  EXPECT_EQ(site, s_public_sets.ColumnString(1));
  EXPECT_EQ(primary, s_public_sets.ColumnString(2));
  EXPECT_EQ(1, s_public_sets.ColumnInt(3));

  EXPECT_TRUE(s_public_sets.Step());
  EXPECT_EQ(version.GetString(), s_public_sets.ColumnString(0));
  EXPECT_EQ(primary, s_public_sets.ColumnString(1));
  EXPECT_EQ(primary, s_public_sets.ColumnString(2));
  EXPECT_EQ(0, s_public_sets.ColumnInt(3));

  EXPECT_FALSE(s_public_sets.Step());

  static constexpr char kVersionSql[] =
      "SELECT browser_context_id,public_sets_version "
      "FROM browser_context_sets_version";
  sql::Statement s_version(db.GetUniqueStatement(kVersionSql));
  EXPECT_TRUE(s_version.Step());
  EXPECT_EQ(browser_context_id, s_version.ColumnString(0));
  EXPECT_EQ(version.GetString(), s_version.ColumnString(1));

  EXPECT_FALSE(s_version.Step());

  // ============ Verify persisting context config
  const char kSelectConfigSql[] =
      "SELECT browser_context_id,site,primary_site FROM policy_configurations";
  sql::Statement s_config(db.GetUniqueStatement(kSelectConfigSql));
  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member1, s_config.ColumnString(1));
  EXPECT_EQ(primary_site, s_config.ColumnString(2));

  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member2, s_config.ColumnString(1));
  EXPECT_EQ("", s_config.ColumnString(2));

  EXPECT_FALSE(s_config.Step());

  // ============ Verify persisting manual config
  const char kSelectManualSql[] =
      "SELECT site,primary_site,site_type FROM manual_configurations";
  sql::Statement s_manual(db.GetUniqueStatement(kSelectManualSql));
  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_site, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(1, s_manual.ColumnInt(2));

  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_primary, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(0, s_manual.ColumnInt(2));

  EXPECT_FALSE(s_manual.Step());
}

// Verify public sets are not persisted with invalid version, and manual sets
// and context config are still persisted.
TEST_F(FirstPartySetsDatabaseTest, PersistSets_NoPreExistingDB_NoPublicSets) {
  const std::string browser_context_id = "b";
  const std::string site = "https://site.test";
  const std::string primary = "https://primary.test";

  const std::string manual_site = "https://aaa.test";
  const std::string manual_primary = "https://bbb.test";

  const std::string primary_site = "https://example.test";
  const std::string site_member1 = "https://member1.test";
  const std::string site_member2 = "https://member2.test";

  net::GlobalFirstPartySets global_sets(
      base::Version(),
      /*entries=*/
      {{net::SchemefulSite(GURL(site)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kAssociated, std::nullopt)},
       {net::SchemefulSite(GURL(primary)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kPrimary, std::nullopt)}},
      /*aliases=*/{});

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> manual_sets = {
      {net::SchemefulSite(GURL(manual_site)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kAssociated, std::nullopt)},
      {net::SchemefulSite(GURL(manual_primary)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kPrimary, std::nullopt)}};
  global_sets.ApplyManuallySpecifiedSet(
      net::LocalSetDeclaration(/*set_entries=*/manual_sets, /*aliases=*/{}));

  net::FirstPartySetsContextConfig config(
      {{net::SchemefulSite(GURL(site_member1)),
        net::FirstPartySetEntryOverride(
            net::FirstPartySetEntry(net::SchemefulSite(GURL(primary_site)),
                                    net::SiteType::kAssociated, std::nullopt))},
       {net::SchemefulSite(GURL(site_member2)),
        net::FirstPartySetEntryOverride()}});

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->PersistSets(browser_context_id, global_sets, config));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(0u, CountPublicSetsEntries(&db));
  EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));
  EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));

  // ============ Verify persisting context config
  const char kSelectConfigSql[] =
      "SELECT browser_context_id,site,primary_site FROM policy_configurations";
  sql::Statement s_config(db.GetUniqueStatement(kSelectConfigSql));
  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member1, s_config.ColumnString(1));
  EXPECT_EQ(primary_site, s_config.ColumnString(2));

  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member2, s_config.ColumnString(1));
  EXPECT_EQ("", s_config.ColumnString(2));

  EXPECT_FALSE(s_config.Step());

  // ============ Verify persisting manual configurations
  const char kSelectManualSql[] =
      "SELECT site,primary_site,site_type FROM manual_configurations";
  sql::Statement s_manual(db.GetUniqueStatement(kSelectManualSql));
  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_site, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(1, s_manual.ColumnInt(2));

  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_primary, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(0, s_manual.ColumnInt(2));

  EXPECT_FALSE(s_manual.Step());
}

TEST_F(FirstPartySetsDatabaseTest, PersistSets_PreExistingDB) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  const std::string browser_context_id = "b2";
  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    ASSERT_EQ(2u, CountPublicSetsEntries(&db));
    ASSERT_EQ(2u, CountPolicyConfigurationsEntries(&db));

    // Verify data in the public_sets table.
    static constexpr char kSelectPublicSetsSql[] =
        "SELECT version,site,primary_site,site_type FROM public_sets";
    sql::Statement s_public_sets(db.GetUniqueStatement(kSelectPublicSetsSql));
    ASSERT_TRUE(s_public_sets.Step());
    ASSERT_EQ("0.0.1", s_public_sets.ColumnString(0));
    ASSERT_EQ("https://aaa.test", s_public_sets.ColumnString(1));
    ASSERT_EQ("https://bbb.test", s_public_sets.ColumnString(2));
    ASSERT_EQ(1, s_public_sets.ColumnInt(3));

    ASSERT_TRUE(s_public_sets.Step());
    ASSERT_EQ("0.0.1", s_public_sets.ColumnString(0));
    ASSERT_EQ("https://bbb.test", s_public_sets.ColumnString(1));
    ASSERT_EQ("https://bbb.test", s_public_sets.ColumnString(2));
    ASSERT_EQ(0, s_public_sets.ColumnInt(3));

    // Verify data in the policy_configurations table.
    const char kSelectConfigSql[] =
        "SELECT browser_context_id,site,primary_site FROM "
        "policy_configurations";
    sql::Statement s_config(db.GetUniqueStatement(kSelectConfigSql));
    EXPECT_TRUE(s_config.Step());
    EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
    EXPECT_EQ("https://member1.test", s_config.ColumnString(1));
    EXPECT_EQ("https://example.test", s_config.ColumnString(2));

    EXPECT_TRUE(s_config.Step());
    EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
    EXPECT_EQ("https://member2.test", s_config.ColumnString(1));
    EXPECT_EQ("", s_config.ColumnString(2));

    EXPECT_FALSE(s_config.Step());

    // Verify data in the manual_configurations table
    static constexpr char kSelectManualSetsSql[] =
        "SELECT browser_context_id,site,primary_site,site_type FROM "
        "manual_configurations";
    sql::Statement s_manual(db.GetUniqueStatement(kSelectManualSetsSql));
    ASSERT_TRUE(s_manual.Step());
    ASSERT_EQ("b0", s_manual.ColumnString(0));
    ASSERT_EQ("https://ccc.test", s_manual.ColumnString(1));
    ASSERT_EQ("https://ddd.test", s_manual.ColumnString(2));
    ASSERT_EQ(1, s_manual.ColumnInt(3));

    ASSERT_TRUE(s_manual.Step());
    ASSERT_EQ("b0", s_manual.ColumnString(0));
    ASSERT_EQ("https://ddd.test", s_manual.ColumnString(1));
    ASSERT_EQ("https://ddd.test", s_manual.ColumnString(2));
    ASSERT_EQ(0, s_manual.ColumnInt(3));
  }
  const base::Version version("0.0.2");
  const std::string site = "https://site1.test";
  const std::string primary = "https://site2.test";

  const std::string manual_site = "https://manualsite1.test";
  const std::string manual_primary = "https://manualsite2.test";

  const std::string primary_site = "https://example2.test";
  const std::string site_member1 = "https://member3.test";
  const std::string site_member2 = "https://member4.test";

  net::GlobalFirstPartySets global_sets(
      version,
      /*entries=*/
      {{net::SchemefulSite(GURL(site)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kAssociated, std::nullopt)},
       {net::SchemefulSite(GURL(primary)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kPrimary, std::nullopt)}},
      /*aliases=*/{});

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> manual_sets = {
      {net::SchemefulSite(GURL(manual_site)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kAssociated, std::nullopt)},
      {net::SchemefulSite(GURL(manual_primary)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(manual_primary)),
                               net::SiteType::kPrimary, std::nullopt)}};
  global_sets.ApplyManuallySpecifiedSet(
      net::LocalSetDeclaration(/*set_entries=*/manual_sets, /*aliases=*/{}));

  net::FirstPartySetsContextConfig config(
      {{net::SchemefulSite(GURL(site_member1)),
        net::FirstPartySetEntryOverride(
            net::FirstPartySetEntry(net::SchemefulSite(GURL(primary_site)),
                                    net::SiteType::kAssociated, std::nullopt))},
       {net::SchemefulSite(GURL(site_member2)),
        net::FirstPartySetEntryOverride()}});

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->PersistSets(browser_context_id, global_sets, config));
  CloseDatabase();

  // Verify data is inserted.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(4u, CountPublicSetsEntries(&db));
  EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));

  // ============ Verify persisting public sets
  static constexpr char kSelectPublicSetsSql[] =
      "SELECT site,primary_site,site_type FROM public_sets "
      "WHERE version=?";
  sql::Statement s_public_sets(db.GetUniqueStatement(kSelectPublicSetsSql));
  s_public_sets.BindString(0, version.GetString());
  EXPECT_TRUE(s_public_sets.Step());
  EXPECT_EQ(site, s_public_sets.ColumnString(0));
  EXPECT_EQ(primary, s_public_sets.ColumnString(1));
  EXPECT_EQ(1, s_public_sets.ColumnInt(2));

  EXPECT_TRUE(s_public_sets.Step());
  EXPECT_EQ(primary, s_public_sets.ColumnString(0));
  EXPECT_EQ(primary, s_public_sets.ColumnString(1));
  EXPECT_EQ(0, s_public_sets.ColumnInt(2));

  EXPECT_FALSE(s_public_sets.Step());

  static constexpr char kVersionSql[] =
      "SELECT public_sets_version FROM browser_context_sets_version "
      "WHERE browser_context_id=?";
  sql::Statement s_version(db.GetUniqueStatement(kVersionSql));
  s_version.BindString(0, browser_context_id);
  EXPECT_TRUE(s_version.Step());
  EXPECT_EQ(version.GetString(), s_version.ColumnString(0));

  EXPECT_FALSE(s_version.Step());

  // ============ Verify the new context config overwrote the pre-existing
  // data.
  const char kSelectConfigSql[] =
      "SELECT browser_context_id,site,primary_site FROM policy_configurations "
      "WHERE browser_context_id=?";
  sql::Statement s_config(db.GetUniqueStatement(kSelectConfigSql));
  s_config.BindString(0, browser_context_id);
  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member1, s_config.ColumnString(1));
  EXPECT_EQ(primary_site, s_config.ColumnString(2));

  EXPECT_TRUE(s_config.Step());
  EXPECT_EQ(browser_context_id, s_config.ColumnString(0));
  EXPECT_EQ(site_member2, s_config.ColumnString(1));
  EXPECT_EQ("", s_config.ColumnString(2));
  EXPECT_FALSE(s_config.Step());

  // ============ Verify new manual config overwrote pre-existing data
  static constexpr char kSelectManualSetsSql[] =
      "SELECT site,primary_site,site_type FROM manual_configurations "
      "WHERE browser_context_id=?";
  sql::Statement s_manual(db.GetUniqueStatement(kSelectManualSetsSql));
  s_manual.BindString(0, browser_context_id);
  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_site, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(1, s_manual.ColumnInt(2));

  EXPECT_TRUE(s_manual.Step());
  EXPECT_EQ(manual_primary, s_manual.ColumnString(0));
  EXPECT_EQ(manual_primary, s_manual.ColumnString(1));
  EXPECT_EQ(0, s_manual.ColumnInt(2));

  EXPECT_FALSE(s_manual.Step());
}

TEST_F(FirstPartySetsDatabaseTest, PersistSets_PreExistingVersion) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  const base::Version version("0.0.1");
  const std::string aaa = "https://aaa.test";
  const std::string bbb = "https://bbb.test";
  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    ASSERT_EQ(2u, CountPublicSetsEntries(&db));

    static constexpr char kSelectSql[] =
        "SELECT 1 FROM public_sets WHERE version=?";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    s.BindString(0, version.GetString());
    ASSERT_TRUE(s.Step());
  }

  const std::string browser_context_id = "b";
  const std::string site = "https://site1.test";
  const std::string primary = "https://site2.test";

  net::GlobalFirstPartySets input(
      version,
      /*entries=*/
      {{net::SchemefulSite(GURL(site)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kAssociated, std::nullopt)},
       {net::SchemefulSite(GURL(primary)),
        net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                                net::SiteType::kPrimary, std::nullopt)}},
      /*aliases=*/{});

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->PersistSets(browser_context_id, input,
                                net::FirstPartySetsContextConfig()));
  CloseDatabase();

  // Verify data is not overwritten with the same version.
  sql::Database db;
  ASSERT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));

  static constexpr char kSelectSql[] =
      "SELECT version,site,primary_site,site_type FROM public_sets";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(version.GetString(), s.ColumnString(0));
  ASSERT_EQ(aaa, s.ColumnString(1));
  ASSERT_EQ(bbb, s.ColumnString(2));
  ASSERT_EQ(1, s.ColumnInt(3));

  ASSERT_TRUE(s.Step());
  ASSERT_EQ(version.GetString(), s.ColumnString(0));
  ASSERT_EQ(bbb, s.ColumnString(1));
  ASSERT_EQ(bbb, s.ColumnString(2));
  ASSERT_EQ(0, s.ColumnInt(3));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, InsertSitesToClear_NoPreExistingDB) {
  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };
  int64_t expected_run_count = 1;

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear("b", input));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));

  const char kSelectSql[] =
      "SELECT browser_context_id, site, marked_at_run FROM "
      "browser_context_sites_to_clear";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("b", s.ColumnString(0));
  EXPECT_EQ("https://example1.test", s.ColumnString(1));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("b", s.ColumnString(0));
  EXPECT_EQ("https://example2.test", s.ColumnString(1));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(2));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, InsertSitesToClear_PreExistingDB) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  const std::string browser_context_id = "b0";
  int64_t pre_run_count = 0;
  // Verify data in the pre-existing DB, and set `pre_run_count`.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));

    const char kSelectSql[] =
        "SELECT site, marked_at_run FROM browser_context_sites_to_clear "
        "WHERE browser_context_id=?";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    s.BindString(0, browser_context_id);
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("https://example.test", s.ColumnString(0));
    EXPECT_EQ(1, s.ColumnInt64(1));
    pre_run_count = s.ColumnInt64(1);
  }

  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear(browser_context_id, input));
  CloseDatabase();

  int64_t expected_run_count = 2;
  // Verify the inserted data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(4u, CountBrowserContextSitesToClearEntries(&db));

  const char kSelectSql[] =
      "SELECT site, marked_at_run FROM browser_context_sites_to_clear "
      "WHERE marked_at_run>?"
      "AND browser_context_id=?";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  s.BindInt64(0, pre_run_count);
  s.BindString(1, browser_context_id);

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(input.at(0).Serialize(), s.ColumnString(0));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(1));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(input.at(1).Serialize(), s.ColumnString(0));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(1));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest,
       InsertBrowserContextCleared_NoPreExistingDB) {
  const std::string browser_context_id = "b";
  int64_t expected_run_count = 1;

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertBrowserContextCleared(browser_context_id));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

  const char kSelectSql[] =
      "SELECT browser_context_id, cleared_at_run FROM browser_contexts_cleared";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  EXPECT_TRUE(s.Step());
  EXPECT_EQ(browser_context_id, s.ColumnString(0));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(1));
  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, InsertBrowserContextCleared_PreExistingDB) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  int64_t pre_run_count = 0;
  // Verify data in the pre-existing DB, and set `pre_run_count`.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id, cleared_at_run FROM "
        "browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("b0", s.ColumnString(0));
    EXPECT_EQ(1, s.ColumnInt64(1));
    pre_run_count = s.ColumnInt64(1);
  }

  std::string browser_context_id = "b";
  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertBrowserContextCleared(browser_context_id));
  CloseDatabase();

  // Verify the inserted data has the updated `cleared_at_run` value.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountBrowserContextsClearedEntries(&db));

  const char kSelectSql[] =
      "SELECT browser_context_id FROM browser_contexts_cleared "
      "WHERE cleared_at_run>?";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  s.BindInt64(0, pre_run_count);

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(browser_context_id, s.ColumnString(0));
  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, GetSitesToClearFilters_NoPreExistingDB) {
  OpenDatabase();
  std::optional<std::pair<std::vector<net::SchemefulSite>,
                          net::FirstPartySetsCacheFilter>>
      res = db()->GetSitesToClearFilters("b");
  EXPECT_TRUE(res.has_value());
  EXPECT_THAT(res->first, std::vector<net::SchemefulSite>());
  EXPECT_EQ(res->second, net::FirstPartySetsCacheFilter());
}

TEST_F(FirstPartySetsDatabaseTest, GetSitesToClearFilters) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  const std::string browser_context_id = "b0";
  const int64_t expected_run_count = 2;

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    const char kSelectSql[] =
        "SELECT site, marked_at_run FROM browser_context_sites_to_clear "
        "WHERE browser_context_id=?";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    s.BindString(0, browser_context_id);
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://example.test", s.ColumnString(0));
    ASSERT_EQ(1, s.ColumnInt64(1));
    ASSERT_FALSE(s.Step());
  }

  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example1(GURL("https://example1.test"));
  net::SchemefulSite example2(GURL("https://example2.test"));

  std::vector<net::SchemefulSite> input = {example1, example2};

  // Insert new sites to be cleared.
  OpenDatabase();
  EXPECT_TRUE(db()->InsertSitesToClear(browser_context_id, input));

  net::FirstPartySetsCacheFilter cache_filter(
      {{example, 1}, {example1, 2}, {example2, 2}}, expected_run_count);

  std::optional<std::pair<std::vector<net::SchemefulSite>,
                          net::FirstPartySetsCacheFilter>>
      res = db()->GetSitesToClearFilters(browser_context_id);
  EXPECT_TRUE(res.has_value());
  EXPECT_THAT(res->first, input);
  EXPECT_EQ(res->second, cache_filter);
}

TEST_F(FirstPartySetsDatabaseTest, GetSets_NoPreExistingDB) {
  OpenDatabase();
  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig("b");
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res->first.empty());
  EXPECT_TRUE(res->second.empty());
}

TEST_F(FirstPartySetsDatabaseTest, GetSets_NoPublicSets) {
  const std::string browser_context_id = "b";
  const net::SchemefulSite site(GURL("https://site.test"));
  const net::SchemefulSite primary(GURL("https://primary.test"));
  const net::SchemefulSite manual_site(GURL("https://aaa.test"));
  const net::SchemefulSite manual_primary(GURL("https://bbb.test"));

  net::GlobalFirstPartySets global_sets(
      base::Version(),
      /*entries=*/
      {{site, net::FirstPartySetEntry(primary, net::SiteType::kAssociated,
                                      std::nullopt)},
       {primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                         std::nullopt)}},
      /*aliases=*/{});

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> manual_sets = {
      {manual_site,
       net::FirstPartySetEntry(manual_primary, net::SiteType::kAssociated,
                               std::nullopt)},
      {manual_primary,
       net::FirstPartySetEntry(manual_primary, net::SiteType::kPrimary,
                               std::nullopt)}};
  global_sets.ApplyManuallySpecifiedSet(
      net::LocalSetDeclaration(/*set_entries=*/manual_sets, /*aliases=*/{}));

  OpenDatabase();
  // Trigger the lazy-initialization and insert data with a invalid version, so
  // that public sets will not be persisted.
  ASSERT_TRUE(db()->PersistSets(browser_context_id, global_sets,
                                net::FirstPartySetsContextConfig()));

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig(browser_context_id);

  EXPECT_TRUE(res.has_value());
  EXPECT_THAT(
      res->first.FindEntries({manual_site, manual_primary},
                             net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(manual_site,
               net::FirstPartySetEntry(
                   manual_primary, net::SiteType::kAssociated, std::nullopt)),
          Pair(manual_primary,
               net::FirstPartySetEntry(manual_primary, net::SiteType::kPrimary,
                                       std::nullopt))));
  EXPECT_TRUE(res->second.empty());
}

TEST_F(FirstPartySetsDatabaseTest, GetSets_PublicSetsHaveSingleton) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v5.public_sets_singleton.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(4u, CountPublicSetsEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextSetsVersionEntries(&db));
    EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  }
  const net::SchemefulSite aaa(GURL("https://aaa.test"));
  const net::SchemefulSite bbb(GURL("https://bbb.test"));
  const net::SchemefulSite ccc(GURL("https://ccc.test"));
  const net::SchemefulSite ddd(GURL("https://ddd.test"));
  OpenDatabase();

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig("b0");
  EXPECT_TRUE(res.has_value());
  // The singleton set should be deleted.
  EXPECT_THAT(res->first.FindEntries({aaa, bbb, ccc, ddd},
                                     net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(ccc, net::FirstPartySetEntry(
                                ddd, net::SiteType::kAssociated, std::nullopt)),
                  Pair(ddd, net::FirstPartySetEntry(
                                ddd, net::SiteType::kPrimary, std::nullopt))));
  EXPECT_EQ(res->second, net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsDatabaseTest, GetSets_PublicSetsHaveOrphan) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v5.public_sets_orphan.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(4u, CountPublicSetsEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextSetsVersionEntries(&db));
    EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  }
  const net::SchemefulSite aaa(GURL("https://aaa.test"));
  const net::SchemefulSite bbb(GURL("https://bbb.test"));
  const net::SchemefulSite ccc(GURL("https://ccc.test"));
  const net::SchemefulSite ddd(GURL("https://ddd.test"));
  OpenDatabase();

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig("b0");
  EXPECT_TRUE(res.has_value());
  // The singleton set should be deleted.
  EXPECT_THAT(res->first.FindEntries({aaa, bbb, ccc, ddd},
                                     net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(ccc, net::FirstPartySetEntry(
                                ddd, net::SiteType::kAssociated, std::nullopt)),
                  Pair(ddd, net::FirstPartySetEntry(
                                ddd, net::SiteType::kPrimary, std::nullopt))));
  EXPECT_EQ(res->second, net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsDatabaseTest, GetSets) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(2u, CountPublicSetsEntries(&db));
    EXPECT_EQ(3u, CountBrowserContextSetsVersionEntries(&db));
    EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));
  }
  const net::SchemefulSite aaa(GURL("https://aaa.test"));
  const net::SchemefulSite bbb(GURL("https://bbb.test"));
  const net::SchemefulSite ccc(GURL("https://ccc.test"));
  const net::SchemefulSite ddd(GURL("https://ddd.test"));
  OpenDatabase();

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig("b0");
  EXPECT_TRUE(res.has_value());
  EXPECT_THAT(res->first.FindEntries({aaa, bbb, ccc, ddd},
                                     net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(aaa, net::FirstPartySetEntry(
                                bbb, net::SiteType::kAssociated, std::nullopt)),
                  Pair(bbb, net::FirstPartySetEntry(
                                bbb, net::SiteType::kPrimary, std::nullopt)),
                  Pair(ccc, net::FirstPartySetEntry(
                                ddd, net::SiteType::kAssociated, std::nullopt)),
                  Pair(ddd, net::FirstPartySetEntry(
                                ddd, net::SiteType::kPrimary, std::nullopt))));
  EXPECT_EQ(res->second, net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsDatabaseTest,
       HasEntryInBrowserContextsClearedForTesting_NoPreExistingDB) {
  OpenDatabase();
  EXPECT_FALSE(db()->HasEntryInBrowserContextsClearedForTesting("b"));
}

TEST_F(FirstPartySetsDatabaseTest, HasEntryInBrowserContextsClearedForTesting) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(),
                                               GetCurrentVersionSqlFilePath()));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));
  }

  OpenDatabase();
  EXPECT_TRUE(db()->HasEntryInBrowserContextsClearedForTesting("b0"));
}

TEST_F(FirstPartySetsDatabaseTest, PersistSets_FormatCheck) {
  const base::Version version("0.0.1");
  const std::string browser_context_id = "b";
  const net::SchemefulSite primary(GURL("https://aaa.test"));
  const net::SchemefulSite associated_site(GURL("https://bbb.test"));
  const net::SchemefulSite service_site(GURL("https://ccc.test"));

  const net::SchemefulSite manual_primary(GURL("https://ddd.test"));
  const net::SchemefulSite manual_associated_site(GURL("https://eee.test"));
  const net::SchemefulSite manual_service_site(GURL("https://fff.test"));

  const net::SchemefulSite config_primary_site(GURL("https://example.test"));
  const net::SchemefulSite config_site_member1(GURL("https://member1.test"));
  const net::SchemefulSite config_site_member2(GURL("https://member2.test"));

  net::GlobalFirstPartySets global_sets(
      version,
      /*entries=*/
      {{associated_site,
        net::FirstPartySetEntry(primary, net::SiteType::kAssociated,
                                std::nullopt)},
       {service_site, net::FirstPartySetEntry(primary, net::SiteType::kService,
                                              std::nullopt)},
       {primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                         std::nullopt)}},
      /*aliases=*/{});
  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> manual_sets = {
      {manual_associated_site,
       net::FirstPartySetEntry(manual_primary, net::SiteType::kAssociated,
                               std::nullopt)},
      {manual_service_site,
       net::FirstPartySetEntry(manual_primary, net::SiteType::kService,
                               std::nullopt)},
      {manual_primary,
       net::FirstPartySetEntry(manual_primary, net::SiteType::kPrimary,
                               std::nullopt)}};
  global_sets.ApplyManuallySpecifiedSet(
      net::LocalSetDeclaration(/*set_entries=*/manual_sets, /*aliases=*/{}));

  net::FirstPartySetsContextConfig config(
      {{config_site_member1,
        net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
            config_primary_site, net::SiteType::kAssociated, std::nullopt))},
       {config_site_member2, net::FirstPartySetEntryOverride()}});

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->PersistSets(browser_context_id, global_sets, config));

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      res = db()->GetGlobalSetsAndConfig(browser_context_id);
  EXPECT_TRUE(res.has_value());
  EXPECT_EQ(res->first, global_sets);
  EXPECT_EQ(res->second, config);
}

class FirstPartySetsDatabaseMigrationsTest : public FirstPartySetsDatabaseTest {
 public:
  FirstPartySetsDatabaseMigrationsTest() = default;

  void MigrateDatabase() {
    FirstPartySetsDatabase db(db_path());
    // Trigger the lazy-initialization.
    std::ignore = db.GetGlobalSetsAndConfig("b");
  }
};

TEST_F(FirstPartySetsDatabaseMigrationsTest, MigrateEmptyToCurrent) {
  {
    FirstPartySetsDatabase db(db_path());
    // Trigger the lazy-initialization.
    std::ignore = db.GetGlobalSetsAndConfig("b");
  }

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("policy_configurations"));
    EXPECT_EQ(0u, CountPolicyConfigurationsEntries(&db));
  }
}

TEST_F(FirstPartySetsDatabaseMigrationsTest, MigrateVersion2ToCurrent) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v2.sql")));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    ASSERT_EQ(2, VersionFromMetaTable(db));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
    EXPECT_EQ(kCurrentVersionNumber, CompatibleVersionFromMetaTable(db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("policy_configurations"));
    EXPECT_FALSE(db.DoesTableExist("policy_modifications"));
    EXPECT_TRUE(db.DoesTableExist("manual_configurations"));
    EXPECT_FALSE(db.DoesTableExist("manual_sets"));

    // Verify that data is preserved across the migration.
    EXPECT_EQ(2u, CountPolicyConfigurationsEntries(&db));
    EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));
  }
}

TEST_F(FirstPartySetsDatabaseMigrationsTest, MigrateVersion3ToCurrent) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v3.sql")));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    ASSERT_EQ(3, VersionFromMetaTable(db));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
    EXPECT_EQ(kCurrentVersionNumber, CompatibleVersionFromMetaTable(db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("manual_configurations"));
    EXPECT_FALSE(db.DoesTableExist("manual_sets"));

    // Verify that data is preserved across the migration.
    EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));
  }
}

TEST_F(FirstPartySetsDatabaseMigrationsTest, MigrateVersion4ToCurrent) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v4.sql")));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    ASSERT_EQ(4, VersionFromMetaTable(db));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromMetaTable(db));
    EXPECT_EQ(kCurrentVersionNumber, CompatibleVersionFromMetaTable(db));

    // Verify that data is preserved across the migration.
    EXPECT_EQ(2u, CountManualConfigurationsEntries(&db));
  }
}
}  // namespace content
