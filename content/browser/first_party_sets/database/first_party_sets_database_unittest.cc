// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/database/first_party_sets_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;

namespace content {

namespace {

static const size_t kTableCount = 5u;

int VersionFromMetaTable(sql::Database& db) {
  // Get version.
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step())
    return 0;
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

  static base::FilePath GetSqlFilePath(base::StringPiece sql_file_name) {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("content/test/data/first_party_sets/");
    path = path.AppendASCII(sql_file_name);
    EXPECT_TRUE(base::PathExists(path));
    return path;
  }

  size_t CountPublicSetsEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "public_sets", &size));
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

  size_t CountPolicyModificationsEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "policy_modifications", &size));
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
  // [public_sets], [policy_modifications], [browser_context_sites_to_clear],
  // [browser_contexts_cleared], and [meta].
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  // [idx_marked_at_run_sites], [idx_cleared_at_run_browser_contexts], and
  // [sqlite_autoindex_meta_1].
  EXPECT_EQ(3u, sql::test::CountSQLIndices(&db));
  // `site`, `primary`, `site_type`.
  EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "public_sets"));
  // `browser_context_id`, `site`, `marked_at_run`.
  EXPECT_EQ(
      3u, sql::test::CountTableColumns(&db, "browser_context_sites_to_clear"));
  // `browser_context_id`, `cleared_at_run`.
  EXPECT_EQ(2u, sql::test::CountTableColumns(&db, "browser_contexts_cleared"));
  // `browser_context_id`, `site`, `site_owner`.
  EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "policy_modifications"));
  EXPECT_EQ(0u, CountPublicSetsEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(0u, CountPolicyModificationsEntries(&db));
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_CurrentVersion_Success) {
  base::HistogramTester histograms;
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear("b", {}));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_TooOld_Fail) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v0.init_too_old.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_FALSE(db()->InsertSitesToClear("b", {}));
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(0, VersionFromMetaTable(db));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kTooOld, 1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_TooNew_Fail) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v1.init_too_new.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_FALSE(db()->InsertSitesToClear("b", {}));
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(2, VersionFromMetaTable(db));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));
  EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kTooNew, 1);
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

