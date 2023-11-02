// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/database_test_peer.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

class RecoverModuleTest : public testing::Test {
 public:
  ~RecoverModuleTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("recovery_test.sqlite")));
    ASSERT_TRUE(DatabaseTestPeer::EnableRecoveryExtension(&db_));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  sql::Database db_{sql::DatabaseOptions{
      .enable_virtual_tables_discouraged = true,
  }};
};

TEST_F(RecoverModuleTest, CreateVtable) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  EXPECT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                  "USING recover(backing, t TEXT)"));
}
TEST_F(RecoverModuleTest, CreateVtableWithDatabaseSpecifier) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  EXPECT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                  "USING recover(main.backing, t TEXT)"));
}
TEST_F(RecoverModuleTest, CreateVtableOnSqliteSchema) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  EXPECT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing USING recover("
                  "sqlite_schema, type TEXT, name TEXT, tbl_name TEXT, "
                  "rootpage INTEGER, sql TEXT)"));
}

TEST_F(RecoverModuleTest, CreateVtableFailsOnNonTempTable) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(db_.Execute(
        "CREATE VIRTUAL TABLE recover_backing USING recover(backing, t TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnMissingTable) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_missing "
                    "USING recover(missing, t TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnMissingDatabase) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(db.backing, t TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnTableWithInvalidQualifier) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing invalid, t TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnMissingTableName) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(main., t TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnMissingSchemaSpec) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, CreateVtableFailsOnMissingDbName) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(.backing)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}

TEST_F(RecoverModuleTest, ColumnTypeMappingAny) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  EXPECT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                  "USING recover(backing, t ANY)"));

  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "t");
  EXPECT_EQ("(nullptr)", column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_FALSE(column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
TEST_F(RecoverModuleTest, ColumnTypeMappingAnyNotNull) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  EXPECT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                  "USING recover(backing, t ANY NOT NULL)"));

  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "t");
  EXPECT_EQ("(nullptr)", column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_TRUE(column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
TEST_F(RecoverModuleTest, ColumnTypeMappingAnyStrict) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing, t ANY STRICT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}

TEST_F(RecoverModuleTest, ColumnTypeExtraKeyword) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing, t INTEGER SOMETHING)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, ColumnTypeNotNullExtraKeyword) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing, t INTEGER NOT NULL SOMETHING)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, ColumnTypeDoubleTypes) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing "
                    "USING recover(backing, t INTEGER FLOAT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}
TEST_F(RecoverModuleTest, ColumnTypeNotNullDoubleTypes) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE backing(t TEXT)"));
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_.Execute("CREATE VIRTUAL TABLE temp.recover_backing USING recover("
                    "backing, t INTEGER NOT NULL TEXT)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
}

class RecoverModuleColumnTypeMappingTest
    : public RecoverModuleTest,
      public ::testing::WithParamInterface<
          std::tuple<const char*, const char*, bool>> {
 public:
  ~RecoverModuleColumnTypeMappingTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("recovery_test.sqlite")));
    ASSERT_TRUE(DatabaseTestPeer::EnableRecoveryExtension(&db_));

    std::string sql =
        base::StringPrintf("CREATE TABLE backing(data %s)", SchemaType());
    ASSERT_TRUE(db_.Execute(sql.c_str()));
  }

  void CreateRecoveryTable(const char* suffix) {
    std::string sql = base::StringPrintf(
        "CREATE VIRTUAL TABLE temp.recover_backing "
        "USING recover(backing, data %s%s)",
        SchemaType(), suffix);
    ASSERT_TRUE(db_.Execute(sql.c_str()));
  }

  const char* SchemaType() const { return std::get<0>(GetParam()); }
  const char* ExpectedType() const { return std::get<1>(GetParam()); }
  bool IsAlwaysNonNull() const { return std::get<2>(GetParam()); }

 protected:
  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};
