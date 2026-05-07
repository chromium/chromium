// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/table_management_helpers.h"

#include <stdint.h>

#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class TableManagementHelpersTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(db_.OpenInMemory()); }

 protected:
  Database db_{test::kTestTag};
};

// Tests that the `CreateTable()` function returns true on success and that the
// table and columns are created as expected.
TEST_F(TableManagementHelpersTest, CreateTableReturnsTrueOnSuccess) {
  ASSERT_TRUE(CreateTable(db_, "test_table",
                          /*column_names_and_types=*/
                          {
                              {"integer_column", "INTEGER"},
                              {"text_column", "TEXT"},
                              {"blob_column", "BLOB"},
                          }));

  ASSERT_TRUE(db_.DoesTableExist("test_table"));

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "integer_column"));
  test::ColumnInfo integer_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "integer_column");
  EXPECT_EQ(integer_column_info.data_type, "INTEGER");
  EXPECT_FALSE(integer_column_info.is_in_primary_key);

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "text_column"));
  test::ColumnInfo text_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "text_column");
  EXPECT_EQ(text_column_info.data_type, "TEXT");
  EXPECT_FALSE(text_column_info.is_in_primary_key);

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "blob_column"));
  test::ColumnInfo blob_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "blob_column");
  EXPECT_EQ(blob_column_info.data_type, "BLOB");
  EXPECT_FALSE(blob_column_info.is_in_primary_key);
}

// Tests that the `CreateTable()` function when specifying a composite primary
// key returns true on success and that the table and columns are created as
// expected.
TEST_F(TableManagementHelpersTest,
       CreateTableWithCompositePrimaryKeyReturnsTrueOnSuccess) {
  ASSERT_TRUE(CreateTable(db_, "test_table",
                          /*column_names_and_types=*/
                          {
                              {"integer_column", "INTEGER"},
                              {"text_column", "TEXT"},
                              {"blob_column", "BLOB"},
                          },
                          /*composite_primary_key=*/
                          {
                              "integer_column",
                              "text_column",
                          }));

  ASSERT_TRUE(db_.DoesTableExist("test_table"));

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "integer_column"));
  test::ColumnInfo integer_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "integer_column");
  EXPECT_EQ(integer_column_info.data_type, "INTEGER");
  EXPECT_TRUE(integer_column_info.is_in_primary_key);

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "text_column"));
  test::ColumnInfo text_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "text_column");
  EXPECT_EQ(text_column_info.data_type, "TEXT");
  EXPECT_TRUE(text_column_info.is_in_primary_key);

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "blob_column"));
  test::ColumnInfo blob_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "blob_column");
  EXPECT_EQ(blob_column_info.data_type, "BLOB");
  EXPECT_FALSE(blob_column_info.is_in_primary_key);
}

// Tests that the `CreateTable()` function returns false on failure, such as
// when the table already exists.
TEST_F(TableManagementHelpersTest,
       CreateTableReturnsFalseOnDuplicateTableFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  ASSERT_TRUE(db_.DoesTableExist("test_table"));

  EXPECT_FALSE(CreateTable(db_, "test_table",
                           /*column_names_and_types=*/
                           {
                               {"integer_column", "INTEGER"},
                           }));
}

// Tests that the `CreateTable()` function returns false on failure, such as
// when there is a conflict in column names.
TEST_F(TableManagementHelpersTest,
       CreateTableReturnsFalseOnDuplicateColumnFailure) {
  EXPECT_FALSE(CreateTable(db_, "test_table",
                           /*column_names_and_types=*/
                           {
                               {"repeated_column_name", "INTEGER"},
                               {"repeated_column_name", "TEXT"},
                           }));
}

// Tests that the `CreateIndex()` function returns true on success and that the
// index is created as expected.
TEST_F(TableManagementHelpersTest, CreateIndexReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, text_column TEXT, blob_column BLOB);"));

  ASSERT_TRUE(CreateIndex(db_, "test_table",
                          /*columns=*/{"integer_column", "text_column"}));

  EXPECT_TRUE(db_.DoesIndexExist("test_table_integer_column_text_column"));
}

// Tests that the `CreateIndex()` function returns false on failure, such as
// when the table does not exist.
TEST_F(TableManagementHelpersTest,
       CreateIndexReturnsFalseOnMissingTableFailure) {
  ASSERT_FALSE(db_.DoesTableExist("nonexistent_table"));

  EXPECT_FALSE(CreateIndex(db_, "nonexistent_table",
                           /*columns=*/{"integer_column"}));
}

