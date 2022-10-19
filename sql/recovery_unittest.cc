// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recovery.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/paths.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

using sql::test::ExecuteWithResult;
using sql::test::ExecuteWithResults;

// Dump consistent human-readable representation of the database
// schema.  For tables or indices, this will contain the sql command
// to create the table or index.  For certain automatic SQLite
// structures with no sql, the name is used.
std::string GetSchema(Database* db) {
  static const char kSql[] =
      "SELECT COALESCE(sql, name) FROM sqlite_schema ORDER BY 1";
  return ExecuteWithResults(db, kSql, "|", "\n");
}

class SQLRecoveryTest : public testing::Test {
 public:
  ~SQLRecoveryTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("recovery_test.sqlite");
    ASSERT_TRUE(db_.Open(db_path_));
  }

  bool Reopen() {
    db_.Close();
    return db_.Open(db_path_);
  }

  bool OverwriteDatabaseHeader() {
    base::File file(db_path_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    static constexpr char kText[] = "Now is the winter of our discontent.";
    constexpr int kTextBytes = sizeof(kText) - 1;
    return file.Write(0, kText, kTextBytes) == kTextBytes;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  Database db_;
};

// Baseline Recovery test covering the different ways to dispose of the
// scoped pointer received from Recovery::Begin().
TEST_F(SQLRecoveryTest, RecoverBasic) {
  static const char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  static const char kInsertSql[] = "INSERT INTO x VALUES ('This is a test')";
  static const char kAltInsertSql[] =
      "INSERT INTO x VALUES ('That was a test')";
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  // If the Recovery handle goes out of scope without being
  // Recovered(), the database is razed.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());
  }
  EXPECT_FALSE(db_.is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db_.is_open());
  ASSERT_EQ("", GetSchema(&db_));

  // Recreate the database.
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  // Unrecoverable() also razes.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());
    Recovery::Unrecoverable(std::move(recovery));

    // TODO(shess): Test that calls to recover.db_ start failing.
  }
  EXPECT_FALSE(db_.is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db_.is_open());
  ASSERT_EQ("", GetSchema(&db_));

  // Attempting to recover a previously-recovered handle fails early.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());
    recovery.reset();

    recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_FALSE(recovery.get());
  }
  ASSERT_TRUE(Reopen());

  // Recreate the database.
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute(kInsertSql));
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  // Unrecovered table to distinguish from recovered database.
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c INTEGER)"));
  ASSERT_NE("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  // Recovered() replaces the original with the "recovered" version.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());

    // Create the new version of the table.
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // Insert different data to distinguish from original database.
    ASSERT_TRUE(recovery->db()->Execute(kAltInsertSql));

    // Successfully recovered.
    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }
  EXPECT_FALSE(db_.is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db_.is_open());
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  const char* kXSql = "SELECT * FROM x ORDER BY 1";
  ASSERT_EQ("That was a test", ExecuteWithResult(&db_, kXSql));

  // Reset the database contents.
  ASSERT_TRUE(db_.Execute("DELETE FROM x"));
  ASSERT_TRUE(db_.Execute(kInsertSql));

  // Rollback() discards recovery progress and leaves the database as it was.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());

    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));
    ASSERT_TRUE(recovery->db()->Execute(kAltInsertSql));

    Recovery::Rollback(std::move(recovery));
  }
  EXPECT_FALSE(db_.is_open());
  ASSERT_TRUE(Reopen());
  EXPECT_TRUE(db_.is_open());
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  ASSERT_EQ("This is a test", ExecuteWithResult(&db_, kXSql));
}

