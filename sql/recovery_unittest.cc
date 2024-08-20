// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sql/recovery.h"

#include <stddef.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sql_features.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

using sql::test::ExecuteWithResult;
using sql::test::ExecuteWithResults;

constexpr char kRecoveryResultHistogramName[] = "Sql.Recovery.Result";
constexpr char kRecoveryResultCodeHistogramName[] = "Sql.Recovery.ResultCode";

// Dump consistent human-readable representation of the database
// schema.  For tables or indices, this will contain the sql command
// to create the table or index.  For certain automatic SQLite
// structures with no sql, the name is used.
std::string GetSchema(Database* db) {
  static const char kSql[] =
      "SELECT COALESCE(sql, name) FROM sqlite_schema ORDER BY 1";
  return ExecuteWithResults(db, kSql, "|", "\n");
}

// Parameterized to test with and without WAL mode enabled.
class SqlRecoveryTest : public testing::Test,
                        public testing::WithParamInterface<bool> {
 public:
  SqlRecoveryTest() : db_(DatabaseOptions{.wal_mode = ShouldEnableWal()}) {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kEnableWALModeByDefault, ShouldEnableWal()}});
  }

  bool ShouldEnableWal() { return GetParam(); }

  void SetUp() override {
    db_.set_histogram_tag("MyFeatureDatabase");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("recovery_test.sqlite");
    ASSERT_TRUE(db_.Open(db_path_));
  }

  void TearDown() override {
    if (db_.is_open()) {
      db_.Close();
    }
    // Ensure the database, along with any recovery files, are cleaned up.
    ASSERT_TRUE(base::DeleteFile(db_path_));
    ASSERT_TRUE(base::DeleteFile(db_path_.AddExtensionASCII(".backup")));
    ASSERT_TRUE(temp_dir_.Delete());
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  Database db_;
  base::HistogramTester histogram_tester_;
};

#if BUILDFLAG(IS_FUCHSIA)
// WAL + recovery is not supported on Fuchsia, so only test without WAL mode.
INSTANTIATE_TEST_SUITE_P(All, SqlRecoveryTest, testing::Values(false));
#else
INSTANTIATE_TEST_SUITE_P(All, SqlRecoveryTest, testing::Bool());
#endif

TEST_P(SqlRecoveryTest, ShouldAttemptRecovery) {
  // Attempt to recover from corruption.
  ASSERT_TRUE(Recovery::ShouldAttemptRecovery(&db_, SQLITE_CORRUPT));

  // Do not attempt to recover from transient errors.
  EXPECT_FALSE(Recovery::ShouldAttemptRecovery(&db_, SQLITE_BUSY));

  // Do not attempt to recover null databases.
  EXPECT_FALSE(Recovery::ShouldAttemptRecovery(nullptr, SQLITE_CORRUPT));

  // Do not attempt to recover closed databases.
  Database invalid_db;
  EXPECT_FALSE(Recovery::ShouldAttemptRecovery(&invalid_db, SQLITE_CORRUPT));

  // Do not attempt to recover in-memory databases.
  ASSERT_TRUE(invalid_db.OpenInMemory());
  EXPECT_FALSE(Recovery::ShouldAttemptRecovery(&invalid_db, SQLITE_CORRUPT));

  // Return true for databases which have an error callback set, even though
  // the error callback should be reset before recovery is attempted.
  db_.set_error_callback(base::DoNothing());
  EXPECT_TRUE(Recovery::ShouldAttemptRecovery(&db_, SQLITE_CORRUPT));
}

TEST_P(SqlRecoveryTest, RecoverCorruptIndex) {
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

        EXPECT_EQ(
            Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);
        histogram_tester_.ExpectUniqueSample(kRecoveryResultHistogramName,
                                             Recovery::Result::kSuccess,
                                             /*expected_bucket_count=*/1);
        histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                             SqliteLoggedResultCode::kNoError,
                                             /*expected_bucket_count=*/1);
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

