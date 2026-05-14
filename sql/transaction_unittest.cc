// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "sql/database.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/drive_error_test_vfs.h"
#include "sql/test/matchers.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

using ::sql::test::PrimaryErrorIs;
using ::testing::Bool;
using ::testing::Contains;
using ::testing::TestParamInfo;
using ::testing::WithParamInterface;

class SQLTransactionTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("transaction_test.sqlite");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

using SQLTransactionDeathTest = SQLTransactionTest;

class SQLTransactionJournalTypeTest : public SQLTransactionTest,
                                      public WithParamInterface<bool> {
 protected:
  static bool WalModeEnabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SQLTransactionJournalTypeTest,
    Bool(),
    [](const TestParamInfo<SQLTransactionJournalTypeTest::ParamType>& info) {
      return info.param ? "WALEnabled" : "WALDisabled";
    });

// Returns the number of rows in table "foo".
int CountFoo(Database& db) {
  Statement count(db.GetUniqueStatement("SELECT count(*) FROM foo"));
  count.Step();
  return count.ColumnInt(0);
}

TEST_F(SQLTransactionTest, Commit) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (a, b)"));

  {
    Transaction transaction(&db);
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo(db)) << "INSERT did not work as intended";

    transaction.Commit();
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());
  }

  EXPECT_FALSE(db.HasActiveTransactions());
  EXPECT_EQ(1, CountFoo(db)) << "Transaction changes not committed";
}

// Regression test for <https://crbug.com/326498384>.
TEST_F(SQLTransactionTest, CloseDatabase) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (a, b)"));

  EXPECT_FALSE(db.HasActiveTransactions());

  {
    Transaction transaction(&db);
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    db.Close();
    EXPECT_FALSE(db.HasActiveTransactions());
  }

  EXPECT_FALSE(db.HasActiveTransactions());
}

TEST_F(SQLTransactionTest, RollbackOnDestruction) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (a, b)"));

  EXPECT_FALSE(db.HasActiveTransactions());

  {
    Transaction transaction(&db);
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo(db)) << "INSERT did not work as intended";
  }

  EXPECT_FALSE(db.HasActiveTransactions());
  EXPECT_EQ(0, CountFoo(db)) << "Transaction changes not rolled back";
}

TEST_F(SQLTransactionTest, ExplicitRollback) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (a, b)"));

  EXPECT_FALSE(db.HasActiveTransactions());

  {
    Transaction transaction(&db);
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo(db)) << "INSERT did not work as intended";

    transaction.Rollback();
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());
    EXPECT_EQ(0, CountFoo(db)) << "Transaction changes not rolled back";
  }

  EXPECT_FALSE(db.HasActiveTransactions());
  EXPECT_EQ(0, CountFoo(db)) << "Transaction changes not rolled back";
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE foo (a, b)"));

  EXPECT_FALSE(db.HasActiveTransactions());
  EXPECT_EQ(0, db.transaction_nesting());

  // Outermost transaction.
  {
    Transaction outer_txn(&db);
    EXPECT_FALSE(db.HasActiveTransactions());
    EXPECT_EQ(0, db.transaction_nesting());

    ASSERT_TRUE(outer_txn.Begin());
    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_EQ(1, db.transaction_nesting());

    // First inner transaction is committed.
    {
      Transaction committed_inner_txn(&db);
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());

      ASSERT_TRUE(committed_inner_txn.Begin());
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(2, db.transaction_nesting());

      ASSERT_TRUE(db.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      ASSERT_EQ(1, CountFoo(db)) << "INSERT did not work as intended";

      committed_inner_txn.Commit();
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());
    }

    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_EQ(1, db.transaction_nesting());
    EXPECT_EQ(1, CountFoo(db)) << "First inner transaction did not commit";

    // Second inner transaction is rolled back.
    {
      Transaction rolled_back_inner_txn(&db);
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());

      ASSERT_TRUE(rolled_back_inner_txn.Begin());
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(2, db.transaction_nesting());

      ASSERT_TRUE(db.Execute("INSERT INTO foo (a, b) VALUES (2, 3)"));
      ASSERT_EQ(2, CountFoo(db)) << "INSERT did not work as intended";

      rolled_back_inner_txn.Rollback();
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());
      EXPECT_EQ(2, CountFoo(db))
          << "Nested transaction rollback deferred to top-level transaction";
    }

    EXPECT_TRUE(db.HasActiveTransactions());
    EXPECT_EQ(1, db.transaction_nesting());
    EXPECT_EQ(2, CountFoo(db))
        << "Nested transaction rollback deferred to top-level transaction";

    // Third inner transaction fails in Begin(), because a nested transaction
    // has already been rolled back.
    {
      Transaction failed_inner_txn(&db);
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());

      EXPECT_FALSE(failed_inner_txn.Begin());
      EXPECT_TRUE(db.HasActiveTransactions());
      EXPECT_EQ(1, db.transaction_nesting());
    }
  }

  EXPECT_FALSE(db.HasActiveTransactions());
  EXPECT_EQ(0, db.transaction_nesting());
  EXPECT_EQ(0, CountFoo(db));
}

