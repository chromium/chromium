// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

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
    Transaction t(&db_);
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));

    t.Commit();
    EXPECT_FALSE(t.is_open());
  }

  EXPECT_EQ(1, CountFoo());
}

TEST_F(SQLTransactionTest, Rollback) {
  // Test some basic initialization, and that rollback runs when you exit the
  // scope.
  {
    Transaction t(&db_);
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  }

  // Nothing should have been committed since it was implicitly rolled back.
  EXPECT_EQ(0, CountFoo());

  // Test explicit rollback.
  Transaction t2(&db_);
  EXPECT_FALSE(t2.is_open());
  EXPECT_TRUE(t2.Begin());

  EXPECT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  t2.Rollback();
  EXPECT_FALSE(t2.is_open());

  // Nothing should have been committed since it was explicitly rolled back.
  EXPECT_EQ(0, CountFoo());
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  EXPECT_EQ(0, db_.transaction_nesting());

  // Outermost transaction.
  {
    Transaction outer(&db_);
    EXPECT_TRUE(outer.Begin());
    EXPECT_EQ(1, db_.transaction_nesting());

    // The first inner one gets committed.
    {
      Transaction inner1(&db_);
      EXPECT_TRUE(inner1.Begin());
      EXPECT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db_.transaction_nesting());

      inner1.Commit();
      EXPECT_EQ(1, db_.transaction_nesting());
    }

    // One row should have gotten inserted.
    EXPECT_EQ(1, CountFoo());

    // The second inner one gets rolled back.
    {
      Transaction inner2(&db_);
      EXPECT_TRUE(inner2.Begin());
      EXPECT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db_.transaction_nesting());

      inner2.Rollback();
      EXPECT_EQ(1, db_.transaction_nesting());
    }

    // A third inner one will fail in Begin since one has already been rolled
    // back.
    EXPECT_EQ(1, db_.transaction_nesting());
    {
      Transaction inner3(&db_);
      EXPECT_FALSE(inner3.Begin());
      EXPECT_EQ(1, db_.transaction_nesting());
    }
  }
  EXPECT_EQ(0, db_.transaction_nesting());
  EXPECT_EQ(0, CountFoo());
}

}  // namespace

}  // namespace sql