// Tests that the `CreateIndex()` function returns false on failure, such as
// when the columns specified in the index do not exist.
TEST_F(TableManagementHelpersTest,
       CreateIndexReturnsFalseOnMissingColumnFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  ASSERT_FALSE(db_.DoesColumnExist("test_table", "nonexistent_column"));

  EXPECT_FALSE(
      CreateIndex(db_, "test_table", /*columns=*/{"nonexistent_column"}));
}

// Tests that the `CreateIndex()` function returns false on failure, such as
// when the index already exists.
TEST_F(TableManagementHelpersTest,
       CreateIndexReturnsFalseOnDuplicateIndexFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER); "
      "CREATE INDEX test_table_integer_column ON test_table "
      "(integer_column);"));
  ASSERT_TRUE(db_.DoesIndexExist("test_table_integer_column"));

  EXPECT_FALSE(CreateIndex(db_, "test_table", /*columns=*/{"integer_column"}));
}

// Tests that the `InsertBuilder()` function correctly initializes the statement
// with the correct schema and placeholders when `or_replace` is false by
// default.
TEST_F(TableManagementHelpersTest, InsertBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  InsertBuilder(db_, statement, "test_table",
                /*column_names=*/
                {
                    "integer_column",
                    "text_column",
                });

  EXPECT_EQ(
      statement.GetSQLStatement(),
      "INSERT INTO test_table (integer_column, text_column) VALUES (?, ?)");
}

// Tests that the `InsertBuilder()` function correctly initializes the statement
// with the correct schema and placeholders when `or_replace` is true.
TEST_F(TableManagementHelpersTest,
       InsertBuilderWithOrReplaceCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  InsertBuilder(db_, statement, "test_table",
                /*column_names=*/
                {
                    "integer_column",
                    "text_column",
                },
                /*or_replace=*/true);

  EXPECT_EQ(statement.GetSQLStatement(),
            "INSERT OR REPLACE INTO test_table (integer_column, text_column) "
            "VALUES (?, ?)");
}

// Tests that the `CachedInsertBuilder()` function correctly initializes the
// statement with the correct schema and placeholders when `or_replace` is false
// by default.
TEST_F(TableManagementHelpersTest, CachedInsertBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  CachedInsertBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      /*column_names=*/
                      {
                          "integer_column",
                          "text_column",
                      });

  EXPECT_EQ(
      statement.GetSQLStatement(),
      "INSERT INTO test_table (integer_column, text_column) VALUES (?, ?)");
}

// Tests that the `CachedInsertBuilder()` function correctly initializes the
// statement with the correct schema and placeholders when `or_replace` is true.
TEST_F(TableManagementHelpersTest,
       CachedInsertBuilderWithOrReplaceCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  CachedInsertBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      /*column_names=*/
                      {
                          "integer_column",
                          "text_column",
                      },
                      /*or_replace=*/true);

  EXPECT_EQ(statement.GetSQLStatement(),
            "INSERT OR REPLACE INTO test_table (integer_column, text_column) "
            "VALUES (?, ?)");
}

// Tests that the `RenameTable()` function correctly renames the table.
TEST_F(TableManagementHelpersTest, RenameTableReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  ASSERT_TRUE(db_.DoesTableExist("test_table"));
  ASSERT_FALSE(db_.DoesTableExist("renamed_table"));

  EXPECT_TRUE(RenameTable(db_, "test_table", "renamed_table"));
  EXPECT_FALSE(db_.DoesTableExist("test_table"));
  EXPECT_TRUE(db_.DoesTableExist("renamed_table"));
}

// Tests that the `RenameTable()` function returns false on failure, such as
// when the table does not exist.
TEST_F(TableManagementHelpersTest,
       RenameTableReturnsFalseOnMissingTableFailure) {
  ASSERT_FALSE(db_.DoesTableExist("nonexistent_table"));

  EXPECT_FALSE(RenameTable(db_, "nonexistent_table", "renamed_table"));
}

// Tests that the `RenameTable()` function returns false on failure, such as
// when the new table name already exists.
TEST_F(TableManagementHelpersTest,
       RenameTableReturnsFalseOnExistingTableFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER); "
      "CREATE TABLE existing_table (integer_column INTEGER);"));
  ASSERT_TRUE(db_.DoesTableExist("test_table"));
  ASSERT_TRUE(db_.DoesTableExist("existing_table"));

  EXPECT_FALSE(RenameTable(db_, "test_table", "existing_table"));
}

