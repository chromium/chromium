// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/webdata/common/web_database_table.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/initialization.h"
#include "sql/meta_table.h"
#include "sql/test/drive_error_test_vfs.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Functor for closing an `sqlite3` database.
struct Sqlite3Close {
  void operator()(sqlite3* ptr) const { sqlite3_close(ptr); }
};

class WebDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

int GetVersionNumber(const base::FilePath& db_path) {
  sql::Database raw_db(sql::test::kTestTag);
  CHECK(raw_db.Open(db_path));
  sql::InitializedMetaTable meta_table(raw_db);
  return meta_table.GetVersionNumber();
}

void SetVersionNumber(const base::FilePath& db_path,
                      int version,
                      int compatible_version) {
  sql::Database raw_db(sql::test::kTestTag);
  CHECK(raw_db.Open(db_path));
  sql::InitializedMetaTable meta_table(raw_db);
  CHECK(meta_table.SetVersionNumber(version));
  CHECK(meta_table.SetCompatibleVersionNumber(compatible_version));
}

TEST_F(WebDatabaseTest, InitSuccess) {
  base::HistogramTester histogram_tester;
  WebDatabase db;
  EXPECT_EQ(db.Init(db_path_), sql::INIT_OK);
  histogram_tester.ExpectUniqueSample("WebDatabase.InitResult",
                                      WebDatabase::InitResult::kSuccess, 1);
}

TEST_F(WebDatabaseTest, InitPreservesCompatibleFutureVersion) {
  // Create a database with a future compatible version number.
  {
    WebDatabase db;
    ASSERT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  SetVersionNumber(db_path_, /*version=*/WebDatabase::kCurrentVersionNumber + 1,
                   /*compatible_version=*/WebDatabase::kCurrentVersionNumber);

  // Init() should preserve the database.
  ASSERT_EQ(GetVersionNumber(db_path_), WebDatabase::kCurrentVersionNumber + 1);
  {
    WebDatabase db;
    EXPECT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  EXPECT_EQ(GetVersionNumber(db_path_), WebDatabase::kCurrentVersionNumber + 1);
}

TEST_F(WebDatabaseTest, InitRazesIncompatibleOldVersion) {
  // Create a database with a deprecated version number.
  {
    WebDatabase db;
    ASSERT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  SetVersionNumber(
      db_path_, /*version=*/WebDatabase::kDeprecatedVersionNumber,
      /*compatible_version=*/WebDatabase::kDeprecatedVersionNumber);

  // Init() should raze the incompatible old database and initialize a new one
  // successfully.
  ASSERT_EQ(GetVersionNumber(db_path_), WebDatabase::kDeprecatedVersionNumber);
  {
    WebDatabase db;
    EXPECT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  EXPECT_EQ(GetVersionNumber(db_path_), WebDatabase::kCurrentVersionNumber);
}

TEST_F(WebDatabaseTest, InitRazesIncompatibleFutureVersion) {
  // Create a database with a compatible version number newer than
  // kCurrentVersionNumber.
  {
    WebDatabase db;
    ASSERT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  SetVersionNumber(
      db_path_, /*version=*/WebDatabase::kCurrentVersionNumber + 1,
      /*compatible_version=*/WebDatabase::kCurrentVersionNumber + 1);

  // Init() should raze the incompatible future database and initialize a new
  // one successfully.
  ASSERT_EQ(GetVersionNumber(db_path_), WebDatabase::kCurrentVersionNumber + 1);
  {
    WebDatabase db;
    EXPECT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  ASSERT_EQ(GetVersionNumber(db_path_), WebDatabase::kCurrentVersionNumber);
}

TEST_F(WebDatabaseTest, InitFailureCouldNotOpenIfWriteLockHeld) {
  // Create a raw sqlite3 database connection holding a writer lock.
  sql::EnsureSqliteInitialized();
  std::unique_ptr<sqlite3, Sqlite3Close> raw_sqlite_db;
  ASSERT_EQ(sqlite3_open(db_path_.AsUTF8Unsafe().c_str(),
                         std::out_ptr(raw_sqlite_db)),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(raw_sqlite_db.get(), "BEGIN EXCLUSIVE;", nullptr,
                         nullptr, nullptr),
            SQLITE_OK);

  // Other database connection will fail to acquire the lock.
  base::HistogramTester histogram_tester;
  WebDatabase db;
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult", WebDatabase::InitResult::kCouldNotOpen, 1);
}

TEST_F(WebDatabaseTest, InitFailureCommitFailsIfReadLockHeld) {
  // Create a raw sqlite3 database connection holding a read lock.
  sql::EnsureSqliteInitialized();
  std::unique_ptr<sqlite3, Sqlite3Close> raw_sqlite_db;
  ASSERT_EQ(sqlite3_open(db_path_.AsUTF8Unsafe().c_str(),
                         std::out_ptr(raw_sqlite_db)),
            SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(raw_sqlite_db.get(), "BEGIN;", nullptr, nullptr, nullptr),
      SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(raw_sqlite_db.get(), "SELECT COUNT(*) FROM sqlite_schema;",
                   nullptr, nullptr, nullptr),
      SQLITE_OK);

  // Other database connection will fail to acquire the lock.
  base::HistogramTester histogram_tester;
  WebDatabase db;
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult",
      WebDatabase::InitResult::kFailedToCommitInitTransaction, 1);
}

TEST_F(WebDatabaseTest, InitSharesReadLockIfDbExists) {
  // Create a table on disk.
  {
    WebDatabase db;
    ASSERT_EQ(db.Init(db_path_), sql::INIT_OK);
  }

  // Create a raw sqlite3 database connection holding a read lock.
  std::unique_ptr<sqlite3, Sqlite3Close> raw_sqlite_db;
  ASSERT_EQ(sqlite3_open(db_path_.AsUTF8Unsafe().c_str(),
                         std::out_ptr(raw_sqlite_db)),
            SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(raw_sqlite_db.get(), "SELECT COUNT(*) FROM sqlite_schema;",
                   nullptr, nullptr, nullptr),
      SQLITE_OK);

  // `WebDatabase` connection will share the read lock.
  base::HistogramTester histogram_tester;
  WebDatabase db;
  EXPECT_EQ(db.Init(db_path_), sql::INIT_OK);

  // Writing fails because write lock can't be acquired.
  EXPECT_FALSE(db.GetSQLConnection()->Execute("BEGIN EXCLUSIVE"));

  // `WebDatabase` can grab the write lock if it becomes available.
  raw_sqlite_db.reset();
  EXPECT_TRUE(db.GetSQLConnection()->Execute("BEGIN EXCLUSIVE"));
}

TEST_F(WebDatabaseTest, InitFailureDatabaseLockedIfDriveIsFull) {
  sql::test::DriveErrorTestVfs vfs;
  base::HistogramTester histogram_tester;
  WebDatabase db;

  // When the drive is full, the `sql::Database` can still be opened, but
  // nothing can be written.
  vfs.set_drive_full(true);
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  vfs.set_drive_full(false);

  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult", WebDatabase::InitResult::kMetaTableInitFailed,
      1);
}

TEST_F(WebDatabaseTest, InitFailureCouldNotRazeIncompatibleVersion) {
  sql::test::DriveErrorTestVfs vfs;

  // Create a database with a deprecated version number.
  {
    WebDatabase db;
    ASSERT_EQ(db.Init(db_path_), sql::INIT_OK);
  }
  SetVersionNumber(
      db_path_, /*version=*/WebDatabase::kDeprecatedVersionNumber,
      /*compatible_version=*/WebDatabase::kDeprecatedVersionNumber);

  base::HistogramTester histogram_tester;
  WebDatabase db;

  // Force disk writes to fail so RazeIfIncompatible cannot raze the file.
  vfs.set_drive_full(true);
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  vfs.set_drive_full(false);

  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult",
      WebDatabase::InitResult::kCouldNotRazeIncompatibleVersion, 1);
}

TEST_F(WebDatabaseTest, InitFailureMetaTableInitFailed) {
  // Create an incompatible 'meta' index, causing the 'meta' table creation to
  // fail.
  {
    sql::Database raw_db(sql::test::kTestTag);
    ASSERT_TRUE(raw_db.Open(db_path_));
    ASSERT_TRUE(raw_db.Execute("CREATE TABLE foo (id INTEGER)"));
    ASSERT_TRUE(raw_db.Execute("CREATE INDEX meta ON foo(id)"));
  }

  base::HistogramTester histogram_tester;
  WebDatabase db;
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult", WebDatabase::InitResult::kMetaTableInitFailed,
      1);
}

TEST_F(WebDatabaseTest, InitFailureMigrationError) {
  base::HistogramTester histogram_tester;

  // Create a database at an older version that requires migration.
  {
    sql::Database raw_db(sql::test::kTestTag);
    ASSERT_TRUE(raw_db.Open(db_path_));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&raw_db,
                                WebDatabase::kDeprecatedVersionNumber + 1,
                                WebDatabase::kDeprecatedVersionNumber + 1));
  }

  sql::test::DriveErrorTestVfs vfs;
  WebDatabase db;

  // Force disk writes to fail so migration fails when updating meta table.
  vfs.set_drive_full(true);
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  vfs.set_drive_full(false);

  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult", WebDatabase::InitResult::kMigrationError, 1);
}

