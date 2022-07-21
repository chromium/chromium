// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

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

  size_t CountSitesToClearEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(sql::test::CountTableRows(db, "sites_to_clear", &size));
    return size;
  }

  size_t CountBrowserContextsClearedEntries(sql::Database* db) {
    size_t size = 0;
    EXPECT_TRUE(
        sql::test::CountTableRows(db, "browser_contexts_cleared", &size));
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
  EXPECT_TRUE(db()->InsertSitesToClear({}));
  EXPECT_TRUE(base::PathExists(db_path()));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kSuccess,
                                1);
  CloseDatabase();

  // Create a db handle to the existing db file to verify schemas.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  // [sites_to_clear], [browser_contexts_cleared], and [meta].
  EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  // [idx_marked_at_run_sites], [idx_cleared_at_run_browser_contexts], and
  // [sqlite_autoindex_meta_1].
  EXPECT_EQ(3u, sql::test::CountSQLIndices(&db));
  // `site`, `marked_at_run`.
  EXPECT_EQ(2u, sql::test::CountTableColumns(&db, "sites_to_clear"));
  // `browser_context_id`, `cleared_at_run`.
  EXPECT_EQ(2u, sql::test::CountTableColumns(&db, "browser_contexts_cleared"));
  EXPECT_EQ(0u, CountSitesToClearEntries(&db));
  EXPECT_EQ(0u, CountBrowserContextsClearedEntries(&db));
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_CurrentVersion_Success) {
  base::HistogramTester histograms;
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear({}));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

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
  EXPECT_FALSE(db()->InsertSitesToClear({}));
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(0, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kTooOld, 1);
}

TEST_F(FirstPartySetsDatabaseTest, LoadDBFile_TooNew_Fail) {
  base::HistogramTester histograms;
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      db_path(), GetSqlFilePath("v1.init_too_new.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_FALSE(db()->InsertSitesToClear({}));
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(2, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountSitesToClearEntries(&db));
  EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

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
  EXPECT_FALSE(db()->InsertSitesToClear({}));
  CloseDatabase();

  // The original database was destroyed.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(0u, sql::test::CountSQLTables(&db));
  histograms.ExpectUniqueSample("FirstPartySets.Database.InitStatus",
                                FirstPartySetsDatabase::InitStatus::kCorrupted,
                                1);
}

TEST_F(FirstPartySetsDatabaseTest, InsertSitesToClear_NoPreExistingDB) {
  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };
  int64_t expected_run_count = 1;

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear(input));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(2u, CountSitesToClearEntries(&db));

  const char kSelectSql[] = "SELECT site, marked_at_run FROM sites_to_clear";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("https://example1.test", s.ColumnString(0));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(1));

  EXPECT_TRUE(s.Step());
  EXPECT_EQ("https://example2.test", s.ColumnString(0));
  EXPECT_EQ(expected_run_count, s.ColumnInt64(1));

  EXPECT_FALSE(s.Step());
}

TEST_F(FirstPartySetsDatabaseTest, InsertSitesToClear_PreExistingDB) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  int64_t pre_run_count = 0;
  // Verify data in the pre-existing DB, and set `pre_run_count`.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountSitesToClearEntries(&db));

    const char kSelectSql[] = "SELECT site, marked_at_run FROM sites_to_clear";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("https://example.test", s.ColumnString(0));
    EXPECT_EQ(1, s.ColumnInt64(1));
    pre_run_count = s.ColumnInt64(1);
  }

  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };
  int64_t expected_run_count = 2;

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertSitesToClear(input));
  CloseDatabase();

  // Verify the inserted data.
  sql::Database db;
  EXPECT_TRUE(db.Open(db_path()));
  EXPECT_EQ(3u, CountSitesToClearEntries(&db));

  const char kSelectSql[] =
      "SELECT site, marked_at_run FROM sites_to_clear "
      "WHERE marked_at_run>?";
  sql::Statement s(db.GetUniqueStatement(kSelectSql));
  s.BindInt64(0, pre_run_count);

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
  const std::string browser_context_id = "p";
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
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id, cleared_at_run FROM "
        "browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("p", s.ColumnString(0));
    EXPECT_EQ(1, s.ColumnInt64(1));
    pre_run_count = s.ColumnInt64(1);
  }

  std::string browser_context_id = "p1";

  OpenDatabase();
  // Trigger the lazy-initialization.
  EXPECT_TRUE(db()->InsertBrowserContextCleared(browser_context_id));
  CloseDatabase();

  // Verify the inserted data.
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

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear_NoPreExistingDB) {
  OpenDatabase();
  EXPECT_EQ(std::vector<net::SchemefulSite>(), db()->FetchSitesToClear("id"));
}

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear_BrowserContextNotExist) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountSitesToClearEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id FROM browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("p", s.ColumnString(0));
    EXPECT_FALSE(s.Step());
  }

  OpenDatabase();
  EXPECT_EQ(std::vector<net::SchemefulSite>(), db()->FetchSitesToClear("p1"));
}

TEST_F(FirstPartySetsDatabaseTest, FetchSitesToClear) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(db_path(), GetSqlFilePath("v1.sql")));

  // Verify data in the pre-existing DB.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_path()));
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));
    EXPECT_EQ(1u, CountSitesToClearEntries(&db));
    EXPECT_EQ(1u, CountBrowserContextsClearedEntries(&db));

    const char kSelectSql[] =
        "SELECT browser_context_id FROM browser_contexts_cleared";
    sql::Statement s(db.GetUniqueStatement(kSelectSql));
    EXPECT_TRUE(s.Step());
    EXPECT_EQ("p", s.ColumnString(0));
    EXPECT_FALSE(s.Step());
  }
  // Insert new sites to be cleared.
  std::vector<net::SchemefulSite> input = {
      net::SchemefulSite(GURL("https://example1.test")),
      net::SchemefulSite(GURL("https://example2.test")),
  };

  OpenDatabase();
  EXPECT_TRUE(db()->InsertSitesToClear(input));
  EXPECT_EQ(input, db()->FetchSitesToClear("p"));
}

}  // namespace content