TEST_P(RecoverModuleColumnTypeMappingTest, Unqualified) {
  CreateRecoveryTable("");
  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "data");
  EXPECT_EQ(ExpectedType(), column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_EQ(IsAlwaysNonNull(), column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
TEST_P(RecoverModuleColumnTypeMappingTest, NotNull) {
  CreateRecoveryTable(" NOT NULL");
  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "data");
  EXPECT_EQ(ExpectedType(), column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_TRUE(column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
TEST_P(RecoverModuleColumnTypeMappingTest, Strict) {
  CreateRecoveryTable(" STRICT");
  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "data");
  EXPECT_EQ(ExpectedType(), column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_EQ(IsAlwaysNonNull(), column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
TEST_P(RecoverModuleColumnTypeMappingTest, StrictNotNull) {
  CreateRecoveryTable(" STRICT NOT NULL");
  sql::test::ColumnInfo column_info =
      sql::test::ColumnInfo::Create(&db_, "temp", "recover_backing", "data");
  EXPECT_EQ(ExpectedType(), column_info.data_type);
  EXPECT_EQ("BINARY", column_info.collation_sequence);
  EXPECT_TRUE(column_info.has_non_null_constraint);
  EXPECT_FALSE(column_info.is_in_primary_key);
  EXPECT_FALSE(column_info.is_auto_incremented);
}
INSTANTIATE_TEST_SUITE_P(
    All,
    RecoverModuleColumnTypeMappingTest,
    ::testing::Values(std::make_tuple("TEXT", "TEXT", false),
                      std::make_tuple("INTEGER", "INTEGER", false),
                      std::make_tuple("FLOAT", "FLOAT", false),
                      std::make_tuple("BLOB", "BLOB", false),
                      std::make_tuple("NUMERIC", "NUMERIC", false),
                      std::make_tuple("ROWID", "INTEGER", true)));

namespace {

void GenerateAlteredTable(sql::Database* db) {
  ASSERT_TRUE(db->Execute("CREATE TABLE altered(t TEXT)"));
  ASSERT_TRUE(db->Execute("INSERT INTO altered VALUES('a')"));
  ASSERT_TRUE(db->Execute("INSERT INTO altered VALUES('b')"));
  ASSERT_TRUE(db->Execute("INSERT INTO altered VALUES('c')"));
  ASSERT_TRUE(db->Execute(
      "ALTER TABLE altered ADD COLUMN i INTEGER NOT NULL DEFAULT 10"));
  ASSERT_TRUE(db->Execute("INSERT INTO altered VALUES('d', 5)"));
}

}  // namespace

TEST_F(RecoverModuleTest, ReadFromAlteredTableNullDefaults) {
  GenerateAlteredTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_altered "
                  "USING recover(altered, t TEXT, i INTEGER)"));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT t, i FROM recover_altered ORDER BY rowid"));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ("a", statement.ColumnString(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ("b", statement.ColumnString(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ("c", statement.ColumnString(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ("d", statement.ColumnString(0));
  EXPECT_EQ(5, statement.ColumnInt(1));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, ReadFromAlteredTableSkipsNulls) {
  GenerateAlteredTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_altered "
                  "USING recover(altered, t TEXT, i INTEGER NOT NULL)"));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT t, i FROM recover_altered ORDER BY rowid"));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ("d", statement.ColumnString(0));
  EXPECT_EQ(5, statement.ColumnInt(1));
  EXPECT_FALSE(statement.Step());
}

namespace {

void GenerateSizedTable(sql::Database* db,
                        int row_count,
                        const std::string& prefix) {
  ASSERT_TRUE(db->Execute("CREATE TABLE sized(t TEXT, i INTEGER)"));

  sql::Transaction transaction(db);
  ASSERT_TRUE(transaction.Begin());
  sql::Statement statement(
      db->GetUniqueStatement("INSERT INTO sized VALUES(?, ?)"));

  for (int i = 0; i < row_count; ++i) {
    statement.BindString(0, base::StringPrintf("%s%d", prefix.c_str(), i));
    statement.BindInt(1, i);
    ASSERT_TRUE(statement.Run());
    statement.Reset(/* clear_bound_vars= */ true);
  }
  ASSERT_TRUE(transaction.Commit());
}

}  // namespace

TEST_F(RecoverModuleTest, LeafNodes) {
  GenerateSizedTable(&db_, 10, "Leaf-node-generating line ");
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_sized "
                  "USING recover(sized, t TEXT, i INTEGER NOT NULL)"));

  sql::Statement statement(
      db_.GetUniqueStatement("SELECT t, i FROM recover_sized ORDER BY rowid"));
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(base::StringPrintf("Leaf-node-generating line %d", i),
              statement.ColumnString(0));
    EXPECT_EQ(i, statement.ColumnInt(1));
  }
  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, EmptyTable) {
  GenerateSizedTable(&db_, 0, "");
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_sized "
                  "USING recover(sized, t TEXT, i INTEGER NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, t, i FROM recover_sized ORDER BY rowid"));
  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, SingleLevelInteriorNodes) {
  GenerateSizedTable(&db_, 100, "Interior-node-generating line ");
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_sized "
                  "USING recover(sized, t TEXT, i INTEGER NOT NULL)"));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, t, i FROM recover_sized ORDER BY rowid"));
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(i + 1, statement.ColumnInt(0));
    EXPECT_EQ(base::StringPrintf("Interior-node-generating line %d", i),
              statement.ColumnString(1));
    EXPECT_EQ(i, statement.ColumnInt(2));
  }
  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, MultiLevelInteriorNodes) {
  GenerateSizedTable(&db_, 5000, "Interior-node-generating line ");
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_sized "
                  "USING recover(sized, t TEXT, i INTEGER NOT NULL)"));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, t, i FROM recover_sized ORDER BY rowid"));
  for (int i = 0; i < 5000; ++i) {
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(i + 1, statement.ColumnInt(0));
    EXPECT_EQ(base::StringPrintf("Interior-node-generating line %d", i),
              statement.ColumnString(1));
    EXPECT_EQ(i, statement.ColumnInt(2));
  }
  EXPECT_FALSE(statement.Step());
}