TEST_P(SqlRecoveryTest, RecoverCorruptTable) {
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
    ASSERT_FALSE(Reopen());
    EXPECT_TRUE(expecter.SawExpectedErrors());
    // PRAGMAs executed inside Database::Open() will error out.
  }

  int error = SQLITE_OK;
  db_.set_error_callback(
      base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
        error = sqlite_error;

        // Recovery::Begin() does not support a pre-existing error callback.
        db_.reset_error_callback();

        EXPECT_EQ(
            Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);
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

TEST_P(SqlRecoveryTest, Meta) {
  const int kVersion = 3;
  const int kCompatibleVersion = 2;

  {
    MetaTable meta;
    EXPECT_TRUE(meta.Init(&db_, kVersion, kCompatibleVersion));
    EXPECT_EQ(kVersion, meta.GetVersionNumber());
  }

  // Test expected case where everything works.
  EXPECT_EQ(Recovery::RecoverDatabase(
                &db_, Recovery::Strategy::kRecoverWithMetaVersionOrRaze),
            SqliteResultCode::kOk);
  histogram_tester_.ExpectUniqueSample(kRecoveryResultHistogramName,
                                       Recovery::Result::kSuccess,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                       SqliteLoggedResultCode::kNoError,
                                       /*expected_bucket_count=*/1);

  ASSERT_TRUE(Reopen());  // Handle was poisoned.

  ASSERT_TRUE(db_.DoesTableExist("meta"));

  // Test version row missing.
  EXPECT_TRUE(db_.Execute("DELETE FROM meta WHERE key = 'version'"));

  EXPECT_EQ(Recovery::RecoverDatabase(
                &db_, Recovery::Strategy::kRecoverWithMetaVersionOrRaze),
            SqliteResultCode::kError);
  histogram_tester_.ExpectBucketCount(
      kRecoveryResultHistogramName,
      Recovery::Result::kFailedMetaTableVersionWasInvalid,
      /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                       SqliteLoggedResultCode::kNoError,
                                       /*expected_bucket_count=*/2);
  ASSERT_TRUE(Reopen());  // Handle was poisoned.

  // Test meta table missing.
  ASSERT_FALSE(db_.DoesTableExist("meta"));

  EXPECT_EQ(Recovery::RecoverDatabase(
                &db_, Recovery::Strategy::kRecoverWithMetaVersionOrRaze),
            SqliteResultCode::kError);
  histogram_tester_.ExpectBucketCount(
      kRecoveryResultHistogramName,
      Recovery::Result::kFailedMetaTableDoesNotExist,
      /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                       SqliteLoggedResultCode::kNoError,
                                       /*expected_bucket_count=*/3);
}

// Baseline AutoRecoverTable() test.
TEST_P(SqlRecoveryTest, AutoRecoverTable) {
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

  EXPECT_EQ(Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(orig_schema, GetSchema(&db_));
  ASSERT_EQ(orig_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));

  // Recovery succeeds silently, since there's nothing to do.
  EXPECT_EQ(Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);
}

// Test that default values correctly replace nulls.  The recovery
// virtual table reads directly from the database, so DEFAULT is not
// interpreted at that level.
TEST_P(SqlRecoveryTest, AutoRecoverTableWithDefault) {
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

  std::string final_schema(orig_schema);
  std::string final_data(orig_data);
  EXPECT_EQ(Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(final_schema, GetSchema(&db_));
  ASSERT_EQ(final_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

// Test AutoRecoverTable with a ROWID alias.
TEST_P(SqlRecoveryTest, AutoRecoverTableWithRowid) {
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

  EXPECT_EQ(Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);

  // Since the database was not corrupt, the entire schema and all
  // data should be recovered.
  ASSERT_TRUE(Reopen());
  ASSERT_EQ(orig_schema, GetSchema(&db_));
  ASSERT_EQ(orig_data, ExecuteWithResults(&db_, kXSql, "|", "\n"));
}

void TestRecoverDatabase(Database& db,
                         const base::FilePath& db_path,
                         bool with_meta,
                         base::OnceClosure run_recovery) {
  const int kVersion = 3;
  const int kCompatibleVersion = 2;

  if (with_meta) {
    MetaTable meta;
    EXPECT_TRUE(meta.Init(&db, kVersion, kCompatibleVersion));
    EXPECT_EQ(kVersion, meta.GetVersionNumber());
    EXPECT_EQ(kCompatibleVersion, meta.GetCompatibleVersionNumber());
  }

  // As a side effect, AUTOINCREMENT creates the sqlite_sequence table for
  // RecoverDatabase() to handle.
  ASSERT_TRUE(db.Execute(
      "CREATE TABLE table1(id INTEGER PRIMARY KEY AUTOINCREMENT, value TEXT)"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('turtle')"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('truck')"));
  EXPECT_TRUE(db.Execute("INSERT INTO table1(value) VALUES('trailer')"));

  // This table needs index and a unique index to work.
  ASSERT_TRUE(db.Execute("CREATE TABLE table2(name TEXT, value TEXT)"));
  ASSERT_TRUE(db.Execute("CREATE UNIQUE INDEX table2_name ON table2(name)"));
  ASSERT_TRUE(db.Execute("CREATE INDEX table2_value ON table2(value)"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('jim', 'telephone')"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('bob', 'truck')"));
  EXPECT_TRUE(
      db.Execute("INSERT INTO table2(name, value) VALUES('dean', 'trailer')"));

  // Save aside a copy of the original schema, verifying that it has the created
  // items plus the sqlite_sequence table.
  const std::string original_schema = GetSchema(&db);
  ASSERT_EQ(with_meta ? 6 : 4, base::ranges::count(original_schema, '\n'))
      << original_schema;

  static constexpr char kTable1Sql[] = "SELECT * FROM table1 ORDER BY 1";
  static constexpr char kTable2Sql[] = "SELECT * FROM table2 ORDER BY 1";
  EXPECT_EQ("1|turtle\n2|truck\n3|trailer",
            ExecuteWithResults(&db, kTable1Sql, "|", "\n"));
  EXPECT_EQ("bob|truck\ndean|trailer\njim|telephone",
            ExecuteWithResults(&db, kTable2Sql, "|", "\n"));

  // Database handle is valid before recovery, poisoned after.
  static constexpr char kTrivialSql[] = "SELECT COUNT(*) FROM sqlite_schema";
  EXPECT_TRUE(db.IsSQLValid(kTrivialSql));

  std::move(run_recovery).Run();

  EXPECT_FALSE(db.is_open());

  // Since the database was not corrupt, the entire schema and all data should
  // be recovered. Re-open the database.
  db.Close();
  ASSERT_TRUE(db.Open(db_path));
  ASSERT_EQ(original_schema, GetSchema(&db));
  EXPECT_EQ("1|turtle\n2|truck\n3|trailer",
            ExecuteWithResults(&db, kTable1Sql, "|", "\n"));
  EXPECT_EQ("bob|truck\ndean|trailer\njim|telephone",
            ExecuteWithResults(&db, kTable2Sql, "|", "\n"));

  if (with_meta) {
    MetaTable meta;
    EXPECT_TRUE(meta.Init(&db, kVersion, kCompatibleVersion));
    EXPECT_EQ(kVersion, meta.GetVersionNumber());
    EXPECT_EQ(kCompatibleVersion, meta.GetCompatibleVersionNumber());
  }
}

TEST_P(SqlRecoveryTest, RecoverDatabase) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(
        Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
        SqliteResultCode::kOk);
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/false,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverDatabaseMeta) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(Recovery::RecoverDatabase(
                  &db_, Recovery::Strategy::kRecoverWithMetaVersionOrRaze),
              SqliteResultCode::kOk);
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/true,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossible) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT, Recovery::Strategy::kRecoverOrRaze));
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/false,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossibleMeta) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT,
        Recovery::Strategy::kRecoverWithMetaVersionOrRaze));
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/true,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossibleWithoutErrorCallback) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    // `RecoverIfPossible()` should not set an error callback.
    EXPECT_FALSE(db_.has_error_callback());
    bool recovery_was_attempted = Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT,
        Recovery::Strategy::kRecoverWithMetaVersionOrRaze);
    EXPECT_TRUE(recovery_was_attempted);
    EXPECT_FALSE(db_.has_error_callback());
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/true,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossibleWithErrorCallback) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    db_.set_error_callback(base::DoNothing());
    // The error callback should be reset during `RecoverIfPossible()` if
    // recovery was attempted.
    bool recovery_was_attempted = Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT,
        Recovery::Strategy::kRecoverWithMetaVersionOrRaze);
    EXPECT_TRUE(recovery_was_attempted);
    EXPECT_NE(db_.has_error_callback(), recovery_was_attempted);
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/true,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossibleWithClosedDatabase) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    // Recovery should not be attempted on a closed database.
    db_.Close();

    EXPECT_FALSE(Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT, Recovery::Strategy::kRecoverOrRaze));
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/false,
                      std::move(run_recovery));
}