TEST_F(SQLTransactionTest, TransactionCommitWithPendingWriter) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows (id)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows (id) VALUES (12)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());

  // The'RETURNING' clause changes the behavior of the statement to return a
  // row. A pending write statement is kept alive in the sqlite connection.
  Statement update(
      db.GetUniqueStatement("UPDATE rows SET id = 2 * id RETURNING id"));
  EXPECT_TRUE(update.Step());

  // The commit will fail due to the pending writer.
  EXPECT_FALSE(transaction.Commit());

  EXPECT_FALSE(update.Step());
  EXPECT_TRUE(update.Succeeded());
}

TEST_F(SQLTransactionTest, TransactionCommitWithActiveReader) {
  Database db(sql::DatabaseOptions().set_exclusive_locking(false),
              test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));

  Database other_db(sql::DatabaseOptions().set_exclusive_locking(false),
                    test::kTestTag);
  ASSERT_TRUE(other_db.Open(db_path_));

  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(3)"));

  Statement select(other_db.GetUniqueStatement("SELECT * FROM rows"));
  EXPECT_TRUE(select.Step());

  // The commit will fail with a SQL_BUSY error code since there is an
  // open statement. When this error code is detected, a explicit rollback is
  // issued by the Database code to close the pending transaction.
  EXPECT_FALSE(transaction.Commit());

  // The open statements on a different connections remain valid since the
  // modifications were reverted. Statement is expected to work.
  EXPECT_TRUE(select.Step());
}

TEST_F(SQLTransactionTest, TransactionCommitWithActiveTransaction) {
  Database db(sql::DatabaseOptions().set_exclusive_locking(false),
              test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));

  Database other_db(sql::DatabaseOptions().set_exclusive_locking(false),
                    test::kTestTag);
  ASSERT_TRUE(other_db.Open(db_path_));

  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());

  Transaction other_transaction(&other_db);
  EXPECT_TRUE(other_transaction.Begin());
  Statement select(other_db.GetUniqueStatement("SELECT * FROM rows"));
  EXPECT_TRUE(select.Step());

  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(3)"));

  // The commit will fail with a SQL_BUSY error code since there is an
  // open statement. When this error code is detected, a explicit rollback is
  // issued by the Database code to close the pending transaction.
  EXPECT_FALSE(transaction.Commit());

  // Statement is expected to work.
  EXPECT_TRUE(select.Step());
  ASSERT_TRUE(other_transaction.Commit());
}

TEST_F(SQLTransactionTest, CreateTransactionOnRazedDB) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));

  EXPECT_TRUE(db.Raze());

  // Transactions can be created and used normally after `Raze()`.
  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));
  EXPECT_TRUE(transaction.Commit());

  Statement select(db.GetUniqueStatement("SELECT id FROM rows"));
  EXPECT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnInt(0), 2);
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLTransactionTest, BeginOnRazedDB) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));

  Transaction transaction(&db);
  EXPECT_TRUE(db.Raze());

  // Transactions still work normally after `Raze()`.
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));
  EXPECT_TRUE(transaction.Commit());

  Statement select(db.GetUniqueStatement("SELECT id FROM rows"));
  EXPECT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnInt(0), 2);
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLTransactionTest, RollbackOnRazedDB) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));

  // Raze won't succeed if there is a pending transaction. The pending commit
  // will succeed to apply the modifications.
  EXPECT_FALSE(db.Raze());
  transaction.Rollback();

  Statement select(db.GetUniqueStatement("SELECT id FROM rows"));
  EXPECT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnInt(0), 1);
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLTransactionTest, CommitOnRazedDB) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(2)"));

  // Raze won't succeed if there is a pending transaction. The pending commit
  // will succeed to apply the modifications.
  EXPECT_FALSE(db.Raze());
  EXPECT_TRUE(transaction.Commit());

  Statement select(db.GetUniqueStatement("SELECT id FROM rows ORDER BY id"));
  EXPECT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnInt(0), 1);
  EXPECT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnInt(0), 2);
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLTransactionTest, CreateTransactionOnPoisonedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  db.Poison();
  Transaction transaction(&db);
  EXPECT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, BeginOnPoisonedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  db.Poison();
  EXPECT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, RollbackOnPoisonedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  db.Poison();
  transaction.Rollback();
}

TEST_F(SQLTransactionTest, CommitOnPoisonedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  db.Poison();
  EXPECT_FALSE(transaction.Commit());
}

TEST_F(SQLTransactionTest, CreateTransactionOnClosedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  db.Close();
  Transaction transaction(&db);
  EXPECT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, BeginOnClosedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  db.Close();
  EXPECT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, RollbackOnClosedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  db.Close();
  transaction.Rollback();
}