// Tests that the `AddColumn()` function correctly adds the column to the table.
TEST_F(TableManagementHelpersTest, AddColumnReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  ASSERT_FALSE(db_.DoesColumnExist("test_table", "new_column"));

  ASSERT_TRUE(AddColumn(db_, "test_table", "new_column", "TEXT"));

  ASSERT_TRUE(db_.DoesColumnExist("test_table", "new_column"));
  test::ColumnInfo new_column_info = test::ColumnInfo::Create(
      &db_, test::ColumnInfo::kMainDbName, "test_table", "new_column");
  EXPECT_EQ(new_column_info.data_type, "TEXT");
  EXPECT_FALSE(new_column_info.is_in_primary_key);
}

// Tests that the `AddColumn()` function returns false on failure, such as when
// the table does not exist.
TEST_F(TableManagementHelpersTest, AddColumnReturnsFalseOnMissingTableFailure) {
  ASSERT_FALSE(db_.DoesTableExist("nonexistent_table"));

  EXPECT_FALSE(AddColumn(db_, "nonexistent_table", "new_column", "TEXT"));
}

// Tests that the `AddColumn()` function returns false on failure, such as when
// the column already exists.
TEST_F(TableManagementHelpersTest,
       AddColumnReturnsFalseOnDuplicateColumnFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (existing_column INTEGER);"));
  ASSERT_TRUE(db_.DoesColumnExist("test_table", "existing_column"));

  EXPECT_FALSE(AddColumn(db_, "test_table", "existing_column", "TEXT"));
}

// Tests that the `DropColumn()` function correctly drops the column from the
// table.
TEST_F(TableManagementHelpersTest, DropColumnReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, column_to_drop TEXT);"));
  ASSERT_TRUE(db_.DoesTableExist("test_table"));
  ASSERT_TRUE(db_.DoesColumnExist("test_table", "integer_column"));
  ASSERT_TRUE(db_.DoesColumnExist("test_table", "column_to_drop"));

  ASSERT_TRUE(DropColumn(db_, "test_table", "column_to_drop"));

  ASSERT_TRUE(db_.DoesTableExist("test_table"));
  EXPECT_TRUE(db_.DoesColumnExist("test_table", "integer_column"));
  EXPECT_FALSE(db_.DoesColumnExist("test_table", "column_to_drop"));
}

// Tests that the `DropColumn()` function returns false on failure, such as when
// the table does not exist.
TEST_F(TableManagementHelpersTest,
       DropColumnReturnsFalseOnMissingTableFailure) {
  ASSERT_FALSE(db_.DoesTableExist("nonexistent_table"));

  EXPECT_FALSE(DropColumn(db_, "nonexistent_table", "integer_column"));
}

// Tests that the `DropColumn()` function returns false on failure, such as when
// the column does not exist.
TEST_F(TableManagementHelpersTest,
       DropColumnReturnsFalseOnMissingColumnFailure) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  ASSERT_TRUE(db_.DoesTableExist("test_table"));
  ASSERT_FALSE(db_.DoesColumnExist("test_table", "nonexistent_column"));

  EXPECT_FALSE(DropColumn(db_, "test_table", "nonexistent_column"));
}

// Tests that the `DeleteBuilder()` function correctly initializes the
// statement.
TEST_F(TableManagementHelpersTest, DeleteBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  Statement statement;
  DeleteBuilder(db_, statement, "test_table");

  EXPECT_EQ(statement.GetSQLStatement(), "DELETE FROM test_table");
}

// Tests that the `DeleteBuilder()` function correctly initializes the statement
// when a `where_clause` is specified.
TEST_F(TableManagementHelpersTest,
       DeleteBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));

  Statement statement;
  DeleteBuilder(db_, statement, "test_table",
                /*where_clause=*/"integer_column = ?");

  EXPECT_EQ(statement.GetSQLStatement(),
            "DELETE FROM test_table WHERE integer_column = ?");
}

// Tests that the `CachedDeleteBuilder()` function correctly initializes the
// statement.
TEST_F(TableManagementHelpersTest, CachedDeleteBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));
  Statement statement;
  CachedDeleteBuilder(SQL_FROM_HERE, db_, statement, "test_table");

  EXPECT_EQ(statement.GetSQLStatement(), "DELETE FROM test_table");
}

// Tests that the `CachedDeleteBuilder()` function correctly initializes the
// statement when a `where_clause` is specified.
TEST_F(TableManagementHelpersTest,
       CachedDeleteBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER);"));

  Statement statement;
  CachedDeleteBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      /*where_clause=*/"integer_column = ?");

  EXPECT_EQ(statement.GetSQLStatement(),
            "DELETE FROM test_table WHERE integer_column = ?");
}