TEST_P(SqlRecoveryTest, RecoverIfPossibleWithPerDatabaseUma) {
  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(Recovery::RecoverIfPossible(
        &db_, SQLITE_CORRUPT, Recovery::Strategy::kRecoverOrRaze));
  });

  TestRecoverDatabase(db_, db_path_, /*with_meta=*/false,
                      std::move(run_recovery));

  // Log to the overall histograms.
  histogram_tester_.ExpectUniqueSample(kRecoveryResultHistogramName,
                                       Recovery::Result::kSuccess,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                       SqliteLoggedResultCode::kNoError,
                                       /*expected_bucket_count=*/1);
  // And the histograms for this specific feature.
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kRecoveryResultHistogramName, ".MyFeatureDatabase"}),
      Recovery::Result::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({kRecoveryResultCodeHistogramName, ".MyFeatureDatabase"}),
      SqliteLoggedResultCode::kNoError,
      /*expected_bucket_count=*/1);
}

TEST_P(SqlRecoveryTest, RecoverDatabaseWithView) {
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
  EXPECT_EQ(Recovery::RecoverDatabase(&db, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);
  EXPECT_FALSE(db.IsSQLValid(kTrivialSql));

  // Since the database was not corrupt, the entire schema and all data should
  // be recovered.
  db.Close();
  ASSERT_TRUE(db.Open(db_path_));
  EXPECT_EQ("trailer\ntruck", ExecuteWithResults(&db, kViewSql, "|", "\n"));
}

// When RecoverDatabase() encounters SQLITE_NOTADB, the database is deleted.
TEST_P(SqlRecoveryTest, RecoverDatabaseDelete) {
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
    ASSERT_FALSE(Reopen());

    // This should "recover" the database by making it valid, but empty.
    EXPECT_EQ(
        Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
        SqliteResultCode::kNotADatabase);
    histogram_tester_.ExpectUniqueSample(kRecoveryResultHistogramName,
                                         Recovery::Result::kFailedRecoveryRun,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                         SqliteLoggedResultCode::kNotADatabase,
                                         /*expected_bucket_count=*/1);
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Recovery poisoned the handle, must re-open.
  db_.Close();
  ASSERT_TRUE(Reopen());

  EXPECT_EQ("", GetSchema(&db_));
}

// Allow callers to validate the database between recovery and commit.
TEST_P(SqlRecoveryTest, BeginRecoverDatabase) {
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
  EXPECT_EQ(Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);
  db_.Close();
  ASSERT_TRUE(Reopen());

  EXPECT_EQ("15", ExecuteWithResult(&db_, kIndexedCountSql))
      << "Index should be reconstructed after database recovery";
}

TEST_P(SqlRecoveryTest, AttachFailure) {
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
    ASSERT_FALSE(Reopen());

    // Begin() should fail.
    EXPECT_EQ(
        Recovery::RecoverDatabase(&db_, Recovery::Strategy::kRecoverOrRaze),
        SqliteResultCode::kNotADatabase);
    histogram_tester_.ExpectUniqueSample(kRecoveryResultHistogramName,
                                         Recovery::Result::kFailedRecoveryRun,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(kRecoveryResultCodeHistogramName,
                                         SqliteLoggedResultCode::kNotADatabase,
                                         /*expected_bucket_count=*/1);
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

// Helper for SqlRecoveryTest.PageSize.  Creates a fresh db based on db_prefix,
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
  EXPECT_EQ(Recovery::RecoverDatabase(&recover_db,
                                      Recovery::Strategy::kRecoverOrRaze),
            SqliteResultCode::kOk);

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
TEST_P(SqlRecoveryTest, PageSize) {
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

  ASSERT_NE("2048", default_page_size);
  // Databases with no page size specified should recover to the page size of
  // the source database.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(
      db_path_, 2048, "2048", DatabaseOptions::kDefaultPageSize, "2048"));
}

TEST_P(SqlRecoveryTest, CannotRecoverClosedDb) {
  db_.Close();

  EXPECT_CHECK_DEATH(std::ignore = Recovery::RecoverDatabase(
                         &db_, Recovery::Strategy::kRecoverOrRaze));
}

TEST_P(SqlRecoveryTest, CannotRecoverDbWithErrorCallback) {
  db_.set_error_callback(base::DoNothing());

  EXPECT_CHECK_DEATH(std::ignore = Recovery::RecoverDatabase(
                         &db_, Recovery::Strategy::kRecoverOrRaze));
}

// TODO(crbug.com/40199997): Ideally this would be a
// `SqlRecoveryTest`, but `Recovery::RecoverDatabase()` does not DCHECK
// that it is passed a non-null database pointer and will instead likely result
// in unexpected behavior or crashes.
TEST_P(SqlRecoveryTest, CannotRecoverNullDb) {
  EXPECT_CHECK_DEATH(std::ignore = Recovery::RecoverDatabase(
                         nullptr, Recovery::Strategy::kRecoverOrRaze));
}

// TODO(crbug.com/40199997): Ideally this would be a
// `SqlRecoveryTest`, but `Recovery::RecoverDatabase()` does not DCHECK
// whether the database is in-memory and will instead likely result in
// unexpected behavior or crashes.
TEST_P(SqlRecoveryTest, CannotRecoverInMemoryDb) {
  Database in_memory_db;
  ASSERT_TRUE(in_memory_db.OpenInMemory());

  EXPECT_CHECK_DEATH(std::ignore = Recovery::RecoverDatabase(
                         &in_memory_db, Recovery::Strategy::kRecoverOrRaze));
}

// This test mimics the case where a database that was using WAL mode crashed,
// then next Chrome launch the database is not opened in WAL mode. This may
// occur when e.g. WAL mode if configured via Finch and the user not in the
// experiment group on the second launch of Chrome.
TEST_P(SqlRecoveryTest, PRE_RecoverFormerlyWalDbAfterCrash) {
  base::FilePath wal_db_path =
      temp_dir_.GetPath().AppendASCII("recovery_wal_test.sqlite");

  // Open the DB in WAL mode to set journal_mode="wal".
  Database wal_db{{.wal_mode = true}};
  ASSERT_TRUE(wal_db.Open(wal_db_path));

  EXPECT_TRUE(wal_db.UseWALMode());
  EXPECT_EQ(ExecuteWithResult(&wal_db, "PRAGMA journal_mode"), "wal");

  // Crash the database somehow, foregoing the opportunity for any cleanup.
  wal_db.set_error_callback(base::DoNothing());
  EXPECT_DCHECK_DEATH(wal_db.set_error_callback(base::DoNothing()));
}

TEST_P(SqlRecoveryTest, RecoverFormerlyWalDbAfterCrash) {
  base::FilePath wal_db_path =
      temp_dir_.GetPath().AppendASCII("recovery_wal_test.sqlite");

  Database non_wal_db{{.wal_mode = false}};
  ASSERT_TRUE(non_wal_db.Open(wal_db_path));

  auto run_recovery = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(
        Recovery::RecoverDatabase(
            &non_wal_db, Recovery::Strategy::kRecoverWithMetaVersionOrRaze),
        SqliteResultCode::kOk);
  });

  TestRecoverDatabase(non_wal_db, wal_db_path, /*with_meta=*/true,
                      std::move(run_recovery));
}

}  // namespace

}  // namespace sql