TEST_F(FirstPartySetsDatabaseTest, SetPublicSets_NoPreExistingDB) {
  const std::string site = "https://aaa.test";
  const std::string primary = "https://bbb.test";

  FirstPartySetsDatabase::FlattenedSets input = {
      {net::SchemefulSite(GURL(site)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                               net::SiteType::kAssociated, absl::nullopt)},
      {net::SchemefulSite(GURL(primary)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                               net::SiteType::kPrimary, absl::nullopt)}};

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->SetPublicSets(input));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));

  static constexpr char kSelectSql[] =
      "SELECT site,primary_site,site_type FROM public_sets";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  EXPECT_TRUE(s.Step());
  EXPECT_EQ(site, s.ColumnString(0));
  EXPECT_EQ(primary, s.ColumnString(1));
  EXPECT_EQ(1, s.ColumnInt(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(primary, s.ColumnString(0));
  EXPECT_EQ(primary, s.ColumnString(1));
  EXPECT_EQ(0, s.ColumnInt(2));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, SetPublicSets_PreExistingDB) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    ASSERT_EQ(2u, CountPublicSetsEntries(&db));

    static constexpr char kSelectSql[] =
        "SELECT site,primary_site,site_type FROM public_sets";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://aaa.test", s.ColumnString(0));
    ASSERT_EQ("https://bbb.test", s.ColumnString(1));
    ASSERT_EQ(1, s.ColumnInt(2));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://bbb.test", s.ColumnString(0));
    ASSERT_EQ("https://bbb.test", s.ColumnString(1));
    ASSERT_EQ(0, s.ColumnInt(2));
  }

  const std::string site = "https://site1.test";
  const std::string primary = "https://site2.test";

  FirstPartySetsDatabase::FlattenedSets input = {
      {net::SchemefulSite(GURL(site)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                               net::SiteType::kAssociated, absl::nullopt)},
      {net::SchemefulSite(GURL(primary)),
       net::FirstPartySetEntry(net::SchemefulSite(GURL(primary)),
                               net::SiteType::kPrimary, absl::nullopt)}};

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->SetPublicSets(input));
  CloseDatabase();

  // Verify the inserted data overwrote the pre-existing data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountPublicSetsEntries(&db));

  static constexpr char kSelectSql[] =
      "SELECT site,primary_site,site_type FROM public_sets";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  EXPECT_TRUE(s.Step());
  EXPECT_EQ(site, s.ColumnString(0));
  EXPECT_EQ(primary, s.ColumnString(1));
  EXPECT_EQ(1, s.ColumnInt(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(primary, s.ColumnString(0));
  EXPECT_EQ(primary, s.ColumnString(1));
  EXPECT_EQ(0, s.ColumnInt(2));

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
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

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
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

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

TEST_F(FirstPartySetsDatabaseTest, InsertPolicymodifications_NoPreExistingDB) {
  const std::string browser_context_id = "b";
  const std::string site_owner = "https://example.test";
  const std::string site_member1 = "https://member1.test";
  const std::string site_member2 = "https://member2.test";

  base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>
      input = {
          {net::SchemefulSite(GURL(site_member1)),
           net::FirstPartySetEntry(net::SchemefulSite(GURL(site_owner)),
                                   net::SiteType::kAssociated, absl::nullopt)},
          {net::SchemefulSite(GURL(site_member2)), absl::nullopt}};

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertPolicyModifications(browser_context_id, input));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));

  const char kSelectSql[] =
      "SELECT browser_context_id,site,site_owner FROM policy_modifications";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  EXPECT_TRUE(s.Step());
  EXPECT_EQ(browser_context_id, s.ColumnString(0));
  EXPECT_EQ(site_member1, s.ColumnString(1));
  EXPECT_EQ(site_owner, s.ColumnString(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ(browser_context_id, s.ColumnString(0));
  EXPECT_EQ(site_member2, s.ColumnString(1));
  EXPECT_EQ("", s.ColumnString(2));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, InsertPolicymodifications_PreExistingDB) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  const std::string browser_context_id = "b2";
  // Verify data in the pre-existing DB, and set `pre_run_count`.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    ASSERT_EQ(2u, CountPolicyModificationsEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id,site,site_owner FROM policy_modifications "
        "WHERE browser_context_id=?";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    s.BindString(0, browser_context_id);
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("b2", s.ColumnString(0));
    ASSERT_EQ("https://member1.test", s.ColumnString(1));
    ASSERT_EQ("https://example.test", s.ColumnString(2));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("b2", s.ColumnString(0));
    ASSERT_EQ("https://member2.test", s.ColumnString(1));
    ASSERT_EQ("", s.ColumnString(2));
    ASSERT_FALSE(s.Step());
  }

  const std::string site_owner = "https://example2.test";
  const std::string site_member1 = "https://member3.test";
  const std::string site_member2 = "https://member4.test";

  base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>
      input = {
          {net::SchemefulSite(GURL(site_member1)),
           net::FirstPartySetEntry(net::SchemefulSite(GURL(site_owner)),
                                   net::SiteType::kAssociated, absl::nullopt)},
          {net::SchemefulSite(GURL(site_member2)), absl::nullopt}};

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertPolicyModifications(browser_context_id, input));
  CloseDatabase();

  // Verify the inserted data overwrote the pre-existing data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
  EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));

  const char kSelectSql[] =
      "SELECT browser_context_id,site,site_owner FROM policy_modifications "
      "WHERE browser_context_id=?";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  s.BindString(0, browser_context_id);
  EXPECT_TRUE(s.Step());
  EXPECT_EQ("b2", s.ColumnString(0));
  EXPECT_EQ(site_member1, s.ColumnString(1));
  EXPECT_EQ(site_owner, s.ColumnString(2));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("b2", s.ColumnString(0));
  EXPECT_EQ(site_member2, s.ColumnString(1));
  EXPECT_EQ("", s.ColumnString(2));
  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear_NoPreExistingDB) {
  OpenDatabase();
  EXPECT_EQ(std::vector<net::SchemefulSite>(), db()->FetchSitesToClear("b"));
}

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear_BrowserContextNotExist) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  std::string browser_context_id = "b";
  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    // b hasn't been cleared before.
    const char kSelectSql[] =
        "SELECT browser_context_id FROM browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("b0", s.ColumnString(0));
    EXPECT_FALSE(s.Step());
  }

  OpenDatabase();
  EXPECT_EQ(std::vector<net::SchemefulSite>(),
            db()->FetchSitesToClear(browser_context_id));
}

