// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace {

class StatementTest : public testing::Test {
 public:
  ~StatementTest() override = default;

  void SetUp() override {
    db_.set_histogram_tag("Test");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("statement_test.sqlite")));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_;
};

TEST_F(StatementTest, Assign) {
  Statement create;
  EXPECT_FALSE(create.is_valid());

  create.Assign(db_.GetUniqueStatement(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));
  EXPECT_TRUE(create.is_valid());
}

TEST_F(StatementTest, Run) {
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(a, b) VALUES(3, 12)"));

  Statement select(db_.GetUniqueStatement("SELECT b FROM rows WHERE a=?"));
  EXPECT_FALSE(select.Succeeded());

  // Stepping it won't work since we haven't bound the value.
  EXPECT_FALSE(select.Step());

  // Run should fail since this produces output, and we should use Step(). This
  // gets a bit wonky since sqlite says this is OK so succeeded is set.
  select.Reset(/*clear_bound_vars=*/true);
  select.BindInt64(0, 3);
  EXPECT_FALSE(select.Run());
  EXPECT_EQ(SQLITE_ROW, db_.GetErrorCode());
  EXPECT_TRUE(select.Succeeded());

  // Resetting it should put it back to the previous state (not runnable).
  select.Reset(/*clear_bound_vars=*/true);
  EXPECT_FALSE(select.Succeeded());

  // Binding and stepping should produce one row.
  select.BindInt64(0, 3);
  EXPECT_TRUE(select.Step());
  EXPECT_TRUE(select.Succeeded());
  EXPECT_EQ(12, select.ColumnInt64(0));
  EXPECT_FALSE(select.Step());
  EXPECT_TRUE(select.Succeeded());
}

// Error callback called for error running a statement.
TEST_F(StatementTest, DatabaseErrorCallbackCalledOnError) {
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));

  bool error_callback_called = false;
  int error = SQLITE_OK;
  db_.set_error_callback(base::BindLambdaForTesting(
      [&](int sqlite_error, sql::Statement* statement) {
        error_callback_called = true;
        error = sqlite_error;
      }));

  // `rows` is a table with ROWID. https://www.sqlite.org/rowidtable.html
  // Since `a` is declared as INTEGER PRIMARY KEY, it is an alias for SQLITE's
  // rowid. This means `a` can only take on integer values. Attempting to insert
  // anything else causes the error callback handler to be called with
  // SQLITE_MISMATCH as error code.
  Statement insert(db_.GetUniqueStatement("INSERT INTO rows(a) VALUES(?)"));
  ASSERT_TRUE(insert.is_valid());
  insert.BindString(0, "not an integer, not suitable as primary key value");
  EXPECT_FALSE(insert.Run())
      << "Invalid statement should not Run() successfully";
  EXPECT_TRUE(error_callback_called)
      << "Statement::Run() should report errors to the database error callback";
  EXPECT_EQ(SQLITE_MISMATCH, error)
      << "Statement::Run() should report errors to the database error callback";
}

// Error expecter works for error running a statement.
TEST_F(StatementTest, ScopedIgnoreError) {
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));

  Statement insert(db_.GetUniqueStatement("INSERT INTO rows(a) VALUES(?)"));
  EXPECT_TRUE(insert.is_valid());
  insert.BindString(0, "not an integer, not suitable as primary key value");

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_MISMATCH);
    EXPECT_FALSE(insert.Run());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(StatementTest, Reset) {
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(a, b) VALUES(3, 12)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(a, b) VALUES(4, 13)"));

  Statement insert(db_.GetUniqueStatement("SELECT b FROM rows WHERE a=?"));
  insert.BindInt64(0, 3);
  ASSERT_TRUE(insert.Step());
  EXPECT_EQ(12, insert.ColumnInt64(0));
  ASSERT_FALSE(insert.Step());

  insert.Reset(/*clear_bound_vars=*/false);
  // Verify that we can get all rows again.
  ASSERT_TRUE(insert.Step());
  EXPECT_EQ(12, insert.ColumnInt64(0));
  EXPECT_FALSE(insert.Step());

  insert.Reset(/*clear_bound_vars=*/true);
  ASSERT_FALSE(insert.Step());
}

