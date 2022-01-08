// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/backup_util.h"
#endif

// Test that certain features are/are-not enabled in our SQLite.

namespace sql {
namespace {

using sql::test::ExecuteWithResult;
using sql::test::ExecuteWithResults;

}  // namespace

class SQLiteFeaturesTest : public testing::Test {
 public:
  ~SQLiteFeaturesTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("sqlite_features_test.sqlite");
    ASSERT_TRUE(db_.Open(db_path_));
  }

  bool Reopen() {
    db_.Close();
    return db_.Open(db_path_);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  Database db_;

  // The error code of the most recent error.
  int error_ = SQLITE_OK;
  // Original statement which has caused the error.
  std::string sql_text_;
};

// Do not include fts1 support, it is not useful, and nobody is
// looking at it.
TEST_F(SQLiteFeaturesTest, NoFTS1) {
  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_ERROR);
  EXPECT_FALSE(db_.Execute("CREATE VIRTUAL TABLE foo USING fts1(x)"));
  EXPECT_TRUE(expecter.SawExpectedErrors());
}

// Do not include fts2 support, it is not useful, and nobody is
// looking at it.
TEST_F(SQLiteFeaturesTest, NoFTS2) {
  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_ERROR);
  EXPECT_FALSE(db_.Execute("CREATE VIRTUAL TABLE foo USING fts2(x)"));
  EXPECT_TRUE(expecter.SawExpectedErrors());
}

// fts3 is exposed in WebSQL.
TEST_F(SQLiteFeaturesTest, FTS3) {
  EXPECT_TRUE(db_.Execute("CREATE VIRTUAL TABLE foo USING fts3(x)"));
}

// Originally history used fts2, which Chromium patched to treat "foo*" as a
// prefix search, though the icu tokenizer would return it as two tokens {"foo",
// "*"}.  Test that fts3 works correctly.
TEST_F(SQLiteFeaturesTest, FTS3_Prefix) {
  db_.Close();
  sql::Database db({.enable_virtual_tables_discouraged = true});
  ASSERT_TRUE(db.Open(db_path_));

  static constexpr char kCreateSql[] =
      "CREATE VIRTUAL TABLE foo USING fts3(x, tokenize icu)";
  ASSERT_TRUE(db.Execute(kCreateSql));

  ASSERT_TRUE(db.Execute("INSERT INTO foo (x) VALUES ('test')"));

  EXPECT_EQ("test",
            ExecuteWithResult(&db, "SELECT x FROM foo WHERE x MATCH 'te*'"));
}

// Verify that Chromium's SQLite is compiled with HAVE_USLEEP defined.  With
// HAVE_USLEEP, SQLite uses usleep() with millisecond granularity.  Otherwise it
// uses sleep() with second granularity.
TEST_F(SQLiteFeaturesTest, UsesUsleep) {
  base::TimeTicks before = base::TimeTicks::Now();
  sqlite3_sleep(1);
  base::TimeDelta delta = base::TimeTicks::Now() - before;

  // It is not impossible for this to be over 1000 if things are compiled
  // correctly, but that is very unlikely.  Most platforms seem to be exactly
  // 1ms, with the rest at 2ms, and the worst observed cases was ASAN at 7ms.
  EXPECT_LT(delta.InMilliseconds(), 1000);
}

// Ensure that our SQLite version has working foreign key support with cascade
// delete support.
TEST_F(SQLiteFeaturesTest, ForeignKeySupport) {
  ASSERT_TRUE(db_.Execute("PRAGMA foreign_keys=1"));
  ASSERT_TRUE(db_.Execute("CREATE TABLE parents (id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.Execute(
      "CREATE TABLE children ("
      "    id INTEGER PRIMARY KEY,"
      "    pid INTEGER NOT NULL REFERENCES parents(id) ON DELETE CASCADE)"));
  static const char kSelectParentsSql[] = "SELECT * FROM parents ORDER BY id";
  static const char kSelectChildrenSql[] = "SELECT * FROM children ORDER BY id";

  // Inserting without a matching parent should fail with constraint violation.
  EXPECT_EQ("", ExecuteWithResult(&db_, kSelectParentsSql));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT | SQLITE_CONSTRAINT_FOREIGNKEY);
    EXPECT_FALSE(db_.Execute("INSERT INTO children VALUES (10, 1)"));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  EXPECT_EQ("", ExecuteWithResult(&db_, kSelectChildrenSql));

  // Inserting with a matching parent should work.
  ASSERT_TRUE(db_.Execute("INSERT INTO parents VALUES (1)"));
  EXPECT_EQ("1", ExecuteWithResults(&db_, kSelectParentsSql, "|", "\n"));
  EXPECT_TRUE(db_.Execute("INSERT INTO children VALUES (11, 1)"));
  EXPECT_TRUE(db_.Execute("INSERT INTO children VALUES (12, 1)"));
  EXPECT_EQ("11|1\n12|1",
            ExecuteWithResults(&db_, kSelectChildrenSql, "|", "\n"));

  // Deleting the parent should cascade, deleting the children as well.
  ASSERT_TRUE(db_.Execute("DELETE FROM parents"));
  EXPECT_EQ("", ExecuteWithResult(&db_, kSelectParentsSql));
  EXPECT_EQ("", ExecuteWithResult(&db_, kSelectChildrenSql));
}