TEST_F(SQLTransactionTest, CommitOnClosedDb) {
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));
  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Transaction transaction(&db);
  EXPECT_TRUE(transaction.Begin());
  db.Close();
  EXPECT_FALSE(transaction.Commit());
}

TEST_F(SQLTransactionTest, BeginOnDestroyedDb) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->Open(db_path_));
  Transaction transaction(db.get());
  db.reset();
  ASSERT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, RollbackOnDestroyedDb) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->Open(db_path_));
  Transaction transaction(db.get());
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db->HasActiveTransactions());
  db.reset();
  // `Transaction::Rollback()` does not return a value, so we cannot verify
  // externally whether it returned early.
  transaction.Rollback();
}

TEST_F(SQLTransactionTest, CommitOnDestroyedDb) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->Open(db_path_));
  Transaction transaction(db.get());
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db->HasActiveTransactions());
  db.reset();
  ASSERT_FALSE(transaction.Commit());
}

TEST_F(SQLTransactionDeathTest, CreateTransactionFromNullptr) {
  EXPECT_CHECK_DEATH(Transaction transaction(nullptr));
}

TEST_F(SQLTransactionTest, BeginBeforeDbOpen) {
  Database db(test::kTestTag);
  Transaction transaction(&db);
  EXPECT_FALSE(transaction.Begin());
}

TEST_F(SQLTransactionTest, CloseDbInTransactionCommitErrorCallback) {
  test::DriveErrorTestVfs test_vfs;
  Database db(test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));

  db.set_error_callback(
      base::BindLambdaForTesting([&](int error, sql::Statement* statement) {
        EXPECT_EQ(ToSqliteResultCode(error), SqliteResultCode::kFullDisk);
        db.RazeAndPoison();
      }));

  ASSERT_TRUE(db.Execute("CREATE TABLE rows(id)"));
  Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("INSERT INTO rows(id) VALUES(1)"));
  test_vfs.set_drive_full(true);
  EXPECT_FALSE(transaction.Commit());
  EXPECT_THAT(test_vfs.errors_produced(), Contains(SqliteErrorCode::kFullDisk));
  ASSERT_FALSE(db.is_open());
}

int64_t GetJournalSize(const base::FilePath& db_path, bool is_wal_mode) {
  base::FilePath journal_path = is_wal_mode
                                    ? Database::WriteAheadLogPath(db_path)
                                    : Database::JournalPath(db_path);
  return base::GetFileSize(journal_path).value_or(0);
}

TEST_P(SQLTransactionJournalTypeTest, TransactionRollbackDiskUnusable) {
  test::DriveErrorTestVfs test_vfs;
  Database db(
      DatabaseOptions().set_cache_size(1).set_wal_mode(WalModeEnabled()),
      test::kTestTag);
  ASSERT_TRUE(db.Open(db_path_));

  db.set_error_callback(base::BindLambdaForTesting(
      [&](int sqlite_error, sql::Statement* statement) {
        FAIL() << "Error callback should not have been called";
      }));

  ASSERT_TRUE(db.Execute("CREATE TABLE rows(key, value)"));
  ASSERT_TRUE(db.Execute("INSERT INTO rows(key, value) VALUES(1, 'original')"));

  Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db.Execute("UPDATE rows SET value = 'modified' WHERE key = 1"));

  // Write more rows until the memory cache is full and SQLite is forced to
  // write to the journal file.
  const int64_t start_journal_size = GetJournalSize(db_path_, WalModeEnabled());
  while (GetJournalSize(db_path_, WalModeEnabled()) == start_journal_size) {
    ASSERT_TRUE(db.Execute("INSERT INTO rows(key, value) VALUES(42, 'foo')"));
  }

  // Make the drive unusable, causing file operations to fail with SQLITE_IOERR.
  // If WAL mode is disabled, the rollback will fail because the journal file
  // cannot be read to restore the main database file. If WAL mode is enabled,
  // SQLite just rewind the WAL file leaving the rolled-back data there. No file
  // IO usually happens. But since lots of data was written in the transaction,
  // the database page 1 had to be modified to keep track of the new pages
  // created. The rollback therefore needs to restore page 1 to its original
  // state. It does so by reading it from the main database file. If this fails
  // on an SQLITE_IOERR, the rollback aborts.
  test_vfs.set_drive_unusable(true);
  transaction.Rollback();
  test_vfs.set_drive_unusable(false);
  EXPECT_THAT(test_vfs.errors_produced(),
              Contains(PrimaryErrorIs(SqliteErrorCode::kIo)));

  // The rollback failed, leaving behind a hot journal. But the database is
  // still valid. The next time the database is used, the hot journal will be
  // used to restore the database.
  Statement select(
      db.GetUniqueStatement("SELECT value FROM rows WHERE key = 1"));
  ASSERT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnString(0), "original");
}

}  // namespace

}  // namespace sql