// Tests that the `DeleteAllRows()` function correctly deletes all rows from
// the table when no `where_clause` is specified and returns true on success.
TEST_F(TableManagementHelpersTest, DeleteAllRowsReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER); "
      "INSERT INTO test_table (integer_column) VALUES (1); "
      "INSERT INTO test_table (integer_column) VALUES (2);"));

  ASSERT_TRUE(DeleteAllRows(db_, "test_table"));

  Statement count_remaining_rows;
  count_remaining_rows.Assign(
      db_.GetUniqueStatement("SELECT COUNT(*) FROM test_table"));
  ASSERT_TRUE(count_remaining_rows.Step());
  EXPECT_EQ(count_remaining_rows.ColumnInt(0), 0);
}

// Tests that the `DeleteAllRows()` function returns false on failure, such as
// when the table does not exist.
TEST_F(TableManagementHelpersTest,
       DeleteAllRowsReturnsFalseOnMissingTableFailure) {
  EXPECT_FALSE(DeleteAllRows(db_, "nonexistent_table"));
}

// Tests that the `DeleteFromTable()` function correctly deletes matching rows
// from the table and returns true on success.
TEST_F(TableManagementHelpersTest, DeleteFromTableReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (text_column TEXT); "
      "INSERT INTO test_table (text_column) VALUES ('value_to_delete'); "
      "INSERT INTO test_table (text_column) VALUES ('value_to_delete'); "
      "INSERT INTO test_table (text_column) VALUES ('other_value');"));

  ASSERT_TRUE(
      DeleteFromTable(db_, "test_table",
                      /*where_clause=*/"text_column = 'value_to_delete'"));

  Statement remaining_rows;
  remaining_rows.Assign(
      db_.GetUniqueStatement("SELECT text_column FROM test_table"));
  ASSERT_TRUE(remaining_rows.Step());
  EXPECT_EQ(remaining_rows.ColumnString(0), "other_value");
  EXPECT_FALSE(remaining_rows.Step());
}

// Tests that the `DeleteFromTable()` function returns false on failure, such
// as when the table does not exist.
TEST_F(TableManagementHelpersTest,
       DeleteFromTableReturnsFalseOnMissingTableFailure) {
  EXPECT_FALSE(
      DeleteFromTable(db_, "nonexistent_table",
                      /*where_clause=*/"text_column = 'value_to_delete'"));
}

// Tests that the `DeleteWhereColumnEq()` function for strings correctly deletes
// matching rows from the table and returns true on success.
TEST_F(TableManagementHelpersTest,
       DeleteWhereColumnEqStringValueReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (text_column TEXT); "
      "INSERT INTO test_table (text_column) VALUES ('value_to_delete'); "
      "INSERT INTO test_table (text_column) VALUES ('value_to_delete'); "
      "INSERT INTO test_table (text_column) VALUES ('other_value');"));

  ASSERT_TRUE(
      DeleteWhereColumnEq(db_, "test_table", "text_column", "value_to_delete"));

  Statement remaining_rows;
  remaining_rows.Assign(
      db_.GetUniqueStatement("SELECT text_column FROM test_table"));
  ASSERT_TRUE(remaining_rows.Step());
  EXPECT_EQ(remaining_rows.ColumnString(0), "other_value");
  EXPECT_FALSE(remaining_rows.Step());
}

// Tests that the `DeleteWhereColumnEq()` function for strings returns false on
// failure, such as when the table does not exist.
TEST_F(TableManagementHelpersTest,
       DeleteWhereColumnEqStringValueReturnsFalseOnMissingTableFailure) {
  EXPECT_FALSE(DeleteWhereColumnEq(db_, "nonexistent_table", "text_column",
                                   "value_to_delete"));
}

// Tests that the `DeleteWhereColumnEq()` function for `int64_t`s correctly
// deletes matching rows from the table and returns true on success.
TEST_F(TableManagementHelpersTest,
       DeleteWhereColumnEqIntValueReturnsTrueOnSuccess) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER); "
      "INSERT INTO test_table (integer_column) VALUES (2); "
      "INSERT INTO test_table (integer_column) VALUES (1); "
      "INSERT INTO test_table (integer_column) VALUES (2);"));

  ASSERT_TRUE(DeleteWhereColumnEq(db_, "test_table", "integer_column", 2));

  Statement remaining_rows;
  remaining_rows.Assign(
      db_.GetUniqueStatement("SELECT integer_column FROM test_table"));
  EXPECT_TRUE(remaining_rows.Step());
  EXPECT_EQ(remaining_rows.ColumnInt(0), 1);
  EXPECT_FALSE(remaining_rows.Step());
}

