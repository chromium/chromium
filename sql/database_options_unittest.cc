// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

enum class OpenVariant {
  kInMemory = 1,
  kOnDiskExclusiveJournal = 2,
  kOnDiskNonExclusiveJournal = 3,
  kOnDiskExclusiveWal = 4,
};

// We use the parameter to run all tests with WAL mode on and off.
class DatabaseOptionsTest : public testing::TestWithParam<OpenVariant> {
 public:
  DatabaseOptionsTest() = default;
  ~DatabaseOptionsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("database_test.sqlite");
  }

  OpenVariant open_variant() const { return GetParam(); }

  // The options below interact with all other options. These tests ensure that
  // all combinations work.
  bool exclusive_locking() const {
    return GetParam() != OpenVariant::kOnDiskNonExclusiveJournal;
  }
  bool wal_mode() const {
    return GetParam() == OpenVariant::kOnDiskExclusiveWal;
  }

  void OpenDatabase(Database& db) {
    switch (open_variant()) {
      case OpenVariant::kOnDiskExclusiveJournal:
        ASSERT_TRUE(db.Open(db_path_));
        break;

      case OpenVariant::kOnDiskNonExclusiveJournal:
        ASSERT_TRUE(db.Open(db_path_));
        break;

      case OpenVariant::kOnDiskExclusiveWal:
        ASSERT_TRUE(db.Open(db_path_));
        break;

      case OpenVariant::kInMemory:
        ASSERT_TRUE(db.OpenInMemory());
        break;
    }
  }

  // Runs a rolled back transaction, followed by a committed transaction.
  void RunTransactions(Database& db) {
    {
      Transaction rolled_back(&db);
      ASSERT_TRUE(rolled_back.Begin());
      ASSERT_TRUE(db.Execute("CREATE TABLE rows(id PRIMARY KEY NOT NULL)"));
      rolled_back.Rollback();
    }
    {
      Transaction committed(&db);
      ASSERT_TRUE(committed.Begin());
      ASSERT_TRUE(db.Execute("CREATE TABLE rows(id PRIMARY KEY NOT NULL)"));
      ASSERT_TRUE(committed.Commit());
    }
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_P(DatabaseOptionsTest, FlushToDisk_FalseByDefault) {
  DatabaseOptions options = {
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
  };
  EXPECT_FALSE(options.flush_to_media) << "Invalid test assumption";

  Database db(options);
  OpenDatabase(db);

  EXPECT_EQ("0", sql::test::ExecuteWithResult(&db, "PRAGMA fullfsync"));
}

TEST_P(DatabaseOptionsTest, FlushToDisk_True) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .flush_to_media = true,
  });
  OpenDatabase(db);

  EXPECT_EQ("1", sql::test::ExecuteWithResult(&db, "PRAGMA fullfsync"));
}

TEST_P(DatabaseOptionsTest, FlushToDisk_False_DoesNotCrash) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .flush_to_media = false,
  });
  OpenDatabase(db);

  EXPECT_EQ("0", sql::test::ExecuteWithResult(&db, "PRAGMA fullfsync"))
      << "Invalid test setup";
  RunTransactions(db);
}

TEST_P(DatabaseOptionsTest, FlushToDisk_True_DoesNotCrash) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .flush_to_media = true,
  });
  OpenDatabase(db);

  EXPECT_EQ("1", sql::test::ExecuteWithResult(&db, "PRAGMA fullfsync"))
      << "Invalid test setup";
  RunTransactions(db);
}

TEST_P(DatabaseOptionsTest, PageSize_Default) {
  static_assert(DatabaseOptions::kDefaultPageSize == 4096,
                "The page size numbers in this test file need to change");
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .page_size = 4096,
  });

  OpenDatabase(db);
  EXPECT_EQ("4096", sql::test::ExecuteWithResult(&db, "PRAGMA page_size"));

  RunTransactions(db);
  if (open_variant() != OpenVariant::kInMemory) {
    db.Close();
    EXPECT_EQ(4096, sql::test::ReadDatabasePageSize(db_path_).value_or(-1));
  }
}