namespace {

void GenerateTypesTable(sql::Database* db) {
  ASSERT_TRUE(db->Execute("CREATE TABLE types(rowtype TEXT, value)"));

  ASSERT_TRUE(db->Execute("INSERT INTO types VALUES('NULL', NULL)"));
  ASSERT_TRUE(db->Execute("INSERT INTO types VALUES('INTEGER', 17)"));
  ASSERT_TRUE(db->Execute("INSERT INTO types VALUES('FLOAT', 3.1415927)"));
  ASSERT_TRUE(db->Execute("INSERT INTO types VALUES('TEXT', 'This is text')"));
  ASSERT_TRUE(db->Execute(
      "INSERT INTO types VALUES('BLOB', CAST('This is a blob' AS BLOB))"));
}

}  // namespace

TEST_F(RecoverModuleTest, Any) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value ANY)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, Integers) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value INTEGER)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullIntegers) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(db_.Execute(
      "CREATE VIRTUAL TABLE temp.recover_types "
      "USING recover(types, rowtype TEXT, value INTEGER NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, Floats) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value FLOAT)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullFloats) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value FLOAT NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, FloatsStrict) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value FLOAT STRICT)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullFloatsStrict) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(db_.Execute(
      "CREATE VIRTUAL TABLE temp.recover_types "
      "USING recover(types, rowtype TEXT, value FLOAT STRICT NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, Texts) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value TEXT)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullTexts) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value TEXT NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, TextsStrict) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value TEXT STRICT)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullTextsStrict) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(db_.Execute(
      "CREATE VIRTUAL TABLE temp.recover_types "
      "USING recover(types, rowtype TEXT, value TEXT STRICT NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, Blobs) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value BLOB)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ("NULL", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, NonNullBlobs) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value BLOB NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, AnyNonNull) {
  GenerateTypesTable(&db_);
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_types "
                  "USING recover(types, rowtype TEXT, value ANY NOT NULL)"));
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT rowid, rowtype, value FROM recover_types"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ("INTEGER", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kInteger, statement.GetColumnType(2));
  EXPECT_EQ(17, statement.ColumnInt(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ("FLOAT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kFloat, statement.GetColumnType(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ("TEXT", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kText, statement.GetColumnType(2));
  EXPECT_EQ("This is text", statement.ColumnString(2));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ("BLOB", statement.ColumnString(1));
  EXPECT_EQ(sql::ColumnType::kBlob, statement.GetColumnType(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(2, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, RowidAlias) {
  GenerateTypesTable(&db_);

  // The id column is an alias for rowid, and its values get serialized as NULL.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE types2(id INTEGER PRIMARY KEY, rowtype TEXT, value)"));
  ASSERT_TRUE(
      db_.Execute("INSERT INTO types2(id, rowtype, value) "
                  "SELECT rowid, rowtype, value FROM types WHERE true"));
  ASSERT_TRUE(db_.Execute(
      "CREATE VIRTUAL TABLE temp.recover_types2 "
      "USING recover(types2, id ROWID NOT NULL, rowtype TEXT, value ANY)"));

  sql::Statement statement(
      db_.GetUniqueStatement("SELECT id, rowid, rowtype, value FROM types2"));

  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(1, statement.ColumnInt(0));
  EXPECT_EQ(1, statement.ColumnInt(1));
  EXPECT_EQ("NULL", statement.ColumnString(2));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(3));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(2, statement.ColumnInt(0));
  EXPECT_EQ(2, statement.ColumnInt(1));
  EXPECT_EQ("INTEGER", statement.ColumnString(2));
  EXPECT_EQ(17, statement.ColumnInt(3));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(3, statement.ColumnInt(0));
  EXPECT_EQ(3, statement.ColumnInt(1));
  EXPECT_EQ("FLOAT", statement.ColumnString(2));
  EXPECT_DOUBLE_EQ(3.1415927, statement.ColumnDouble(3));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(4, statement.ColumnInt(0));
  EXPECT_EQ(4, statement.ColumnInt(1));
  EXPECT_EQ("TEXT", statement.ColumnString(2));
  EXPECT_EQ("This is text", statement.ColumnString(3));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(5, statement.ColumnInt(0));
  EXPECT_EQ(5, statement.ColumnInt(1));
  EXPECT_EQ("BLOB", statement.ColumnString(2));
  std::string blob_text;
  ASSERT_TRUE(statement.ColumnBlobAsString(3, &blob_text));
  EXPECT_EQ("This is a blob", blob_text);

  EXPECT_FALSE(statement.Step());
}

TEST_F(RecoverModuleTest, IntegerEncodings) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE integers(value)"));

  const std::vector<int64_t> values = {
      // Encoded directly in type info.
      0,
      1,
      // 8-bit signed.
      2,
      -2,
      127,
      -128,
      // 16-bit signed.
      12345,
      -12345,
      32767,
      -32768,
      // 24-bit signed.
      1234567,
      -1234567,
      8388607,
      -8388608,
      // 32-bit signed.
      1234567890,
      -1234567890,
      2147483647,
      -2147483647,
      // 48-bit signed.
      123456789012345,
      -123456789012345,
      140737488355327,
      -140737488355327,
      // 64-bit signed.
      1234567890123456789,
      -1234567890123456789,
      9223372036854775807,
      -9223372036854775807,
  };
  sql::Statement insert(
      db_.GetUniqueStatement("INSERT INTO integers VALUES(?)"));
  for (int64_t value : values) {
    insert.BindInt64(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/* clear_bound_vars= */ true);
  }

  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_integers "
                  "USING recover(integers, value INTEGER)"));
  sql::Statement select(
      db_.GetUniqueStatement("SELECT rowid, value FROM recover_integers"));
  for (size_t i = 0; i < values.size(); ++i) {
    ASSERT_TRUE(select.Step()) << "Was attemping to read " << values[i];
    EXPECT_EQ(static_cast<int>(i + 1), select.ColumnInt(0));
    EXPECT_EQ(values[i], select.ColumnInt64(1));
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(RecoverModuleTest, VarintEncodings) {
  const std::vector<int64_t> values = {
      // 1-byte varints.
      0x00, 0x01, 0x02, 0x7e, 0x7f,
      // 2-byte varints
      0x80, 0x81, 0xff, 0x0100, 0x0101, 0x1234, 0x1ffe, 0x1fff, 0x3ffe, 0x3fff,
      // 3-byte varints
      0x4000, 0x4001, 0x0ffffe, 0x0fffff, 0x123456, 0x1fedcb, 0x1ffffe,
      0x1fffff,
      // 4-byte varints
      0x200000, 0x200001, 0x123456, 0xfedcba, 0xfffffe, 0xffffff, 0x01234567,
      0x0fedcba9, 0x0ffffffe, 0x0fffffff,
      // 5-byte varints
      0x10000000, 0x10000001, 0x12345678, 0xfedcba98, 0x01'23456789,
      0x07'fffffffe, 0x07'ffffffff,
      // 6-byte varints
      0x08'00000000, 0x08'00000001, 0x12'3456789a, 0xfe'dcba9876,
      0x0123'456789ab, 0x03ff'fffffffe, 0x03ff'ffffffff,
      // 7-byte varints
      0x0400'00000000, 0x0400'00000001, 0xfedc'ba987654, 0x012345'6789abcd,
      0x01ffff'fffffffe, 0x01ffff'ffffffff,
      // 8-byte varints
      0x020000'00000000, 0x020000'00000001, 0x0fedcb'a9876543,
      0x123456'789abcde, 0xfedcba'98765432, 0xffffff'fffffffe,
      0xffffff'ffffffff,
      // 9-byte positive varints
      0x01000000'00000000, 0x01000000'00000001, 0x12345678'9abcdef0,
      0x7fedcba9'87654321, 0x7fffffff'fffffffe, 0x7fffffff'ffffffff,
      // 9-byte negative varints
      -0x01, -0x02, -0x7e, -0x7f, -0x80, -0x81, -0x12345678'9abcdef0,
      -0x7fedcba9'87654321, -0x7fffffff'ffffffff,
      -0x7fffffff'ffffffff - 1,  // -0x80000000'00000000 is not a valid literal
  };

  ASSERT_TRUE(db_.Execute("CREATE TABLE varints(value INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_varints "
                  "USING recover(varints, value ROWID)"));

  for (int64_t value : values) {
    sql::Statement insert(
        db_.GetUniqueStatement("INSERT INTO varints VALUES(?)"));
    insert.BindInt64(0, value);
    ASSERT_TRUE(insert.Run());

    sql::Statement select(
        db_.GetUniqueStatement("SELECT rowid, value FROM recover_varints"));
    ASSERT_TRUE(select.Step()) << "Was attemping to read " << value;
    EXPECT_EQ(value, select.ColumnInt64(0));
    EXPECT_EQ(value, select.ColumnInt64(1));
    EXPECT_FALSE(select.Step());

    ASSERT_TRUE(db_.Execute("DELETE FROM varints"));
  }
}

TEST_F(RecoverModuleTest, TextEncodings) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE encodings(t TEXT)"));

  const std::vector<std::string> values = {
      "",        "a",       "ö",       "Mjollnir", "Mjölnir", "Mjǫlnir",
      "Mjölner", "Mjølner", "ハンマー",
  };

  sql::Statement insert(
      db_.GetUniqueStatement("INSERT INTO encodings VALUES(?)"));
  for (const std::string& value : values) {
    insert.BindString(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/* clear_bound_vars= */ true);
  }

  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_encodings "
                  "USING recover(encodings, t TEXT)"));
  sql::Statement select(
      db_.GetUniqueStatement("SELECT rowid, t FROM recover_encodings"));
  for (size_t i = 0; i < values.size(); ++i) {
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(static_cast<int>(i + 1), select.ColumnInt(0));
    EXPECT_EQ(values[i], select.ColumnString(1));
  }
  EXPECT_FALSE(select.Step());
}