// Ensure that our SQLite version supports booleans.
TEST_F(SQLiteFeaturesTest, BooleanSupport) {
  ASSERT_TRUE(
      db_.Execute("CREATE TABLE flags ("
                  "    id INTEGER PRIMARY KEY,"
                  "    true_flag BOOL NOT NULL DEFAULT TRUE,"
                  "    false_flag BOOL NOT NULL DEFAULT FALSE)"));
  ASSERT_TRUE(db_.Execute(
      "ALTER TABLE flags ADD COLUMN true_flag2 BOOL NOT NULL DEFAULT TRUE"));
  ASSERT_TRUE(db_.Execute(
      "ALTER TABLE flags ADD COLUMN false_flag2 BOOL NOT NULL DEFAULT FALSE"));
  ASSERT_TRUE(db_.Execute("INSERT INTO flags (id) VALUES (1)"));

  sql::Statement s(db_.GetUniqueStatement(
      "SELECT true_flag, false_flag, true_flag2, false_flag2"
      "    FROM flags WHERE id=1;"));
  ASSERT_TRUE(s.Step());

  EXPECT_TRUE(s.ColumnBool(0)) << " default TRUE at table creation time";
  EXPECT_TRUE(!s.ColumnBool(1)) << " default FALSE at table creation time";

  EXPECT_TRUE(s.ColumnBool(2)) << " default TRUE added by altering the table";
  EXPECT_TRUE(!s.ColumnBool(3)) << " default FALSE added by altering the table";
}

TEST_F(SQLiteFeaturesTest, IcuEnabled) {
  sql::Statement lower_en(db_.GetUniqueStatement("SELECT lower('I', 'en_us')"));
  ASSERT_TRUE(lower_en.Step());
  EXPECT_EQ("i", lower_en.ColumnString(0));

  sql::Statement lower_tr(db_.GetUniqueStatement("SELECT lower('I', 'tr_tr')"));
  ASSERT_TRUE(lower_tr.Step());
  EXPECT_EQ("\u0131", lower_tr.ColumnString(0));
}

// Verify that OS file writes are reflected in the memory mapping of a
// memory-mapped file.  Normally SQLite writes to memory-mapped files using
// memcpy(), which should stay consistent.  Our SQLite is slightly patched to
// mmap read only, then write using OS file writes.  If the memory-mapped
// version doesn't reflect the OS file writes, SQLite's memory-mapped I/O should
// be disabled on this platform using SQLITE_MAX_MMAP_SIZE=0.
TEST_F(SQLiteFeaturesTest, Mmap) {
  // Try to turn on mmap'ed I/O.
  std::ignore = db_.Execute("PRAGMA mmap_size = 1048576");
  {
    sql::Statement s(db_.GetUniqueStatement("PRAGMA mmap_size"));

    ASSERT_TRUE(s.Step());
    ASSERT_GT(s.ColumnInt64(0), 0);
  }
  db_.Close();

  const uint32_t kFlags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
  char buf[4096];

  // Create a file with a block of '0', a block of '1', and a block of '2'.
  {
    base::File f(db_path_, kFlags);
    ASSERT_TRUE(f.IsValid());
    memset(buf, '0', sizeof(buf));
    ASSERT_EQ(f.Write(0*sizeof(buf), buf, sizeof(buf)), (int)sizeof(buf));

    memset(buf, '1', sizeof(buf));
    ASSERT_EQ(f.Write(1*sizeof(buf), buf, sizeof(buf)), (int)sizeof(buf));

    memset(buf, '2', sizeof(buf));
    ASSERT_EQ(f.Write(2*sizeof(buf), buf, sizeof(buf)), (int)sizeof(buf));
  }

  // mmap the file and verify that everything looks right.
  {
    base::MemoryMappedFile m;
    ASSERT_TRUE(m.Initialize(db_path_));

    memset(buf, '0', sizeof(buf));
    ASSERT_EQ(0, memcmp(buf, m.data() + 0*sizeof(buf), sizeof(buf)));

    memset(buf, '1', sizeof(buf));
    ASSERT_EQ(0, memcmp(buf, m.data() + 1*sizeof(buf), sizeof(buf)));

    memset(buf, '2', sizeof(buf));
    ASSERT_EQ(0, memcmp(buf, m.data() + 2*sizeof(buf), sizeof(buf)));

    // Scribble some '3' into the first page of the file, and verify that it
    // looks the same in the memory mapping.
    {
      base::File f(db_path_, kFlags);
      ASSERT_TRUE(f.IsValid());
      memset(buf, '3', sizeof(buf));
      ASSERT_EQ(f.Write(0*sizeof(buf), buf, sizeof(buf)), (int)sizeof(buf));
    }
    ASSERT_EQ(0, memcmp(buf, m.data() + 0*sizeof(buf), sizeof(buf)));

    // Repeat with a single '4' in case page-sized blocks are different.
    const size_t kOffset = 1*sizeof(buf) + 123;
    ASSERT_NE('4', m.data()[kOffset]);
    {
      base::File f(db_path_, kFlags);
      ASSERT_TRUE(f.IsValid());
      buf[0] = '4';
      ASSERT_EQ(f.Write(kOffset, buf, 1), 1);
    }
    ASSERT_EQ('4', m.data()[kOffset]);
  }
}