// A fake `WebDatabaseTable` that always fails creating tables.
class CreateTableFailedTable : public WebDatabaseTable {
 public:
  WebDatabaseTable::TypeKey GetTypeKey() const override {
    static int table_key = 0;
    return reinterpret_cast<void*>(&table_key);
  }
  bool CreateTablesIfNecessary() override { return false; }
  bool MigrateToVersion(int version, bool* update_compatible_version) override {
    return true;
  }
};

TEST_F(WebDatabaseTest, InitFailureFailedToCreateTable) {
  base::HistogramTester histogram_tester;
  CreateTableFailedTable table;
  WebDatabase db;
  db.AddTable(&table);
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult", WebDatabase::InitResult::kFailedToCreateTable,
      1);
}

// A fake `WebDatabaseTable` that causes the `sql::Transaction` in
// `WebDatabase::Init()` to fail.
class CommitFailedTable : public WebDatabaseTable {
 public:
  explicit CommitFailedTable(sql::test::DriveErrorTestVfs* vfs) : vfs_(vfs) {}
  WebDatabaseTable::TypeKey GetTypeKey() const override {
    static int table_key = 0;
    return reinterpret_cast<void*>(&table_key);
  }
  bool CreateTablesIfNecessary() override {
    vfs_->set_drive_unusable(true);
    return true;
  }
  bool MigrateToVersion(int version, bool* update_compatible_version) override {
    return true;
  }

 private:
  raw_ptr<sql::test::DriveErrorTestVfs> vfs_;
};

TEST_F(WebDatabaseTest, InitFailureFailedToCommitInitTransaction) {
  base::HistogramTester histogram_tester;
  sql::test::DriveErrorTestVfs vfs;
  CommitFailedTable table(&vfs);
  WebDatabase db;
  db.AddTable(&table);
  EXPECT_EQ(db.Init(db_path_), sql::INIT_FAILURE);
  vfs.set_drive_unusable(false);
  histogram_tester.ExpectUniqueSample(
      "WebDatabase.InitResult",
      WebDatabase::InitResult::kFailedToCommitInitTransaction, 1);
}

}  // namespace
