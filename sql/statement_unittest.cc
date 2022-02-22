// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece_forward.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/error_callback_support.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace {

class SQLStatementTest : public testing::Test {
 public:
  ~SQLStatementTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("statement_test.sqlite")));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_;
};

TEST_F(SQLStatementTest, Assign) {
  Statement s;
  EXPECT_FALSE(s.is_valid());

  s.Assign(db_.GetUniqueStatement("CREATE TABLE foo (a, b)"));
  EXPECT_TRUE(s.is_valid());
}

TEST_F(SQLStatementTest, Run) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));

  Statement s(db_.GetUniqueStatement("SELECT b FROM foo WHERE a=?"));
  EXPECT_FALSE(s.Succeeded());

  // Stepping it won't work since we haven't bound the value.
  EXPECT_FALSE(s.Step());

  // Run should fail since this produces output, and we should use Step(). This
  // gets a bit wonky since sqlite says this is OK so succeeded is set.
  s.Reset(true);
  s.BindInt(0, 3);
  EXPECT_FALSE(s.Run());
  EXPECT_EQ(SQLITE_ROW, db_.GetErrorCode());
  EXPECT_TRUE(s.Succeeded());

  // Resetting it should put it back to the previous state (not runnable).
  s.Reset(true);
  EXPECT_FALSE(s.Succeeded());

  // Binding and stepping should produce one row.
  s.BindInt(0, 3);
  EXPECT_TRUE(s.Step());
  EXPECT_TRUE(s.Succeeded());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_FALSE(s.Step());
  EXPECT_TRUE(s.Succeeded());
}

// Error callback called for error running a statement.
TEST_F(SQLStatementTest, ErrorCallback) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a INTEGER PRIMARY KEY, b)"));

  int error = SQLITE_OK;
  ScopedErrorCallback sec(&db_,
                          base::BindRepeating(&CaptureErrorCallback, &error));

  // Insert in the foo table the primary key. It is an error to insert
  // something other than an number. This error causes the error callback
  // handler to be called with SQLITE_MISMATCH as error code.
  Statement s(db_.GetUniqueStatement("INSERT INTO foo (a) VALUES (?)"));
  EXPECT_TRUE(s.is_valid());
  s.BindCString(0, "bad bad");
  EXPECT_FALSE(s.Run());
  EXPECT_EQ(SQLITE_MISMATCH, error);
}

// Error expecter works for error running a statement.
TEST_F(SQLStatementTest, ScopedIgnoreError) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a INTEGER PRIMARY KEY, b)"));

  Statement s(db_.GetUniqueStatement("INSERT INTO foo (a) VALUES (?)"));
  EXPECT_TRUE(s.is_valid());

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_MISMATCH);
    s.BindCString(0, "bad bad");
    ASSERT_FALSE(s.Run());
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(SQLStatementTest, Reset) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO foo (a, b) VALUES (4, 13)"));

  Statement s(db_.GetUniqueStatement("SELECT b FROM foo WHERE a = ? "));
  s.BindInt(0, 3);
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
  ASSERT_FALSE(s.Step());

  s.Reset(false);
  // Verify that we can get all rows again.
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_FALSE(s.Step());

  s.Reset(true);
  ASSERT_FALSE(s.Step());
}

TEST_F(SQLStatementTest, BindInt64) {
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
    insert.Reset(/* clear_bound_vars= */ true);
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
TEST_F(SQLStatementTest, BindInt64_FromUint64t) {
  // `id` makes SQLite's rowid mechanism explicit. We rely on it to retrieve
  // the rows in the same order that they were inserted.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE ints(id INTEGER PRIMARY KEY, i INTEGER NOT NULL)"));

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
    insert.Reset(/* clear_bound_vars= */ true);
  }

  Statement select(db_.GetUniqueStatement("SELECT i FROM ints ORDER BY id"));
  for (uint64_t value : values) {
    ASSERT_TRUE(select.Step());
    int64_t column_value = select.ColumnInt64(0);
    uint64_t cast_column_value = static_cast<uint64_t>(column_value);
    EXPECT_EQ(value, cast_column_value) << " column_value: " << column_value;
  }
}

TEST_F(SQLStatementTest, BindBlob) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE blobs (b BLOB NOT NULL)"));

  const std::vector<std::vector<uint8_t>> values = {
      {},
      {0x01},
      {0x41, 0x42, 0x43, 0x44},
  };

  Statement insert(db_.GetUniqueStatement("INSERT INTO blobs VALUES(?)"));
  for (const std::vector<uint8_t>& value : values) {
    insert.BindBlob(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/* clear_bound_vars= */ true);
  }

  Statement select(db_.GetUniqueStatement("SELECT b FROM blobs"));
  for (const std::vector<uint8_t>& value : values) {
    ASSERT_TRUE(select.Step());
    std::vector<uint8_t> column_value;
    EXPECT_TRUE(select.ColumnBlobAsVector(0, &column_value));
    EXPECT_EQ(value, column_value);
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLStatementTest, BindString) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE strings (s TEXT NOT NULL)"));

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

  Statement insert(db_.GetUniqueStatement("INSERT INTO strings VALUES(?)"));
  for (const std::string& value : values) {
    insert.BindString(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/* clear_bound_vars= */ true);
  }

  Statement select(db_.GetUniqueStatement("SELECT s FROM strings"));
  for (const std::string& value : values) {
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(value, select.ColumnString(0));
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(SQLStatementTest, BindString_NullData) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE strings (s TEXT NOT NULL)"));

  Statement insert(db_.GetUniqueStatement("INSERT INTO strings VALUES(?)"));
  insert.BindString(0, base::StringPiece(nullptr, 0));
  ASSERT_TRUE(insert.Run());

  Statement select(db_.GetUniqueStatement("SELECT s FROM strings"));
  ASSERT_TRUE(select.Step());
  EXPECT_EQ(std::string(), select.ColumnString(0));

  EXPECT_FALSE(select.Step());
}

}  // namespace
}  // namespace sql
