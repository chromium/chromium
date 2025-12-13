// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class SQLTransactionTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("transaction_test.sqlite");
    ASSERT_TRUE(db_.Open(db_path_));

    ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a, b)"));
  }

  // Returns the number of rows in table "foo".
  int CountFoo() {
    Statement count(db_.GetUniqueStatement("SELECT count(*) FROM foo"));
    count.Step();
    return count.ColumnInt(0);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_{sql::DatabaseOptions().set_exclusive_locking(false),
               test::kTestTag};
  base::FilePath db_path_;
};

TEST_F(SQLTransactionTest, Commit) {
  {
    Transaction transaction(&db_);
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo()) << "INSERT did not work as intended";

    transaction.Commit();
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());
  }

  EXPECT_FALSE(db_.HasActiveTransactions());
  EXPECT_EQ(1, CountFoo()) << "Transaction changes not committed";
}

// Regression test for <https://crbug.com/326498384>.
TEST_F(SQLTransactionTest, CloseDatabase) {
  EXPECT_FALSE(db_.HasActiveTransactions());

  {
    Transaction transaction(&db_);
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    db_.Close();
    EXPECT_FALSE(db_.HasActiveTransactions());
  }

  EXPECT_FALSE(db_.HasActiveTransactions());
}

TEST_F(SQLTransactionTest, RollbackOnDestruction) {
  EXPECT_FALSE(db_.HasActiveTransactions());

  {
    Transaction transaction(&db_);
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo()) << "INSERT did not work as intended";
  }

  EXPECT_FALSE(db_.HasActiveTransactions());
  EXPECT_EQ(0, CountFoo()) << "Transaction changes not rolled back";
}

TEST_F(SQLTransactionTest, ExplicitRollback) {
  EXPECT_FALSE(db_.HasActiveTransactions());

  {
    Transaction transaction(&db_);
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());

    ASSERT_TRUE(transaction.Begin());
    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_TRUE(transaction.IsActiveForTesting());

    ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
    ASSERT_EQ(1, CountFoo()) << "INSERT did not work as intended";

    transaction.Rollback();
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_FALSE(transaction.IsActiveForTesting());
    EXPECT_EQ(0, CountFoo()) << "Transaction changes not rolled back";
  }

  EXPECT_FALSE(db_.HasActiveTransactions());
  EXPECT_EQ(0, CountFoo()) << "Transaction changes not rolled back";
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  EXPECT_FALSE(db_.HasActiveTransactions());
  EXPECT_EQ(0, db_.transaction_nesting());

  // Outermost transaction.
  {
    Transaction outer_txn(&db_);
    EXPECT_FALSE(db_.HasActiveTransactions());
    EXPECT_EQ(0, db_.transaction_nesting());

    ASSERT_TRUE(outer_txn.Begin());
    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_EQ(1, db_.transaction_nesting());

    // First inner transaction is committed.
    {
      Transaction committed_inner_txn(&db_);
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());

      ASSERT_TRUE(committed_inner_txn.Begin());
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(2, db_.transaction_nesting());

      ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      ASSERT_EQ(1, CountFoo()) << "INSERT did not work as intended";

      committed_inner_txn.Commit();
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());
    }

    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_EQ(1, db_.transaction_nesting());
    EXPECT_EQ(1, CountFoo()) << "First inner transaction did not commit";

    // Second inner transaction is rolled back.
    {
      Transaction rolled_back_inner_txn(&db_);
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());

      ASSERT_TRUE(rolled_back_inner_txn.Begin());
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(2, db_.transaction_nesting());

      ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (2, 3)"));
      ASSERT_EQ(2, CountFoo()) << "INSERT did not work as intended";

      rolled_back_inner_txn.Rollback();
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());
      EXPECT_EQ(2, CountFoo())
          << "Nested transaction rollback deferred to top-level transaction";
    }

    EXPECT_TRUE(db_.HasActiveTransactions());
    EXPECT_EQ(1, db_.transaction_nesting());
    EXPECT_EQ(2, CountFoo())
        << "Nested transaction rollback deferred to top-level transaction";

    // Third inner transaction fails in Begin(), because a nested transaction
    // has already been rolled back.
    {
      Transaction failed_inner_txn(&db_);
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());

      EXPECT_FALSE(failed_inner_txn.Begin());
      EXPECT_TRUE(db_.HasActiveTransactions());
      EXPECT_EQ(1, db_.transaction_nesting());
    }
  }

  EXPECT_FALSE(db_.HasActiveTransactions());
  EXPECT_EQ(0, db_.transaction_nesting());
  EXPECT_EQ(0, CountFoo());
}