// Verify that http://crbug.com/248608 is fixed.  In this bug, the
// compiled regular expression is effectively cached with the prepared
// statement, causing errors if the regular expression is rebound.
TEST_F(SQLiteFeaturesTest, CachedRegexp) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE r (id INTEGER UNIQUE, x TEXT)"));
  ASSERT_TRUE(db_.Execute("INSERT INTO r VALUES (1, 'this is a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO r VALUES (2, 'that was a test')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO r VALUES (3, 'this is a stickup')"));
  ASSERT_TRUE(db_.Execute("INSERT INTO r VALUES (4, 'that sucks')"));

  static const char kSimpleSql[] = "SELECT SUM(id) FROM r WHERE x REGEXP ?";
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, kSimpleSql));

  s.BindString(0, "this.*");
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(4, s.ColumnInt(0));

  s.Reset(true);
  s.BindString(0, "that.*");
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(6, s.ColumnInt(0));

  s.Reset(true);
  s.BindString(0, ".*test");
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(3, s.ColumnInt(0));

  s.Reset(true);
  s.BindString(0, ".* s[a-z]+");
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(7, s.ColumnInt(0));
}

#if BUILDFLAG(IS_APPLE)
// If a database file is marked to be excluded from backups, verify that journal
// files are also excluded.
TEST_F(SQLiteFeaturesTest, TimeMachine) {
  ASSERT_TRUE(db_.Execute("CREATE TABLE t (id INTEGER PRIMARY KEY)"));
  db_.Close();

  base::FilePath journal_path = sql::Database::JournalPath(db_path_);
  ASSERT_TRUE(base::PathExists(db_path_));
  ASSERT_TRUE(base::PathExists(journal_path));

  // Not excluded to start.
  EXPECT_FALSE(base::mac::GetBackupExclusion(db_path_));
  EXPECT_FALSE(base::mac::GetBackupExclusion(journal_path));

  // Exclude the main database file.
  EXPECT_TRUE(base::mac::SetBackupExclusion(db_path_));

  EXPECT_TRUE(base::mac::GetBackupExclusion(db_path_));
  EXPECT_FALSE(base::mac::GetBackupExclusion(journal_path));

  EXPECT_TRUE(db_.Open(db_path_));
  ASSERT_TRUE(db_.Execute("INSERT INTO t VALUES (1)"));
  EXPECT_TRUE(base::mac::GetBackupExclusion(db_path_));
  EXPECT_TRUE(base::mac::GetBackupExclusion(journal_path));

  // TODO(shess): In WAL mode this will touch -wal and -shm files.  -shm files
  // could be always excluded.
}
#endif

#if !BUILDFLAG(IS_FUCHSIA)
// SQLite WAL mode defaults to checkpointing the WAL on close.  This would push
// additional work into Chromium shutdown.  Verify that SQLite supports a config
// option to not checkpoint on close.
TEST_F(SQLiteFeaturesTest, WALNoClose) {
  base::FilePath wal_path = sql::Database::WriteAheadLogPath(db_path_);

  // Turn on WAL mode, then verify that the mode changed (WAL is supported).
  ASSERT_TRUE(db_.Execute("PRAGMA journal_mode = WAL"));
  ASSERT_EQ("wal", ExecuteWithResult(&db_, "PRAGMA journal_mode"));

  // The WAL file is created lazily on first change.
  ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a, b)"));

  // By default, the WAL is checkpointed then deleted on close.
  ASSERT_TRUE(base::PathExists(wal_path));
  db_.Close();
  ASSERT_FALSE(base::PathExists(wal_path));

  // Reopen and configure the database to not checkpoint WAL on close.
  ASSERT_TRUE(Reopen());
  ASSERT_TRUE(db_.Execute("PRAGMA journal_mode = WAL"));
  ASSERT_TRUE(db_.Execute("ALTER TABLE foo ADD COLUMN c"));
  ASSERT_EQ(
      SQLITE_OK,
      sqlite3_db_config(db_.db_, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 1, nullptr));
  ASSERT_TRUE(base::PathExists(wal_path));
  db_.Close();
  ASSERT_TRUE(base::PathExists(wal_path));
}
#endif

}  // namespace sql