// Test operation of the virtual table used by Recovery.
TEST_F(SQLRecoveryTest, VirtualTable) {
  static const char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('This is a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('That was a test')"));

  // Successfully recover the database.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);

    // Tables to recover original DB, now at [corrupt].
    static const char kRecoveryCreateSql[] =
        "CREATE VIRTUAL TABLE temp.recover_x using recover("
        "  corrupt.x,"
        "  t TEXT STRICT"
        ")";
    ASSERT_TRUE(recovery->db()->Execute(kRecoveryCreateSql));

    // Re-create the original schema.
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // Copy the data from the recovery tables to the new database.
    static const char kRecoveryCopySql[] =
        "INSERT INTO x SELECT t FROM recover_x";
    ASSERT_TRUE(recovery->db()->Execute(kRecoveryCopySql));

    // Successfully recovered.
    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ("CREATE TABLE x (t TEXT)", GetSchema(&db_));

  static const char* kXSql = "SELECT * FROM x ORDER BY 1";
  ASSERT_EQ("That was a test\nThis is a test",
            ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Our corruption handling assumes that a corrupt index doesn't impact
// SQL statements that only operate on the associated table. This test verifies
// the assumption.
TEST_F(SQLRecoveryTest, TableIndependentFromCorruptIndex) {
  static const char kCreateTable[] =
      "CREATE TABLE rows(indexed INTEGER NOT NULL, unindexed INTEGER NOT NULL)";
  ASSERT_TRUE(db_.Execute(kCreateTable));
  ASSERT_TRUE(db_.Execute("CREATE UNIQUE INDEX rows_index ON rows(indexed)"));

  // Populate the table with powers of two. These numbers make it easy to see if
  // SUM() missed a row.
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(1, 1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(2, 2)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(4, 4)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(8, 8)"));

  // SQL statement that performs a table scan. SUM(unindexed) heavily nudges
  // SQLite to use the table instead of the index.
  static const char kUnindexedCountSql[] = "SELECT SUM(unindexed) FROM rows";
  EXPECT_EQ("15", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "No SQL statement should fail before corruption";

  // SQL statement that performs an index scan.
  static const char kIndexedCountSql[] =
      "SELECT SUM(indexed) FROM rows INDEXED BY rows_index";
  EXPECT_EQ("15", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Table scan should not fail due to corrupt index";

  db_.Close();
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path_, "rows_index"));
  ASSERT_TRUE(Reopen());

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(db_.Execute(kIndexedCountSql))
        << "Index scan on corrupt index should fail";
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Index scan on corrupt index should fail";
  }

  EXPECT_EQ("15", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "Table scan should not fail due to corrupt index";
}

TEST_F(SQLRecoveryTest, RecoverCorruptIndex) {
  static const char kCreateTable[] =
      "CREATE TABLE rows(indexed INTEGER NOT NULL, unindexed INTEGER NOT NULL)";
  ASSERT_TRUE(db_.Execute(kCreateTable));

  static const char kCreateIndex[] =
      "CREATE UNIQUE INDEX rows_index ON rows(indexed)";
  ASSERT_TRUE(db_.Execute(kCreateIndex));

  // Populate the table with powers of two. These numbers make it easy to see if
  // SUM() missed a row.
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(1, 1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(2, 2)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(4, 4)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(8, 8)"));

  db_.Close();
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path_, "rows_index"));
  ASSERT_TRUE(Reopen());

  int error = SQLITE_OK;
  db_.set_error_callback(
      base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
        error = sqlite_error;

        // Recovery::Begin() does not support a pre-existing error callback.
        db_.reset_error_callback();
        std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
        ASSERT_TRUE(recovery.get());

        ASSERT_TRUE(recovery->db()->Execute(kCreateTable));
        ASSERT_TRUE(recovery->db()->Execute(kCreateIndex));

        size_t rows = 0;
        ASSERT_TRUE(recovery->AutoRecoverTable("rows", &rows));
        ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
      }));

  // SUM(unindexed) heavily nudges SQLite to use the table instead of the index.
  static const char kUnindexedCountSql[] = "SELECT SUM(unindexed) FROM rows";
  EXPECT_EQ("15", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "Table scan should not fail due to corrupt index";
  EXPECT_EQ(SQLITE_OK, error)
      << "Successful statement execution should not invoke the error callback";

  static const char kIndexedCountSql[] =
      "SELECT SUM(indexed) FROM rows INDEXED BY rows_index";
  EXPECT_EQ("", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Index scan on corrupt index should fail";
  EXPECT_EQ(SQLITE_CORRUPT, error)
      << "Error callback should be called during scan on corrupt index";

  EXPECT_EQ("", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "Table scan should not succeed anymore on a poisoned database";

  ASSERT_TRUE(Reopen());

  // The recovered table has consistency between the index and the table.
  EXPECT_EQ("15", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "Table should survive database recovery";
  EXPECT_EQ("15", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Index should be reconstructed during database recovery";
}

TEST_F(SQLRecoveryTest, RecoverCorruptTable) {
  // The `filler` column is used to cause a record to overflow multiple pages.
  static const char kCreateTable[] =
      // clang-format off
      "CREATE TABLE rows(indexed INTEGER NOT NULL, unindexed INTEGER NOT NULL,"
      "filler BLOB NOT NULL)";
  // clang-format on
  ASSERT_TRUE(db_.Execute(kCreateTable));

  static const char kCreateIndex[] =
      "CREATE UNIQUE INDEX rows_index ON rows(indexed)";
  ASSERT_TRUE(db_.Execute(kCreateIndex));

  // Populate the table with powers of two. These numbers make it easy to see if
  // SUM() missed a row.
  ASSERT_TRUE(db_.Execute(
      "INSERT INTO rows(indexed, unindexed, filler) VALUES(1, 1, x'31')"));
  ASSERT_TRUE(db_.Execute(
      "INSERT INTO rows(indexed, unindexed, filler) VALUES(2, 2, x'32')"));
  ASSERT_TRUE(db_.Execute(
      "INSERT INTO rows(indexed, unindexed, filler) VALUES(4, 4, x'34')"));

  constexpr int kDbPageSize = 4096;
  {
    // Insert a record that will overflow the page.
    std::vector<uint8_t> large_buffer;
    ASSERT_EQ(db_.page_size(), kDbPageSize)
        << "Page overflow relies on specific size";
    large_buffer.resize(kDbPageSize * 2);
    base::ranges::fill(large_buffer, '8');
    sql::Statement insert(db_.GetUniqueStatement(
        "INSERT INTO rows(indexed,unindexed,filler) VALUES(8,8,?)"));
    insert.BindBlob(0, large_buffer);
    ASSERT_TRUE(insert.Run());
  }

  db_.Close();
  {
    // Zero out the last page of the database. This should be the overflow page
    // allocated for the last inserted row. So, deleting it should corrupt the
    // rows table.
    base::File db_file(db_path_, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                     base::File::FLAG_WRITE);
    ASSERT_TRUE(db_file.IsValid());
    int64_t db_size = db_file.GetLength();
    ASSERT_GT(db_size, kDbPageSize)
        << "The database should have multiple pages";
    ASSERT_TRUE(db_file.SetLength(db_size - kDbPageSize));
  }

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_TRUE(Reopen());
    EXPECT_TRUE(expecter.SawExpectedErrors());
    // PRAGMAs executed inside Database::Open() will error out.
  }

  int error = SQLITE_OK;
  db_.set_error_callback(
      base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
        error = sqlite_error;

        // Recovery::Begin() does not support a pre-existing error callback.
        db_.reset_error_callback();
        std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
        ASSERT_TRUE(recovery.get());

        ASSERT_TRUE(recovery->db()->Execute(kCreateTable));
        ASSERT_TRUE(recovery->db()->Execute(kCreateIndex));

        size_t rows = 0;
        ASSERT_TRUE(recovery->AutoRecoverTable("rows", &rows));
        ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
      }));

  // SUM(unindexed) heavily nudges SQLite to use the table instead of the index.
  static const char kUnindexedCountSql[] = "SELECT SUM(unindexed) FROM rows";
  EXPECT_FALSE(db_.Execute(kUnindexedCountSql))
      << "Table scan on corrupt table should fail";
  EXPECT_EQ(SQLITE_CORRUPT, error)
      << "Error callback should be called during scan on corrupt index";

  ASSERT_TRUE(Reopen());

  // All rows should be recovered. Only the BLOB in the last row was damaged.
  EXPECT_EQ("15", ExecuteWithResult(&db_, kUnindexedCountSql))
      << "Table should survive database recovery";
  static const char kIndexedCountSql[] =
      "SELECT SUM(indexed) FROM rows INDEXED BY rows_index";
  EXPECT_EQ("15", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Index should be reconstructed during database recovery";
}

TEST_F(SQLRecoveryTest, Meta) {
  const int kVersion = 3;
  const int kCompatibleVersion = 2;

  {
    MetaTable meta;
    EXPECT_TRUE(meta.Init(&db_, kVersion, kCompatibleVersion));
    EXPECT_EQ(kVersion, meta.GetVersionNumber());
  }

  // Test expected case where everything works.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    EXPECT_TRUE(recovery->SetupMeta());
    int version = 0;
    EXPECT_TRUE(recovery->GetMetaVersionNumber(&version));
    EXPECT_EQ(kVersion, version);

    Recovery::Rollback(std::move(recovery));
  }
  ASSERT_TRUE(Reopen());  // Handle was poisoned.

  // Test version row missing.
  EXPECT_TRUE(db_.Execute("DELETE FROM meta WHERE key = 'version'"));
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    EXPECT_TRUE(recovery->SetupMeta());
    int version = 0;
    EXPECT_FALSE(recovery->GetMetaVersionNumber(&version));
    EXPECT_EQ(0, version);

    Recovery::Rollback(std::move(recovery));
  }
  ASSERT_TRUE(Reopen());  // Handle was poisoned.

  // Test meta table missing.
  EXPECT_TRUE(db_.Execute("DROP TABLE meta"));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);  // From virtual table.
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    EXPECT_FALSE(recovery->SetupMeta());
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

// Baseline AutoRecoverTable() test.
TEST_F(SQLRecoveryTest, AutoRecoverTable) {
  // BIGINT and VARCHAR to test type affinity.
  static const char kCreateSql[] =
      "CREATE TABLE x (id BIGINT, t TEXT, v VARCHAR)";
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (11, 'This is', 'a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (5, 'That was', 'a test')"));

  // Save aside a copy of the original schema and data.
  const std::string orig_schema(GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  const std::string orig_data(ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Create a lame-duck table which will not be propagated by recovery to
  // detect that the recovery code actually ran.
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c TEXT)"));
  ASSERT_NE(orig_schema, GetSchema(&db_));

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // Save a copy of the temp db's schema before recovering the table.
    static const char kTempSchemaSql[] =
        "SELECT name, sql FROM sqlite_temp_schema";
    const std::string temp_schema(
        ExecuteWithResults(recovery->db(), kTempSchemaSql, "|", "\n"));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(2u, rows);

    // Test that any additional temp tables were cleaned up.
    EXPECT_EQ(temp_schema,
              ExecuteWithResults(recovery->db(), kTempSchemaSql, "|", "\n"));

    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(orig_schema, GetSchema(&db_));
  ASSERT_EQ(orig_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Recovery fails if the target table doesn't exist.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    // TODO(shess): Should this failure implicitly lead to Raze()?
    size_t rows = 0;
    EXPECT_FALSE(recovery->AutoRecoverTable("y", &rows));

    Recovery::Unrecoverable(std::move(recovery));
  }
}

// Test that default values correctly replace nulls.  The recovery
// virtual table reads directly from the database, so DEFAULT is not
// interpretted at that level.
TEST_F(SQLRecoveryTest, AutoRecoverTableWithDefault) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE x (id INTEGER)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (5)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (15)"));

  // ALTER effectively leaves the new columns NULL in the first two
  // rows.  The row with 17 will get the default injected at insert
  // time, while the row with 42 will get the actual value provided.
  // Embedded "'" to make sure default-handling continues to be quoted
  // correctly.
  ASSERT_TRUE(db_.Execute("ALTER TABLE x ADD COLUMN t TEXT DEFAULT 'a''a'"));
  ASSERT_TRUE(db_.Execute("ALTER TABLE x ADD COLUMN b BLOB DEFAULT x'AA55'"));
  ASSERT_TRUE(db_.Execute("ALTER TABLE x ADD COLUMN i INT DEFAULT 93"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x (id) VALUES (17)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (42, 'b', x'1234', 12)"));

  // Save aside a copy of the original schema and data.
  const std::string orig_schema(GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  const std::string orig_data(ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Create a lame-duck table which will not be propagated by recovery to
  // detect that the recovery code actually ran.
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c TEXT)"));
  ASSERT_NE(orig_schema, GetSchema(&db_));

  // Mechanically adjust the stored schema and data to allow detecting
  // where the default value is coming from.  The target table is just
  // like the original with the default for [t] changed, to signal
  // defaults coming from the recovery system.  The two %5 rows should
  // get the target-table default for [t], while the others should get
  // the source-table default.
  std::string final_schema(orig_schema);
  std::string final_data(orig_data);
  size_t pos;
  while ((pos = final_schema.find("'a''a'")) != std::string::npos) {
    final_schema.replace(pos, 6, "'c''c'");
  }
  while ((pos = final_data.find("5|a'a")) != std::string::npos) {
    final_data.replace(pos, 5, "5|c'c");
  }

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    // Different default to detect which table provides the default.
    ASSERT_TRUE(recovery->db()->Execute(final_schema.c_str()));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(4u, rows);

    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(final_schema, GetSchema(&db_));
  ASSERT_EQ(final_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Test that rows with NULL in a NOT NULL column are filtered
// correctly.  In the wild, this would probably happen due to
// corruption, but here it is simulated by recovering a table which
// allowed nulls into a table which does not.
TEST_F(SQLRecoveryTest, AutoRecoverTableNullFilter) {
  static const char kOrigSchema[] = "CREATE TABLE x (id INTEGER, t TEXT)";
  static const char kFinalSchema[] =
      "CREATE TABLE x (id INTEGER, t TEXT NOT NULL)";

  ASSERT_TRUE(db_.Execute(kOrigSchema));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (5, NULL)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (15, 'this is a test')"));

  // Create a lame-duck table which will not be propagated by recovery to
  // detect that the recovery code actually ran.
  ASSERT_EQ(kOrigSchema, GetSchema(&db_));
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c TEXT)"));
  ASSERT_NE(kOrigSchema, GetSchema(&db_));

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kFinalSchema));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(1u, rows);

    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // The schema should be the same, but only one row of data should
  // have been recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(kFinalSchema, GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  ASSERT_EQ("15|this is a test", ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Test AutoRecoverTable with a ROWID alias.
TEST_F(SQLRecoveryTest, AutoRecoverTableWithRowid) {
  // The rowid alias is almost always the first column, intentionally
  // put it later.
  static const char kCreateSql[] =
      "CREATE TABLE x (t TEXT, id INTEGER PRIMARY KEY NOT NULL)";
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('This is a test', NULL)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('That was a test', NULL)"));

  // Save aside a copy of the original schema and data.
  const std::string orig_schema(GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  const std::string orig_data(ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Create a lame-duck table which will not be propagated by recovery to
  // detect that the recovery code actually ran.
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c TEXT)"));
  ASSERT_NE(orig_schema, GetSchema(&db_));

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(2u, rows);

    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(orig_schema, GetSchema(&db_));
  ASSERT_EQ(orig_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Test that a compound primary key doesn't fire the ROWID code.
TEST_F(SQLRecoveryTest, AutoRecoverTableWithCompoundKey) {
  static const char kCreateSql[] =
      "CREATE TABLE x ("
      "id INTEGER NOT NULL,"
      "id2 TEXT NOT NULL,"
      "t TEXT,"
      "PRIMARY KEY (id, id2)"
      ")";
  ASSERT_TRUE(db_.Execute(kCreateSql));

  // NOTE(shess): Do not accidentally use [id] 1, 2, 3, as those will
  // be the ROWID values.
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (1, 'a', 'This is a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (1, 'b', 'That was a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (2, 'a', 'Another test')"));

  // Save aside a copy of the original schema and data.
  const std::string orig_schema(GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  const std::string orig_data(ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Create a lame-duck table which will not be propagated by recovery to
  // detect that the recovery code actually ran.
  ASSERT_TRUE(db_.Execute("CREATE TABLE y (c TEXT)"));
  ASSERT_NE(orig_schema, GetSchema(&db_));

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(3u, rows);

    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(orig_schema, GetSchema(&db_));
  ASSERT_EQ(orig_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Test recovering from a table with fewer columns than the target.
TEST_F(SQLRecoveryTest, AutoRecoverTableMissingColumns) {
  static const char kCreateSql[] =
      "CREATE TABLE x (id INTEGER PRIMARY KEY, t0 TEXT)";
  static const char kAlterSql[] =
      "ALTER TABLE x ADD COLUMN t1 TEXT DEFAULT 't'";
  ASSERT_TRUE(db_.Execute(kCreateSql));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (1, 'This is')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES (2, 'That was')"));

  // Generate the expected info by faking a table to match what recovery will
  // create.
  const std::string orig_schema(GetSchema(&db_));
  static const char kXSql[] = "SELECT * FROM x ORDER BY 1";
  std::string expected_schema;
  std::string expected_data;
  {
    ASSERT_TRUE(db_.BeginTransaction());
    ASSERT_TRUE(db_.Execute(kAlterSql));

    expected_schema = GetSchema(&db_);
    expected_data = ExecuteWithResults(&db_, kXSql, "|", "\n");

    db_.RollbackTransaction();
  }

  // Following tests are pointless if the rollback didn't work.
  ASSERT_EQ(orig_schema, GetSchema(&db_));

  // Recover the previous version of the table into the altered version.
  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));
    ASSERT_TRUE(recovery->db()->Execute(kAlterSql));
    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(2u, rows);
    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(expected_schema, GetSchema(&db_));
  ASSERT_EQ(expected_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Recover a golden file where an interior page has been manually modified so
// that the number of cells is greater than will fit on a single page.  This
// case happened in <http://crbug.com/387868>.
TEST_F(SQLRecoveryTest, Bug387868) {
  base::FilePath golden_path;
  ASSERT_TRUE(base::PathService::Get(sql::test::DIR_TEST_DATA, &golden_path));
  golden_path = golden_path.AppendASCII("recovery_387868");
  db_.Close();
  ASSERT_TRUE(base::CopyFile(golden_path, db_path_));
  ASSERT_TRUE(Reopen());

  {
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    ASSERT_TRUE(recovery.get());

    // Create the new version of the table.
    static const char kCreateSql[] =
        "CREATE TABLE x (id INTEGER PRIMARY KEY, t0 TEXT)";
    ASSERT_TRUE(recovery->db()->Execute(kCreateSql));

    size_t rows = 0;
    EXPECT_TRUE(recovery->AutoRecoverTable("x", &rows));
    EXPECT_EQ(43u, rows);

    // Successfully recovered.
    EXPECT_TRUE(Recovery::Recovered(std::move(recovery)));
  }
}

// Memory-mapped I/O interacts poorly with I/O errors.  Make sure the recovery
// database doesn't accidentally enable it.
TEST_F(SQLRecoveryTest, NoMmap) {
  std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
  ASSERT_TRUE(recovery.get());

  // In the current implementation, the PRAGMA successfully runs with no result
  // rows.  Running with a single result of |0| is also acceptable.
  Statement s(recovery->db()->GetUniqueStatement("PRAGMA mmap_size"));
  EXPECT_TRUE(!s.Step() || !s.ColumnInt64(0));
}

TEST_F(SQLRecoveryTest, RecoverDatabase) {
  // As a side effect, AUTOINCREMENT creates the sqlite_sequence table for
  // RecoverDatabase() to handle.
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE table1(id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT)"));
  EXPECT_TRUE(db_.Execute("INSERT INTO table1(value) VALUES('turtle')"));
  EXPECT_TRUE(db_.Execute("INSERT INTO table1(value) VALUES('truck')"));
  EXPECT_TRUE(db_.Execute("INSERT INTO table1(value) VALUES('trailer')"));

  // This table needs index and a unique index to work.
  ASSERT_TRUE(db_.Execute("CREATE TABLE table2(name TEXT, value TEXT)"));
  ASSERT_TRUE(db_.Execute("CREATE UNIQUE INDEX table2_name ON table2(name)"));
  ASSERT_TRUE(db_.Execute("CREATE INDEX table2_value ON table2(value)"));
  EXPECT_TRUE(db_.Execute(
      "INSERT INTO table2(name, value) VALUES('jim', 'telephone')"));
  EXPECT_TRUE(
      db_.Execute("INSERT INTO table2(name, value) VALUES('bob', 'truck')"));
  EXPECT_TRUE(
      db_.Execute("INSERT INTO table2(name, value) VALUES('dean', 'trailer')"));

  // Save aside a copy of the original schema, verifying that it has the created
  // items plus the sqlite_sequence table.
  const std::string original_schema = GetSchema(&db_);
  ASSERT_EQ(4, base::ranges::count(original_schema, '\n')) << original_schema;

  static constexpr char kTable1Sql[] = "SELECT * FROM table1 ORDER BY 1";
  static constexpr char kTable2Sql[] = "SELECT * FROM table2 ORDER BY 1";
  EXPECT_EQ("1|turtle\n2|truck\n3|trailer",
            ExecuteWithResults(&db_, kTable1Sql, "|", "\n"));
  EXPECT_EQ("bob|truck\ndean|trailer\njim|telephone",
            ExecuteWithResults(&db_, kTable2Sql, "|", "\n"));

  // Database handle is valid before recovery, poisoned after.
  static constexpr char kTrivialSql[] = "SELECT COUNT(*) FROM sqlite_schema";
  EXPECT_TRUE(db_.IsSQLValid(kTrivialSql));
  Recovery::RecoverDatabase(&db_, db_path_);
  EXPECT_FALSE(db_.IsSQLValid(kTrivialSql));

  // Since the database was not corrupt, the entire schema and all data should
  // be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(original_schema, GetSchema(&db_));
  EXPECT_EQ("1|turtle\n2|truck\n3|trailer",
            ExecuteWithResults(&db_, kTable1Sql, "|", "\n"));
  EXPECT_EQ("bob|truck\ndean|trailer\njim|telephone",
            ExecuteWithResults(&db_, kTable2Sql, "|", "\n"));
}

TEST_F(SQLRecoveryTest, RecoverDatabaseWithView) {
  db_.Close();
  sql::Database db({.enable_views_discouraged = true});
  ASSERT_TRUE(db.Open(db_path_));

  ASSERT_TRUE(db.Execute(
      "CREATE TABLE table1(id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT)"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('turtle')"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('truck')"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('trailer')"));

  ASSERT_TRUE(db.Execute("CREATE TABLE table2(name TEXT, value TEXT)"));
  ASSERT_TRUE(db.Execute("CREATE UNIQUE INDEX table2_name ON table2(name)"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('jim', 'telephone')"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('bob', 'truck')"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('dean', 'trailer')"));

  // View which is the intersection of [table1.value] and [table2.value].
  ASSERT_TRUE(db.Execute(
      "CREATE VIEW view_table12 AS SELECT table1.value FROM table1, table2 "
      "WHERE table1.value = table2.value"));

  static constexpr char kViewSql[] = "SELECT * FROM view_table12 ORDER BY 1";
  EXPECT_EQ("trailer\ntruck", ExecuteWithResults(&db, kViewSql, "|", "\n"));

  // Save aside a copy of the original schema, verifying that it has the created
  // items plus the sqlite_sequence table.
  const std::string original_schema = GetSchema(&db);
  ASSERT_EQ(4, base::ranges::count(original_schema, '\n')) << original_schema;

  // Database handle is valid before recovery, poisoned after.
  static constexpr char kTrivialSql[] = "SELECT COUNT(*) FROM sqlite_schema";
  EXPECT_TRUE(db.IsSQLValid(kTrivialSql));
  Recovery::RecoverDatabase(&db, db_path_);
  EXPECT_FALSE(db.IsSQLValid(kTrivialSql));

  // Since the database was not corrupt, the entire schema and all data should
  // be recovered.
  db.Close();
  ASSERT_TRUE(db.Open(db_path_));
  EXPECT_EQ("trailer\ntruck", ExecuteWithResults(&db, kViewSql, "|", "\n"));
}

// When RecoverDatabase() encounters SQLITE_NOTADB, the database is deleted.
TEST_F(SQLRecoveryTest, RecoverDatabaseDelete) {
  // Create a valid database, then write junk over the header.  This should lead
  // to SQLITE_NOTADB, which will cause ATTACH to fail.
  ASSERT_TRUE(db_.Execute("CREATE TABLE x (t TEXT)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('This is a test')"));
  db_.Close();
  ASSERT_TRUE(OverwriteDatabaseHeader());

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_NOTADB);

    // Reopen() here because it will see SQLITE_NOTADB.
    ASSERT_TRUE(Reopen());

    // This should "recover" the database by making it valid, but empty.
    Recovery::RecoverDatabase(&db_, db_path_);

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Recovery poisoned the handle, must re-open.
  db_.Close();
  ASSERT_TRUE(Reopen());

  EXPECT_EQ("", GetSchema(&db_));
}

// Allow callers to validate the database between recovery and commit.
TEST_F(SQLRecoveryTest, BeginRecoverDatabase) {
  static const char kCreateTable[] =
      "CREATE TABLE rows(indexed INTEGER NOT NULL, unindexed INTEGER NOT NULL)";
  ASSERT_TRUE(db_.Execute(kCreateTable));

  ASSERT_TRUE(db_.Execute("CREATE UNIQUE INDEX rows_index ON rows(indexed)"));

  // Populate the table with powers of two. These numbers make it easy to see if
  // SUM() missed a row.
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(1, 1)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(2, 2)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(4, 4)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO rows(indexed, unindexed) VALUES(8, 8)"));

  db_.Close();
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path_, "rows_index"));
  ASSERT_TRUE(Reopen());

  // Run recovery code, then rollback.  Database remains the same.
  {
    std::unique_ptr<Recovery> recovery =
        Recovery::BeginRecoverDatabase(&db_, db_path_);
    ASSERT_TRUE(recovery);
    Recovery::Rollback(std::move(recovery));
  }
  db_.Close();
  ASSERT_TRUE(Reopen());

  static const char kIndexedCountSql[] =
      "SELECT SUM(indexed) FROM rows INDEXED BY rows_index";
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_EQ("", ExecuteWithResult(&db_, kIndexedCountSql))
        << "Index should still be corrupted after recovery rollback";
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Index should still be corrupted after recovery rollback";
  }

  // Run recovery code, then commit.  The index is recovered.
  {
    std::unique_ptr<Recovery> recovery =
        Recovery::BeginRecoverDatabase(&db_, db_path_);
    ASSERT_TRUE(recovery);
    ASSERT_TRUE(Recovery::Recovered(std::move(recovery)));
  }
  db_.Close();
  ASSERT_TRUE(Reopen());

  EXPECT_EQ("15", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Index should be reconstructed after database recovery";
}

TEST_F(SQLRecoveryTest, AttachFailure) {
  // Create a valid database, then write junk over the header.  This should lead
  // to SQLITE_NOTADB, which will cause ATTACH to fail.
  ASSERT_TRUE(db_.Execute("CREATE TABLE x (t TEXT)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO x VALUES ('This is a test')"));
  db_.Close();
  ASSERT_TRUE(OverwriteDatabaseHeader());

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_NOTADB);

    // Reopen() here because it will see SQLITE_NOTADB.
    ASSERT_TRUE(Reopen());

    // Begin() should fail.
    std::unique_ptr<Recovery> recovery = Recovery::Begin(&db_, db_path_);
    EXPECT_FALSE(recovery.get());

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

// Helper for SQLRecoveryTest.PageSize.  Creates a fresh db based on db_prefix,
// with the given initial page size, and verifies it against the expected size.
// Then changes to the final page size and recovers, verifying that the
// recovered database ends up with the expected final page size.
void TestPageSize(const base::FilePath& db_prefix,
                  int initial_page_size,
                  const std::string& expected_initial_page_size,
                  int final_page_size,
                  const std::string& expected_final_page_size) {
  static const char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  static const char kInsertSql1[] = "INSERT INTO x VALUES ('This is a test')";
  static const char kInsertSql2[] = "INSERT INTO x VALUES ('That was a test')";
  static const char kSelectSql[] = "SELECT * FROM x ORDER BY t";

  const base::FilePath db_path = db_prefix.InsertBeforeExtensionASCII(
      base::NumberToString(initial_page_size));
  Database::Delete(db_path);
  Database db({.page_size = initial_page_size});
  ASSERT_TRUE(db.Open(db_path));
  ASSERT_TRUE(db.Execute(kCreateSql));
  ASSERT_TRUE(db.Execute(kInsertSql1));
  ASSERT_TRUE(db.Execute(kInsertSql2));
  ASSERT_EQ(expected_initial_page_size,
            ExecuteWithResult(&db, "PRAGMA page_size"));
  db.Close();

  // Re-open the database while setting a new |options.page_size| in the object.
  Database recover_db({.page_size = final_page_size});
  ASSERT_TRUE(recover_db.Open(db_path));
  // Recovery will use the page size set in the database object, which may not
  // match the file's page size.
  Recovery::RecoverDatabase(&recover_db, db_path);

  // Recovery poisoned the handle, must re-open.
  recover_db.Close();

  // Make sure the page size is read from the file.
  Database recovered_db({.page_size = DatabaseOptions::kDefaultPageSize});
  ASSERT_TRUE(recovered_db.Open(db_path));
  ASSERT_EQ(expected_final_page_size,
            ExecuteWithResult(&recovered_db, "PRAGMA page_size"));
  EXPECT_EQ("That was a test\nThis is a test",
            ExecuteWithResults(&recovered_db, kSelectSql, "|", "\n"));
}

// Verify that Recovery maintains the page size, and the virtual table
// works with page sizes other than SQLite's default.  Also verify the case
// where the default page size has changed.
TEST_F(SQLRecoveryTest, PageSize) {
  const std::string default_page_size =
      ExecuteWithResult(&db_, "PRAGMA page_size");

  // Check the default page size first.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(
      db_path_, DatabaseOptions::kDefaultPageSize, default_page_size,
      DatabaseOptions::kDefaultPageSize, default_page_size));

  // Sync uses 32k pages.
  EXPECT_NO_FATAL_FAILURE(
      TestPageSize(db_path_, 32768, "32768", 32768, "32768"));

  // Many clients use 4k pages.  This is the SQLite default after 3.12.0.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 4096, "4096", 4096, "4096"));

  // 1k is the default page size before 3.12.0.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 1024, "1024", 1024, "1024"));

  // Databases with no page size specified should recover with the new default
  // page size.  2k has never been the default page size.
  ASSERT_NE("2048", default_page_size);
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 2048, "2048",
                                       DatabaseOptions::kDefaultPageSize,
                                       default_page_size));
}

}  // namespace

}  // namespace sql