TEST_F(SQLTransactionTest, TransactionCommitWithPendingWriter) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE rows (id)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows (id) VALUES (12)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());

  // The'RETURNING' clause changes the behavior of the statement to return a
  // row. A pending write statement is kept alive in the sqlite connection.
  Statement update(
      db_.GetUniqueStatement("UPDATE rows SET id = 2 * id RETURNING id"));
  EXPECT_TRUE(update.Step());

  // The commit will fail due to the pending writer.
  EXPECT_FALSE(transaction.Commit());

  EXPECT_FALSE(update.Step());
  EXPECT_TRUE(update.Succeeded());
}

TEST_F(SQLTransactionTest, TransactionCommitWithActiveReader) {
  Database other_db(sql::DatabaseOptions().set_exclusive_locking(false),
                    test::kTestTag);
  ASSERT_TRUE(other_db.Open(db_path_));

  ASSERT_TRUE(db_.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(2)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(3)"));

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
  Database other_db(sql::DatabaseOptions().set_exclusive_locking(false),
                    test::kTestTag);
  ASSERT_TRUE(other_db.Open(db_path_));

  ASSERT_TRUE(db_.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(2)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());

  Transaction other_transaction(&other_db);
  EXPECT_TRUE(other_transaction.Begin());
  Statement select(other_db.GetUniqueStatement("SELECT * FROM rows"));
  EXPECT_TRUE(select.Step());

  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(3)"));

  // The commit will fail with a SQL_BUSY error code since there is an
  // open statement. When this error code is detected, a explicit rollback is
  // issued by the Database code to close the pending transaction.
  EXPECT_FALSE(transaction.Commit());

  // Statement is expected to work.
  EXPECT_TRUE(select.Step());
  ASSERT_TRUE(other_transaction.Commit());
}

TEST_F(SQLTransactionTest, TransactionOnRazedDB) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(2)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());

  Statement select(db_.GetUniqueStatement("SELECT * FROM rows"));
  EXPECT_TRUE(select.Step());

  // Raze won't succeed if there is a pending transaction. The pending commit
  // will succeed to apply the modifications.
  EXPECT_FALSE(db_.Raze());
  EXPECT_TRUE(transaction.Commit());
}

TEST_F(SQLTransactionTest, TransactionOnPoisonedDB) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(1)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());
  db_.Poison();
  EXPECT_FALSE(transaction.Commit());
}

TEST_F(SQLTransactionTest, TransactionOnClosedDB) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(id) VALUES(1)"));

  Transaction transaction(&db_);
  EXPECT_TRUE(transaction.Begin());
  db_.Close();
  EXPECT_FALSE(transaction.Commit());
}

TEST(SQLTransactionDatabaseDestroyedTest, BeginIsNoOp) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->OpenInMemory());
  Transaction transaction(db.get());
  db.reset();
  ASSERT_FALSE(transaction.Begin());
}

TEST(SQLTransactionDatabaseDestroyedTest, RollbackIsNoOp) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->OpenInMemory());
  Transaction transaction(db.get());
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db->HasActiveTransactions());
  db.reset();
  // `Transaction::Rollback()` does not return a value, so we cannot verify
  // externally whether it returned early.
  transaction.Rollback();
}

TEST(SQLTransactionDatabaseDestroyedTest, CommitIsNoOp) {
  auto db = std::make_unique<Database>(test::kTestTag);
  ASSERT_TRUE(db->OpenInMemory());
  Transaction transaction(db.get());
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db->HasActiveTransactions());
  db.reset();
  ASSERT_FALSE(transaction.Commit());
}

}  // namespace

}  // namespace sql