TEST_F(RecoverModuleTest, BlobEncodings) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE blob_encodings(t BLOB)"));

  const std::vector<std::vector<uint8_t>> values = {
      {},           {0x00},       {0x01},
      {0x42},       {0xff},       {0x00, 0x00},
      {0x00, 0x01}, {0x00, 0xff}, {0x42, 0x43, 0x44, 0x45, 0x46},
  };

  sql::Statement insert(
      db_.GetUniqueStatement("INSERT INTO blob_encodings VALUES(?)"));
  for (const std::vector<uint8_t>& value : values) {
    insert.BindBlob(0, value);
    ASSERT_TRUE(insert.Run());
    insert.Reset(/* clear_bound_vars= */ true);
  }

  ASSERT_TRUE(
      db_.Execute("CREATE VIRTUAL TABLE temp.recover_blob_encodings "
                  "USING recover(blob_encodings, t BLOB)"));
  sql::Statement select(
      db_.GetUniqueStatement("SELECT rowid, t FROM recover_blob_encodings"));
  for (size_t i = 0; i < values.size(); ++i) {
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(static_cast<int>(i + 1), select.ColumnInt(0));

    std::vector<uint8_t> column_value;
    EXPECT_TRUE(select.ColumnBlobAsVector(1, &column_value));
    EXPECT_EQ(values[i], column_value);
  }
  EXPECT_FALSE(select.Step());
}