// b1 has sites to clear but hasn't been cleared before.
TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear_BrowserContextNotCleared) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  const std::string browser_context_id = "b1";
  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT 1 FROM browser_contexts_cleared "
        "WHERE browser_context_id=?";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    s.BindString(0, browser_context_id);
    EXPECT_FALSE(s.Step());
  }

  OpenDatabase();
  EXPECT_EQ(std::vector<net::SchemefulSite>(
                {net::SchemefulSite(GURL("https://example.test"))}),
            db()->FetchSitesToClear(browser_context_id));
}

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  const std::string browser_context_id = "b0";
  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(2u, CountBrowserContextSitesToClearEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id FROM browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ(browser_context_id, s.ColumnString(0));
    EXPECT_FALSE(s.Step());
  }
  // Insert new sites to be cleared.
  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };

  OpenDatabase();
  EXPECT_TRUE(db()->InsertSitesToClear(browser_context_id, input));
  EXPECT_EQ(input, db()->FetchSitesToClear(browser_context_id));
}

TEST_F(FirstPartySetsDatabaseTest, FetchAllSitesToClearFilter) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  const std::string browser_context_id = "b0";
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

  // Insert new sites to be cleared.
  OpenDatabase();
  EXPECT_TRUE(db()->InsertSitesToClear(
      browser_context_id, {
                              net::SchemefulSite(GURL("https://example1.test")),
                              net::SchemefulSite(GURL("https://example2.test")),
                          }));

  base::flat_map<net::SchemefulSite, int64_t> result = {
      {net::SchemefulSite(GURL("https://example.test")), 1},
      {net::SchemefulSite(GURL("https://example1.test")), 2},
      {net::SchemefulSite(GURL("https://example2.test")), 2}};

  EXPECT_THAT(db()->FetchAllSitesToClearFilter(browser_context_id), result);
}

TEST_F(FirstPartySetsDatabaseTest, FetchPolicyModifications_NoPreExistingDB) {
  OpenDatabase();
  EXPECT_THAT(db()->FetchPolicyModifications("b"), IsEmpty());
}

TEST_F(FirstPartySetsDatabaseTest, FetchPolicyModifications) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(kTableCount, sql::test::CountSQLTables(&db));
    EXPECT_EQ(2u, CountPolicyModificationsEntries(&db));
  }
  base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>
      res = {
          {net::SchemefulSite(GURL("https://member1.test")),
           net::FirstPartySetEntry(
               {net::SchemefulSite(GURL("https://example.test"))},
               net::SiteType::kAssociated, absl::nullopt)},
          {net::SchemefulSite(GURL("https://member2.test")), absl::nullopt},
      };
  OpenDatabase();
  EXPECT_THAT(db()->FetchPolicyModifications("b2"), res);
}

TEST_F(FirstPartySetsDatabaseTest, GetPublicSets_NoPreExistingDB) {
  OpenDatabase();
  EXPECT_THAT(db()->GetPublicSets(), IsEmpty());
}

TEST_F(FirstPartySetsDatabaseTest, GetPublicSets) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(2u, CountPublicSetsEntries(&db));
  }
  FirstPartySetsDatabase::FlattenedSets res = {
      {net::SchemefulSite(GURL("https://aaa.test")),
       net::FirstPartySetEntry({net::SchemefulSite(GURL("https://bbb.test"))},
                               net::SiteType::kAssociated, absl::nullopt)},
      {net::SchemefulSite(GURL("https://bbb.test")),
       net::FirstPartySetEntry({net::SchemefulSite(GURL("https://bbb.test"))},
                               net::SiteType::kPrimary, absl::nullopt)},
  };
  OpenDatabase();
  EXPECT_THAT(db()->GetPublicSets(), res);
}

}  // namespace content