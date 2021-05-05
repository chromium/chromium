// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/sql_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

class SQLTransactionTest : public sql::SQLTestBase {
 public:
  void SetUp() override {
    SQLTestBase::SetUp();

    ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  }

  // Returns the number of rows in table "foo".
  int CountFoo() {
    sql::Statement count(db().GetUniqueStatement("SELECT count(*) FROM foo"));
    count.Step();
    return count.ColumnInt(0);
  }
};

TEST_F(SQLTransactionTest, Commit) {
  {
    sql::Transaction t(&db());
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));

    t.Commit();
    EXPECT_FALSE(t.is_open());
  }

  EXPECT_EQ(1, CountFoo());
}

TEST_F(SQLTransactionTest, Rollback) {
  // Test some basic initialization, and that rollback runs when you exit the
  // scope.
  {
    sql::Transaction t(&db());
    EXPECT_FALSE(t.is_open());
    EXPECT_TRUE(t.Begin());
    EXPECT_TRUE(t.is_open());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  }

  // Nothing should have been committed since it was implicitly rolled back.
  EXPECT_EQ(0, CountFoo());

  // Test explicit rollback.
  sql::Transaction t2(&db());
  EXPECT_FALSE(t2.is_open());
  EXPECT_TRUE(t2.Begin());

  EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  t2.Rollback();
  EXPECT_FALSE(t2.is_open());

  // Nothing should have been committed since it was explicitly rolled back.
  EXPECT_EQ(0, CountFoo());
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  EXPECT_EQ(0, db().transaction_nesting());

  // Outermost transaction.
  {
    sql::Transaction outer(&db());
    EXPECT_TRUE(outer.Begin());
    EXPECT_EQ(1, db().transaction_nesting());

    // The first inner one gets committed.
    {
      sql::Transaction inner1(&db());
      EXPECT_TRUE(inner1.Begin());
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().transaction_nesting());

      inner1.Commit();
      EXPECT_EQ(1, db().transaction_nesting());
    }

    // One row should have gotten inserted.
    EXPECT_EQ(1, CountFoo());

    // The second inner one gets rolled back.
    {
      sql::Transaction inner2(&db());
      EXPECT_TRUE(inner2.Begin());
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().transaction_nesting());

      inner2.Rollback();
      EXPECT_EQ(1, db().transaction_nesting());
    }

    // A third inner one will fail in Begin since one has already been rolled
    // back.
    EXPECT_EQ(1, db().transaction_nesting());
    {
      sql::Transaction inner3(&db());
      EXPECT_FALSE(inner3.Begin());
      EXPECT_EQ(1, db().transaction_nesting());
    }
  }
  EXPECT_EQ(0, db().transaction_nesting());
  EXPECT_EQ(0, CountFoo());
}