TEST_F(StatementTest, BindInt64) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE ints(id INTEGER PRIMARY KEY, i INTEGER NOT NULL)"));

  const std::vector<int64_t> values = {
      // Small positive values.
      0,
      1,
      2,
      10,
      101,
      1002,

      // Small negative values.
      -1,
      -2,
      -3,
      -10,
      -101,
      -1002,

      // Large values.
      std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::min(),
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO ints(i) VALUES(?)"));
  for (int64_t value : values) {
    insert.BindInt64(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/*clear_bound_vars=*/true);
  }

  Statement select(db_.GetUniqueStatement("SELECT i FROM ints ORDER BY id"));
  for (int64_t value : values) {
    ASSERT_TRUE(select.Step());
    int64_t column_value = select.ColumnInt64(0);
    EXPECT_EQ(value, column_value);
  }
}

// Chrome features rely on being able to use uint64_t with ColumnInt64().
// This is supported, because (starting in C++20) casting between signed and
// unsigned integers is well-defined in both directions. This test ensures that
// the casting works as expected.
TEST_F(StatementTest, BindInt64_FromUint64t) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  static constexpr char kSql[] =
      "CREATE TABLE ints(id INTEGER PRIMARY KEY NOT NULL, i INTEGER NOT NULL)";
  ASSERT_TRUE(db_.Execute(kSql));

  const std::vector<uint64_t> values = {
      // Small positive values.
      0,
      1,
      2,
      10,
      101,
      1002,

      // Large values.
      std::numeric_limits<int64_t>::max() - 1,
      std::numeric_limits<int64_t>::max(),
      std::numeric_limits<uint64_t>::max() - 1,
      std::numeric_limits<uint64_t>::max(),
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO ints(i) VALUES(?)"));
  for (uint64_t value : values) {
    insert.BindInt64(0, static_cast<int64_t>(value));
    ASSERT_TRUE(insert.Run());
    insert.Reset(/*clear_bound_vars=*/true);
  }

  Statement select(db_.GetUniqueStatement("SELECT i FROM ints ORDER BY id"));
  for (uint64_t value : values) {
    ASSERT_TRUE(select.Step());
    int64_t column_value = select.ColumnInt64(0);
    uint64_t cast_column_value = static_cast<uint64_t>(column_value);
    EXPECT_EQ(value, cast_column_value) << " column_value: " << column_value;
  }
}