namespace {

std::string RandomString(int size) {
  std::mt19937 rng;
  std::uniform_int_distribution<int> random_char(32, 127);
  std::string large_value;
  large_value.reserve(size);
  for (int i = 0; i < size; ++i)
    large_value.push_back(random_char(rng));
  return large_value;
}

void CheckLargeValueRecovery(sql::Database* db, int value_size) {
  const std::string large_value = RandomString(value_size);

  ASSERT_TRUE(db->Execute("CREATE TABLE overflow(t TEXT)"));
  sql::Statement insert(
      db->GetUniqueStatement("INSERT INTO overflow VALUES(?)"));
  insert.BindString(0, large_value);
  ASSERT_TRUE(insert.Run());

  ASSERT_TRUE(db->Execute("VACUUM"));

  ASSERT_TRUE(
      db->Execute("CREATE VIRTUAL TABLE temp.recover_overflow "
                  "USING recover(overflow, t TEXT)"));
  sql::Statement select(
      db->GetUniqueStatement("SELECT rowid, t FROM recover_overflow"));
  ASSERT_TRUE(select.Step());
  EXPECT_EQ(1, select.ColumnInt(0));
  EXPECT_EQ(large_value, select.ColumnString(1));
}

bool HasEnabledAutoVacuum(sql::Database* db) {
  sql::Statement pragma(db->GetUniqueStatement("PRAGMA auto_vacuum"));
  EXPECT_TRUE(pragma.Step());
  return pragma.ColumnInt(0) != 0;
}

// The overhead in the table page is:
// * 35 bytes - the cell size cutoff that causes a cell's payload to overflow
// * 1 byte - record header size
// * 2-3 bytes - type ID for the text column
//   - texts of 58-8185 bytes use 2 bytes
//   - texts of 8186-1048569 bytes  use 3 bytes
//
// The overhead below assumes a 2-byte string type ID.
constexpr int kRecordOverhead = 38;

// Each overflow page uses 4 bytes to store the pointer to the next page.
constexpr int kOverflowOverhead = 4;

}  // namespace

