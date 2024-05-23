// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class SQLTransactionTest : public testing::Test {
 public:
  ~SQLTransactionTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("transaction_test.sqlite")));

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
  Database db_;
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

TEST(SQLTransactionDatabaseDestroyedTest, BeginIsNoOp) {
  auto db = std::make_unique<Database>();
  ASSERT_TRUE(db->OpenInMemory());
  Transaction transaction(db.get());
  db.reset();
  ASSERT_FALSE(transaction.Begin());
}

TEST(SQLTransactionDatabaseDestroyedTest, RollbackIsNoOp) {
  auto db = std::make_unique<Database>();
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
  auto db = std::make_unique<Database>();
  ASSERT_TRUE(db->OpenInMemory());
  Transaction transaction(db.get());
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db->HasActiveTransactions());
  db.reset();
  ASSERT_FALSE(transaction.Commit());
}

}  // namespace

}  // namespace sql