TEST_F(StatementTest, BindBlob) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE blobs(id INTEGER PRIMARY KEY NOT NULL, b BLOB NOT NULL)"));

  const std::vector<std::vector<uint8_t>> values = {
      {},
      {0x01},
      {0x41, 0x42, 0x43, 0x44},
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO blobs(b) VALUES(?)"));
  for (const std::vector<uint8_t>& value : values) {
    insert.BindBlob(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/*clear_bound_vars=*/true);
  }

  Statement select(db_.GetUniqueStatement("SELECT b FROM blobs ORDER BY id"));
  for (const std::vector<uint8_t>& value : values) {
    ASSERT_TRUE(select.Step());
    std::vector<uint8_t> column_value;
    EXPECT_TRUE(select.ColumnBlobAsVector(0, &column_value));
    EXPECT_EQ(value, column_value);
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(StatementTest, BindBlob_String16Overload) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE blobs(id INTEGER PRIMARY KEY NOT NULL, b BLOB NOT NULL)"));

  const std::vector<std::u16string> values = {
      std::u16string(), std::u16string(u"hello\n"), std::u16string(u"😀🍩🎉"),
      std::u16string(u"\xd800\xdc00text"),  // surrogate pair with text
      std::u16string(u"\xd8ff"),            // unpaired high surrogate
      std::u16string(u"\xdddd"),            // unpaired low surrogate
      std::u16string(u"\xdc00\xd800text"),  // lone low followed by lone high
                                            // surrogate and text
      std::u16string(1024, 0xdb23),         // long invalid UTF-16
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO blobs(b) VALUES(?)"));
  for (const std::u16string& value : values) {
    insert.BindBlob(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/*clear_bound_vars=*/true);
  }

  Statement select(db_.GetUniqueStatement("SELECT b FROM blobs ORDER BY id"));
  for (const std::u16string& value : values) {
    ASSERT_TRUE(select.Step());
    std::u16string column_value;
    EXPECT_TRUE(select.ColumnBlobAsString16(0, &column_value));
    EXPECT_EQ(value, column_value);
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(StatementTest, BindString) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE texts(id INTEGER PRIMARY KEY NOT NULL, t TEXT NOT NULL)"));

  const std::vector<std::string> values = {
      "",
      "a",
      "\x01",
      std::string("\x00", 1),
      "abcd",
      "\x01\x02\x03\x04",
      std::string("\x01Test", 5),
      std::string("\x00Test", 5),
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO texts(t) VALUES(?)"));
  for (const std::string& value : values) {
    insert.BindString(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/*clear_bound_vars=*/true);
  }

  {
    Statement select(db_.GetUniqueStatement("SELECT t FROM texts ORDER BY id"));
    for (const std::string& value : values) {
      ASSERT_TRUE(select.Step());
      EXPECT_EQ(value, select.ColumnString(0));
    }
    EXPECT_FALSE(select.Step());
  }

  {
    Statement select(db_.GetUniqueStatement("SELECT t FROM texts ORDER BY id"));
    for (const std::string& value : values) {
      ASSERT_TRUE(select.Step());
      EXPECT_EQ(value, select.ColumnStringView(0));
    }
    EXPECT_FALSE(select.Step());
  }
}

TEST_F(StatementTest, BindString_NullData) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE texts(id INTEGER PRIMARY KEY NOT NULL, t TEXT NOT NULL)"));

  Statement insert(db_.GetUniqueStatement("INSERT INTO texts(t) VALUES(?)"));
  insert.BindString(0, std::string_view(nullptr, 0));
  ASSERT_TRUE(insert.Run());

  Statement select(db_.GetUniqueStatement("SELECT t FROM texts ORDER BY id"));
  ASSERT_TRUE(select.Step());
  EXPECT_EQ(std::string(), select.ColumnString(0));

  EXPECT_FALSE(select.Step());
}

TEST_F(StatementTest, GetSQLStatementExcludesBoundValues) {
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE texts(id INTEGER PRIMARY KEY NOT NULL, t TEXT NOT NULL)"));

  Statement insert(db_.GetUniqueStatement("INSERT INTO texts(t) VALUES(?)"));
  insert.BindString(0, "John Doe");
  ASSERT_TRUE(insert.Run());

  // Verify that GetSQLStatement doesn't leak any bound values that may be PII.
  std::string sql_statement = insert.GetSQLStatement();
  EXPECT_TRUE(base::Contains(sql_statement, "INSERT INTO texts(t) VALUES(?)"));
  EXPECT_TRUE(base::Contains(sql_statement, "VALUES"));
  EXPECT_FALSE(base::Contains(sql_statement, "Doe"));

  // Sanity check that the name was actually committed.
  Statement select(db_.GetUniqueStatement("SELECT t FROM texts ORDER BY id"));
  ASSERT_TRUE(select.Step());
  EXPECT_EQ(select.ColumnString(0), "John Doe");
}

TEST_F(StatementTest, RunReportsPerformanceMetrics) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE rows(a INTEGER PRIMARY KEY NOT NULL, b INTEGER NOT NULL)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(a, b) VALUES(12, 42)"));

  histogram_tester.ExpectTotalCount("Sql.Statement.Test.VMSteps", 0);

  {
    Statement select(db_.GetUniqueStatement("SELECT b FROM rows WHERE a=?"));
    select.BindInt64(0, 12);
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(select.ColumnInt64(0), 42);
  }

  histogram_tester.ExpectTotalCount("Sql.Statement.Test.VMSteps", 1);
}

}  // namespace
}  // namespace sql