TEST_F(RecoverModuleTest, ValueWithoutOverflow) {
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(2 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page and a leaf page";
}

TEST_F(RecoverModuleTest, ValueWithOneByteOverflow) {
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead + 1);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(3 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 1 overflow page";
}

TEST_F(RecoverModuleTest, ValueWithOneOverflowPage) {
  CheckLargeValueRecovery(
      &db_, db_.page_size() - kRecordOverhead + db_.page_size() / 2);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(3 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 1 overflow page";
}

TEST_F(RecoverModuleTest, ValueWithOneFullOverflowPage) {
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead +
                                    db_.page_size() - kOverflowOverhead);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(3 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 1 overflow page";
}

TEST_F(RecoverModuleTest, ValueWithOneByteSecondOverflowPage) {
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead +
                                    db_.page_size() - kOverflowOverhead + 1);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(4 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 2 overflow pages";
}

TEST_F(RecoverModuleTest, ValueWithTwoOverflowPages) {
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead +
                                    db_.page_size() - kOverflowOverhead +
                                    db_.page_size() / 2);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(4 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 2 overflow pages";
}

TEST_F(RecoverModuleTest, ValueWithTwoFullOverflowPages) {
  // This value is large enough that the varint encoding of its type ID takes up
  // 3 bytes, instead of 2.
  CheckLargeValueRecovery(&db_, db_.page_size() - kRecordOverhead +
                                    (db_.page_size() - kOverflowOverhead) * 2 -
                                    1);
  int auto_vacuum_pages = HasEnabledAutoVacuum(&db_) ? 1 : 0;
  ASSERT_EQ(4 + auto_vacuum_pages, sql::test::GetPageCount(&db_))
      << "Database should have a root page, a leaf page, and 2 overflow pages";
}

}  // namespace recover
}  // namespace sql