TEST_P(DatabaseOptionsTest, PageSize_Large) {
  static_assert(DatabaseOptions::kDefaultPageSize < 16384,
                "The page size numbers in this test file need to change");
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .page_size = 16384,
  });

  OpenDatabase(db);
  EXPECT_EQ("16384", sql::test::ExecuteWithResult(&db, "PRAGMA page_size"));

  RunTransactions(db);
  if (open_variant() != OpenVariant::kInMemory) {
    db.Close();
    EXPECT_EQ(16384, sql::test::ReadDatabasePageSize(db_path_).value_or(-1));
  }
}

TEST_P(DatabaseOptionsTest, PageSize_Small) {
  static_assert(DatabaseOptions::kDefaultPageSize > 1024,
                "The page size numbers in this test file need to change");
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .page_size = 1024,
  });

  OpenDatabase(db);
  EXPECT_EQ("1024", sql::test::ExecuteWithResult(&db, "PRAGMA page_size"));

  RunTransactions(db);
  if (open_variant() != OpenVariant::kInMemory) {
    db.Close();
    EXPECT_EQ(1024, sql::test::ReadDatabasePageSize(db_path_).value_or(-1));
  }
}

TEST_P(DatabaseOptionsTest, CacheSize_Legacy) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .cache_size = 0,
  });
  OpenDatabase(db);

  EXPECT_EQ("-2000", sql::test::ExecuteWithResult(&db, "PRAGMA cache_size"));
}

TEST_P(DatabaseOptionsTest, CacheSize_Small) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .cache_size = 16,
  });
  OpenDatabase(db);
  EXPECT_EQ("16", sql::test::ExecuteWithResult(&db, "PRAGMA cache_size"));
}

TEST_P(DatabaseOptionsTest, CacheSize_Large) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .cache_size = 1000,
  });
  OpenDatabase(db);
  EXPECT_EQ("1000", sql::test::ExecuteWithResult(&db, "PRAGMA cache_size"));
}

TEST_P(DatabaseOptionsTest, EnableViewsDiscouraged_FalseByDefault) {
  DatabaseOptions options = {
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
  };
  EXPECT_FALSE(options.enable_views_discouraged) << "Invalid test assumption";

  Database db(options);
  OpenDatabase(db);

  // sqlite3_db_config() currently only disables querying views. Schema
  // operations on views are still allowed.
  ASSERT_TRUE(db.Execute("CREATE VIEW view(id) AS SELECT 1"));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_ERROR);
    Statement select_from_view(db.GetUniqueStatement("SELECT id FROM view"));
    EXPECT_FALSE(select_from_view.is_valid());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // sqlite3_db_config() currently only disables querying views. Schema
  // operations on views are still allowed.
  EXPECT_TRUE(db.Execute("DROP VIEW IF EXISTS view"));
}

TEST_P(DatabaseOptionsTest, EnableViewsDiscouraged_True) {
  Database db(DatabaseOptions{
      .exclusive_locking = exclusive_locking(),
      .wal_mode = wal_mode(),
      .enable_views_discouraged = true,
  });
  OpenDatabase(db);

  ASSERT_TRUE(db.Execute("CREATE VIEW view(id) AS SELECT 1"));

  Statement select_from_view(db.GetUniqueStatement("SELECT id FROM view"));
  ASSERT_TRUE(select_from_view.is_valid());
  EXPECT_TRUE(select_from_view.Step());
  EXPECT_EQ(1, select_from_view.ColumnInt64(0));

  EXPECT_TRUE(db.Execute("DROP VIEW IF EXISTS view"));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DatabaseOptionsTest,
    testing::Values(OpenVariant::kInMemory,
                    OpenVariant::kOnDiskExclusiveJournal,
                    OpenVariant::kOnDiskNonExclusiveJournal,
                    OpenVariant::kOnDiskExclusiveWal));

}  // namespace

}  // namespace sql