// Tests that the `DeleteWhereColumnEq()` function for `int64_t`s returns false
// on failure, such as when the table does not exist.
TEST_F(TableManagementHelpersTest,
       DeleteWhereColumnEqIntValueReturnsFalseOnMissingTableFailure) {
  EXPECT_FALSE(
      DeleteWhereColumnEq(db_, "nonexistent_table", "integer_column", 2));
}

// Tests that the `UpdateBuilder()` function correctly initializes the statement
// with the correct schema and placeholders.
TEST_F(TableManagementHelpersTest, UpdateBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  UpdateBuilder(db_, statement, "test_table",
                /*column_names=*/
                {
                    "integer_column",
                    "text_column",
                });

  EXPECT_EQ(statement.GetSQLStatement(),
            "UPDATE test_table SET integer_column = ?, text_column = ?");
}

// Tests that the `UpdateBuilder()` function correctly initializes the statement
// with the correct schema and placeholders when a `where_clause` is specified.
TEST_F(TableManagementHelpersTest,
       UpdateBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, text_column TEXT, blob_column BLOB);"));

  Statement statement;
  UpdateBuilder(db_, statement, "test_table", {"text_column", "blob_column"},
                /*where_clause=*/"integer_column = 1");

  EXPECT_EQ(statement.GetSQLStatement(),
            "UPDATE test_table SET text_column = ?, blob_column = ? WHERE "
            "integer_column = 1");
}

// Tests that the `CachedUpdateBuilder()` function correctly initializes the
// statement with the correct schema and placeholders.
TEST_F(TableManagementHelpersTest, CachedUpdateBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  CachedUpdateBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      /*column_names=*/
                      {
                          "integer_column",
                          "text_column",
                      });

  EXPECT_EQ(statement.GetSQLStatement(),
            "UPDATE test_table SET integer_column = ?, text_column = ?");
}

// Tests that the `CachedUpdateBuilder()` function correctly initializes the
// statement with the correct schema and placeholders when a `where_clause` is
// specified.
TEST_F(TableManagementHelpersTest,
       CachedUpdateBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, text_column TEXT, blob_column BLOB);"));

  Statement statement;
  CachedUpdateBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      {"text_column", "blob_column"},
                      /*where_clause=*/"integer_column = 1");

  EXPECT_EQ(statement.GetSQLStatement(),
            "UPDATE test_table SET text_column = ?, blob_column = ? WHERE "
            "integer_column = 1");
}

// Tests that the `SelectBuilder()` function correctly initializes the statement
// with the correct schema.
TEST_F(TableManagementHelpersTest, SelectBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  SelectBuilder(db_, statement, "test_table",
                /*columns=*/
                {
                    "integer_column",
                    "text_column",
                });

  EXPECT_EQ(statement.GetSQLStatement(),
            "SELECT integer_column, text_column FROM test_table ");
}

// Tests that the `SelectBuilder()` function correctly initializes the statement
// with the correct schema when `modifiers` is specified.
TEST_F(TableManagementHelpersTest,
       SelectBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, text_column TEXT, blob_column BLOB);"));

  Statement statement;
  SelectBuilder(db_, statement, "test_table", {"text_column", "blob_column"},
                /*modifiers=*/"WHERE integer_column = 1");

  EXPECT_EQ(statement.GetSQLStatement(),
            "SELECT text_column, blob_column FROM test_table WHERE "
            "integer_column = 1");
}

// Tests that the `CachedSelectBuilder()` function correctly initializes the
// statement with the correct schema.
TEST_F(TableManagementHelpersTest, CachedSelectBuilderCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table (integer_column INTEGER, text_column TEXT);"));

  Statement statement;
  CachedSelectBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      /*columns=*/
                      {
                          "integer_column",
                          "text_column",
                      });

  EXPECT_EQ(statement.GetSQLStatement(),
            "SELECT integer_column, text_column FROM test_table ");
}

// Tests that the `CachedSelectBuilder()` function correctly initializes the
// statement with the correct schema when `modifiers` is specified.
TEST_F(TableManagementHelpersTest,
       CachedSelectBuilderWithWhereClauseCreatesCorrectStatement) {
  ASSERT_TRUE(db_.ExecuteScriptForTesting(
      "CREATE TABLE test_table "
      "(integer_column INTEGER, text_column TEXT, blob_column BLOB);"));

  Statement statement;
  CachedSelectBuilder(SQL_FROM_HERE, db_, statement, "test_table",
                      {"text_column", "blob_column"},
                      /*modifiers=*/"WHERE integer_column = 1");

  EXPECT_EQ(statement.GetSQLStatement(),
            "SELECT text_column, blob_column FROM test_table WHERE "
            "integer_column = 1");
}

}  // namespace

}  // namespace sql
