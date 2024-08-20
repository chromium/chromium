// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sql/database.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "sql/database_memory_dump_provider.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/strcat.h"
#endif

namespace sql {

namespace {

using sql::test::ExecuteWithResult;

// Helper to return the count of items in sqlite_schema.  Return -1 in
// case of error.
int SqliteSchemaCount(Database* db) {
  static constexpr char kSchemaCount[] = "SELECT COUNT(*) FROM sqlite_schema";
  Statement s(db->GetUniqueStatement(kSchemaCount));
  return s.Step() ? s.ColumnInt(0) : -1;
}

// Handle errors by blowing away the database.
void RazeErrorCallback(Database* db,
                       int expected_error,
                       int error,
                       Statement* stmt) {
  // Nothing here needs extended errors at this time.
  EXPECT_EQ(expected_error, expected_error & 0xff);
  EXPECT_EQ(expected_error, error & 0xff);
  db->RazeAndPoison();
}

#if BUILDFLAG(IS_POSIX)
// Set a umask and restore the old mask on destruction.  Cribbed from
// shared_memory_unittest.cc.  Used by POSIX-only UserPermission test.
class ScopedUmaskSetter {
 public:
  explicit ScopedUmaskSetter(mode_t target_mask) {
    old_umask_ = umask(target_mask);
  }
  ~ScopedUmaskSetter() { umask(old_umask_); }

  ScopedUmaskSetter(const ScopedUmaskSetter&) = delete;
  ScopedUmaskSetter& operator=(const ScopedUmaskSetter&) = delete;

 private:
  mode_t old_umask_;
};
#endif  // BUILDFLAG(IS_POSIX)

bool IsOpenedInCorrectJournalMode(Database* db, bool is_wal) {
  std::string expected_mode = is_wal ? "wal" : "truncate";
  return ExecuteWithResult(db, "PRAGMA journal_mode") == expected_mode;
}

}  // namespace

// We use the parameter to run all tests with WAL mode on and off.
class SQLDatabaseTest : public testing::Test,
                        public testing::WithParamInterface<bool> {
 public:
  enum class OverwriteType {
    kTruncate,
    kOverwrite,
  };

  ~SQLDatabaseTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("database_test.sqlite");
    CreateFreshDB();
  }

  // Resets the database handle and deletes the backing file. On return, `db_`
  // has just been opened on a fresh temp file named by `db_path_`.
  void CreateFreshDB() {
    ASSERT_FALSE(db_path_.empty());

    db_.reset();
    ASSERT_TRUE(base::DeleteFile(db_path_));

    db_ = std::make_unique<Database>(GetDBOptions());
    ASSERT_TRUE(db_->Open(db_path_));
    ASSERT_TRUE(base::PathExists(db_path_));
  }

  DatabaseOptions GetDBOptions() {
    DatabaseOptions options;
    options.wal_mode = IsWALEnabled();
    // TODO(crbug.com/40146017): Remove after switching to exclusive mode on by
    // default.
    options.exclusive_locking = false;
#if BUILDFLAG(IS_FUCHSIA)  // Exclusive mode needs to be enabled to enter WAL
                           // mode on Fuchsia
    if (IsWALEnabled()) {
      options.exclusive_locking = true;
    }
#endif  // BUILDFLAG(IS_FUCHSIA)
    return options;
  }

  bool IsWALEnabled() { return GetParam(); }

  bool TruncateDatabase() {
    base::File file(db_path_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    return file.SetLength(0);
  }

  bool OverwriteDatabaseHeader(OverwriteType type) {
    base::File file(db_path_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (type == OverwriteType::kTruncate) {
      if (!file.SetLength(0))
        return false;
    }

    static constexpr char kText[] = "Now is the winter of our discontent.";
    constexpr int kTextBytes = sizeof(kText) - 1;
    return file.Write(0, kText, kTextBytes) == kTextBytes;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<Database> db_;
};

TEST_P(SQLDatabaseTest, Execute_ValidStatement) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE data(contents TEXT)"));
  EXPECT_EQ(SQLITE_OK, db_->GetErrorCode());
}

TEST_P(SQLDatabaseTest, Execute_InvalidStatement) {
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(db_->Execute("CREATE TABLE data("));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());
  }
  EXPECT_EQ(SQLITE_ERROR, db_->GetErrorCode());
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_OneLineValid) {
  ASSERT_TRUE(db_->ExecuteScriptForTesting("CREATE TABLE data(contents TEXT)"));
  EXPECT_EQ(SQLITE_OK, db_->GetErrorCode());
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_OneLineInvalid) {
  ASSERT_FALSE(db_->ExecuteScriptForTesting("CREATE TABLE data("));
  EXPECT_EQ(SQLITE_ERROR, db_->GetErrorCode());
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_ExtraContents) {
  EXPECT_TRUE(db_->ExecuteScriptForTesting("CREATE TABLE data1(id)"))
      << "Minimal statement";
  EXPECT_TRUE(db_->ExecuteScriptForTesting("CREATE TABLE data2(id);"))
      << "Extra semicolon";
  EXPECT_TRUE(db_->ExecuteScriptForTesting("CREATE TABLE data3(id) -- Comment"))
      << "Trailing comment";

  EXPECT_TRUE(db_->ExecuteScriptForTesting(
      "CREATE TABLE data4(id);CREATE TABLE data5(id)"))
      << "Extra statement without whitespace";
  EXPECT_TRUE(db_->ExecuteScriptForTesting(
      "CREATE TABLE data6(id); CREATE TABLE data7(id)"))
      << "Extra statement separated by whitespace";

  EXPECT_TRUE(db_->ExecuteScriptForTesting("CREATE TABLE data8(id);-- Comment"))
      << "Comment without whitespace";
  EXPECT_TRUE(
      db_->ExecuteScriptForTesting("CREATE TABLE data9(id); -- Comment"))
      << "Comment sepatated by whitespace";
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_MultipleValidLines) {
  EXPECT_TRUE(db_->ExecuteScriptForTesting(R"(
      CREATE TABLE data1(contents TEXT);
      CREATE TABLE data2(contents TEXT);
      CREATE TABLE data3(contents TEXT);
  )"));
  EXPECT_EQ(SQLITE_OK, db_->GetErrorCode());

  // DoesColumnExist() is implemented directly on top of a SQLite call. The
  // other schema functions use sql::Statement infrastructure to query the
  // schema table.
  EXPECT_TRUE(db_->DoesColumnExist("data1", "contents"));
  EXPECT_TRUE(db_->DoesColumnExist("data2", "contents"));
  EXPECT_TRUE(db_->DoesColumnExist("data3", "contents"));
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_StopsOnCompileError) {
  EXPECT_FALSE(db_->ExecuteScriptForTesting(R"(
      CREATE TABLE data1(contents TEXT);
      CREATE TABLE data1();
      CREATE TABLE data3(contents TEXT);
  )"));
  EXPECT_EQ(SQLITE_ERROR, db_->GetErrorCode());

  EXPECT_TRUE(db_->DoesColumnExist("data1", "contents"));
  EXPECT_FALSE(db_->DoesColumnExist("data3", "contents"));
}

TEST_P(SQLDatabaseTest, ExecuteScriptForTesting_StopsOnStepError) {
  EXPECT_FALSE(db_->ExecuteScriptForTesting(R"(
      CREATE TABLE data1(contents TEXT UNIQUE);
      INSERT INTO data1(contents) VALUES('value1');
      INSERT INTO data1(contents) VALUES('value1');
      CREATE TABLE data3(contents TEXT);
  )"));
  EXPECT_EQ(SQLITE_CONSTRAINT_UNIQUE, db_->GetErrorCode());

  EXPECT_TRUE(db_->DoesColumnExist("data1", "contents"));
  EXPECT_FALSE(db_->DoesColumnExist("data3", "contents"));
}

TEST_P(SQLDatabaseTest, CachedStatement) {
  StatementID id1 = SQL_FROM_HERE;
  StatementID id2 = SQL_FROM_HERE;
  static constexpr char kId1Sql[] = "SELECT a FROM foo";
  static constexpr char kId2Sql[] = "SELECT b FROM foo";

  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo(a, b) VALUES (12, 13)"));

  sqlite3_stmt* raw_id1_statement;
  sqlite3_stmt* raw_id2_statement;
  {
    scoped_refptr<Database::StatementRef> ref_from_id1 =
        db_->GetCachedStatement(id1, kId1Sql);
    raw_id1_statement = ref_from_id1->stmt();

    Statement from_id1(std::move(ref_from_id1));
    ASSERT_TRUE(from_id1.is_valid());
    ASSERT_TRUE(from_id1.Step());
    EXPECT_EQ(12, from_id1.ColumnInt(0));

    scoped_refptr<Database::StatementRef> ref_from_id2 =
        db_->GetCachedStatement(id2, kId2Sql);
    raw_id2_statement = ref_from_id2->stmt();
    EXPECT_NE(raw_id1_statement, raw_id2_statement);

    Statement from_id2(std::move(ref_from_id2));
    ASSERT_TRUE(from_id2.is_valid());
    ASSERT_TRUE(from_id2.Step());
    EXPECT_EQ(13, from_id2.ColumnInt(0));
  }

  {
    scoped_refptr<Database::StatementRef> ref_from_id1 =
        db_->GetCachedStatement(id1, kId1Sql);
    EXPECT_EQ(raw_id1_statement, ref_from_id1->stmt())
        << "statement was not cached";

    Statement from_id1(std::move(ref_from_id1));
    ASSERT_TRUE(from_id1.is_valid());
    ASSERT_TRUE(from_id1.Step()) << "cached statement was not reset";
    EXPECT_EQ(12, from_id1.ColumnInt(0));

    scoped_refptr<Database::StatementRef> ref_from_id2 =
        db_->GetCachedStatement(id2, kId2Sql);
    EXPECT_EQ(raw_id2_statement, ref_from_id2->stmt())
        << "statement was not cached";

    Statement from_id2(std::move(ref_from_id2));
    ASSERT_TRUE(from_id2.is_valid());
    ASSERT_TRUE(from_id2.Step()) << "cached statement was not reset";
    EXPECT_EQ(13, from_id2.ColumnInt(0));
  }

  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(id1, kId2Sql))
      << "Using a different SQL with the same statement ID should DCHECK";
  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(id2, kId1Sql))
      << "Using a different SQL with the same statement ID should DCHECK";
}

TEST_P(SQLDatabaseTest, IsSQLValidTest) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_->IsSQLValid("SELECT a FROM foo"));
  ASSERT_FALSE(db_->IsSQLValid("SELECT no_exist FROM foo"));
}

TEST_P(SQLDatabaseTest, DoesTableExist) {
  EXPECT_FALSE(db_->DoesTableExist("foo"));
  EXPECT_FALSE(db_->DoesTableExist("foo_index"));

  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_->Execute("CREATE INDEX foo_index ON foo (a)"));
  EXPECT_TRUE(db_->DoesTableExist("foo"));
  EXPECT_FALSE(db_->DoesTableExist("foo_index"));

  // DoesTableExist() is case-sensitive.
  EXPECT_FALSE(db_->DoesTableExist("Foo"));
  EXPECT_FALSE(db_->DoesTableExist("FOO"));
}

TEST_P(SQLDatabaseTest, DoesIndexExist) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));
  EXPECT_FALSE(db_->DoesIndexExist("foo"));
  EXPECT_FALSE(db_->DoesIndexExist("foo_ubdex"));

  ASSERT_TRUE(db_->Execute("CREATE INDEX foo_index ON foo (a)"));
  EXPECT_TRUE(db_->DoesIndexExist("foo_index"));
  EXPECT_FALSE(db_->DoesIndexExist("foo"));

  // DoesIndexExist() is case-sensitive.
  EXPECT_FALSE(db_->DoesIndexExist("Foo_index"));
  EXPECT_FALSE(db_->DoesIndexExist("Foo_Index"));
  EXPECT_FALSE(db_->DoesIndexExist("FOO_INDEX"));
}

TEST_P(SQLDatabaseTest, DoesViewExist) {
  EXPECT_FALSE(db_->DoesViewExist("voo"));
  ASSERT_TRUE(db_->Execute("CREATE VIEW voo (a) AS SELECT 1"));
  EXPECT_FALSE(db_->DoesIndexExist("voo"));
  EXPECT_FALSE(db_->DoesTableExist("voo"));
  EXPECT_TRUE(db_->DoesViewExist("voo"));

  // DoesTableExist() is case-sensitive.
  EXPECT_FALSE(db_->DoesViewExist("Voo"));
  EXPECT_FALSE(db_->DoesViewExist("VOO"));
}

TEST_P(SQLDatabaseTest, DoesColumnExist) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));

  EXPECT_FALSE(db_->DoesColumnExist("foo", "bar"));
  EXPECT_TRUE(db_->DoesColumnExist("foo", "a"));

  ASSERT_FALSE(db_->DoesTableExist("bar"));
  EXPECT_FALSE(db_->DoesColumnExist("bar", "b"));

  // SQLite resolves table/column names without case sensitivity.
  EXPECT_TRUE(db_->DoesColumnExist("FOO", "A"));
  EXPECT_TRUE(db_->DoesColumnExist("FOO", "a"));
  EXPECT_TRUE(db_->DoesColumnExist("foo", "A"));
}

TEST_P(SQLDatabaseTest, GetLastInsertRowId) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, value)"));

  ASSERT_TRUE(db_->Execute("INSERT INTO foo (value) VALUES (12)"));

  // Last insert row ID should be valid.
  int64_t row = db_->GetLastInsertRowId();
  EXPECT_LT(0, row);

  // It should be the primary key of the row we just inserted.
  Statement s(db_->GetUniqueStatement("SELECT value FROM foo WHERE id=?"));
  s.BindInt64(0, row);
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
}

// Test the scoped error expecter by attempting to insert a duplicate
// value into an index.
TEST_P(SQLDatabaseTest, ScopedErrorExpecter) {
  static constexpr char kCreateSql[] = "CREATE TABLE foo (id INTEGER UNIQUE)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo (id) VALUES (12)"));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    ASSERT_FALSE(db_->Execute("INSERT INTO foo (id) VALUES (12)"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_P(SQLDatabaseTest, SchemaIntrospectionUsesErrorExpecter) {
  static constexpr char kCreateSql[] = "CREATE TABLE foo (id INTEGER UNIQUE)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_FALSE(db_->DoesTableExist("bar"));
  ASSERT_TRUE(db_->DoesTableExist("foo"));
  ASSERT_TRUE(db_->DoesColumnExist("foo", "id"));
  db_->Close();

  // Corrupt the database so that nothing works, including PRAGMAs.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_FALSE(db_->Open(db_path_));
    ASSERT_FALSE(db_->DoesTableExist("bar"));
    ASSERT_FALSE(db_->DoesTableExist("foo"));
    ASSERT_FALSE(db_->DoesColumnExist("foo", "id"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_P(SQLDatabaseTest, SetErrorCallback) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));

  bool error_callback_called = false;
  int error = SQLITE_OK;
  db_->set_error_callback(base::BindLambdaForTesting(
      [&](int sqlite_error, sql::Statement* statement) {
        error_callback_called = true;
        error = sqlite_error;
      }));
  EXPECT_FALSE(db_->Execute("INSERT INTO rows(id) VALUES(12)"))
      << "Inserting a duplicate primary key should have failed";
  EXPECT_TRUE(error_callback_called)
      << "Execute() should report errors to the database error callback";
  EXPECT_EQ(SQLITE_CONSTRAINT_PRIMARYKEY, error)
      << "Execute() should report errors to the database error callback";
}

TEST_P(SQLDatabaseTest, SetErrorCallbackDchecksOnExistingCallback) {
  db_->set_error_callback(base::DoNothing());
  EXPECT_DCHECK_DEATH(db_->set_error_callback(base::DoNothing()))
      << "set_error_callback() should DCHECK if error callback already exists";
}

TEST_P(SQLDatabaseTest, ResetErrorCallback) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));

  bool error_callback_called = false;
  int error = SQLITE_OK;
  db_->set_error_callback(
      base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
        error_callback_called = true;
        error = sqlite_error;
      }));
  db_->reset_error_callback();

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    EXPECT_FALSE(db_->Execute("INSERT INTO rows(id) VALUES(12)"))
        << "Inserting a duplicate primary key should have failed";
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Inserting a duplicate primary key should have failed";
  }
  EXPECT_FALSE(error_callback_called)
      << "Execute() should not report errors after reset_error_callback()";
  EXPECT_EQ(SQLITE_OK, error)
      << "Execute() should not report errors after reset_error_callback()";
}

// Regression test for https://crbug.com/1522873
TEST_P(SQLDatabaseTest, ErrorCallbackThatClosesDb) {
  for (const bool reopen_db : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "reopen_db: " << reopen_db);
    // Ensure that `db_` is fresh in this iteration.
    CreateFreshDB();
    static constexpr char kCreateSql[] =
        "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)";
    ASSERT_TRUE(db_->Execute(kCreateSql));
    ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));

    bool error_callback_called = false;
    int error = SQLITE_OK;
    db_->set_error_callback(
        base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
          error_callback_called = true;
          error = sqlite_error;
          db_->Close();
          if (reopen_db) {
            ASSERT_TRUE(db_->Open(db_path_));
          }
        }));

    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(SQLITE_CONSTRAINT);
      EXPECT_FALSE(db_->Execute("INSERT INTO rows(id) VALUES(12)"))
          << "Inserting a duplicate primary key should have failed";
      EXPECT_TRUE(expecter.SawExpectedErrors())
          << "Inserting a duplicate primary key should have failed";
    }
    EXPECT_TRUE(error_callback_called);
    EXPECT_EQ(SQLITE_CONSTRAINT_PRIMARYKEY, error);
    EXPECT_EQ(db_->is_open(), reopen_db);
  }
}

TEST_P(SQLDatabaseTest, DetachFromSequence) {
  base::test::TaskEnvironment task_environment;

  // Get a task runner so we can post tasks to different sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  ASSERT_FALSE(task_runner->RunsTasksInCurrentSequence());

  // The database's sequence checker is already implicitly attached to the
  // current sequence because the test fixture opened it.
  ASSERT_TRUE(db_->is_open());

  // Detach before moving the Database instance to another sequence. Note that
  // it will be destroyed on the other sequence.
  db_->DetachFromSequence();
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<Database> db) {
            static constexpr char kCreateSql[] =
                "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)";
            ASSERT_TRUE(db->Execute(kCreateSql));
          },
          std::move(db_)),
      run_loop.QuitClosure());
  run_loop.Run();
}

// Regression test for https://crbug.com/1522873
TEST_P(SQLDatabaseTest, ErrorCallbackThatFreesDatabase) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));

  bool error_callback_called = false;
  int error = SQLITE_OK;
  db_->set_error_callback(
      base::BindLambdaForTesting([&](int sqlite_error, Statement* statement) {
        error_callback_called = true;
        error = sqlite_error;
        db_.reset();
      }));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT);
    EXPECT_FALSE(db_->Execute("INSERT INTO rows(id) VALUES(12)"))
        << "Inserting a duplicate primary key should have failed";
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Inserting a duplicate primary key should have failed";
  }
  EXPECT_TRUE(error_callback_called);
  EXPECT_EQ(SQLITE_CONSTRAINT_PRIMARYKEY, error);
}

// Sets a flag to true/false to track being alive.
class LifeTracker {
 public:
  explicit LifeTracker(bool* flag_ptr) : flag_ptr_(flag_ptr) {
    DCHECK(flag_ptr != nullptr);
    DCHECK(!*flag_ptr)
        << "LifeTracker's flag should be set to false prior to construction";
    *flag_ptr_ = true;
  }

  LifeTracker(LifeTracker&& rhs) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_CALLED_ON_VALID_SEQUENCE(rhs.sequence_checker_);
    flag_ptr_ = rhs.flag_ptr_;
    rhs.flag_ptr_ = nullptr;
  }

  // base::RepeatingCallback only requires move-construction support.
  LifeTracker& operator=(const LifeTracker& rhs) = delete;

  ~LifeTracker() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (flag_ptr_)
      *flag_ptr_ = false;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<bool> flag_ptr_ GUARDED_BY_CONTEXT(sequence_checker_);
};

// base::BindRepeating() can curry arguments to be passed by const reference to
// the callback function. If the error callback function calls
// reset_error_callback() and the Database doesn't hang onto the callback while
// running it, the storage for those arguments may be deleted while the callback
// function is executing. This test ensures that the database is hanging onto
// the callback while running it.
TEST_P(SQLDatabaseTest, ErrorCallbackStorageProtectedWhileRun) {
  bool is_alive = false;
  db_->set_error_callback(base::BindRepeating(
      [](Database* db, bool* life_tracker_is_alive,
         const LifeTracker& life_tracker, int sqlite_error,
         Statement* statement) {
        EXPECT_TRUE(*life_tracker_is_alive)
            << "The error callback storage should be alive when it is Run()";
        db->reset_error_callback();
        EXPECT_TRUE(*life_tracker_is_alive)
            << "The error storage should remain alive during Run() after "
            << "reset_error_callback()";
      },
      base::Unretained(db_.get()), base::Unretained(&is_alive),
      LifeTracker(&is_alive)));

  EXPECT_TRUE(is_alive)
      << "The error callback storage should be alive after creation";
  EXPECT_FALSE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));
  EXPECT_FALSE(is_alive)
      << "The error callback storage should be released after Run() completes";
}

TEST_P(SQLDatabaseTest, Execute_CompilationError) {
  bool error_callback_called = false;
  db_->set_error_callback(base::BindLambdaForTesting([&](int error,
                                                         sql::Statement*
                                                             statement) {
    EXPECT_EQ(SQLITE_ERROR, error);
    EXPECT_EQ(nullptr, statement);
    EXPECT_FALSE(error_callback_called)
        << "SQL compilation errors should call the error callback exactly once";
    error_callback_called = true;
  }));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(db_->Execute("SELECT missing_column FROM missing_table"));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  EXPECT_TRUE(error_callback_called)
      << "SQL compilation errors should call the error callback";
}

TEST_P(SQLDatabaseTest, GetUniqueStatement_CompilationError) {
  bool error_callback_called = false;
  db_->set_error_callback(base::BindLambdaForTesting([&](int error,
                                                         sql::Statement*
                                                             statement) {
    EXPECT_EQ(SQLITE_ERROR, error);
    EXPECT_EQ(nullptr, statement);
    EXPECT_FALSE(error_callback_called)
        << "SQL compilation errors should call the error callback exactly once";
    error_callback_called = true;
  }));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_ERROR);
    sql::Statement statement(
        db_->GetUniqueStatement("SELECT missing_column FROM missing_table"));
    EXPECT_FALSE(statement.is_valid());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  EXPECT_TRUE(error_callback_called)
      << "SQL compilation errors should call the error callback";
}

TEST_P(SQLDatabaseTest, GetCachedStatement_CompilationError) {
  bool error_callback_called = false;
  db_->set_error_callback(base::BindLambdaForTesting([&](int error,
                                                         sql::Statement*
                                                             statement) {
    EXPECT_EQ(SQLITE_ERROR, error);
    EXPECT_EQ(nullptr, statement);
    EXPECT_FALSE(error_callback_called)
        << "SQL compilation errors should call the error callback exactly once";
    error_callback_called = true;
  }));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_ERROR);
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "SELECT missing_column FROM missing_table"));
    EXPECT_FALSE(statement.is_valid());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  EXPECT_TRUE(error_callback_called)
      << "SQL compilation errors should call the error callback";
}

TEST_P(SQLDatabaseTest, GetUniqueStatement_ExtraContents) {
  sql::Statement minimal(db_->GetUniqueStatement("SELECT 1"));
  sql::Statement extra_semicolon(db_->GetUniqueStatement("SELECT 1;"));

  // It would be nice to flag trailing comments too, as they cost binary size.
  // However, there's no easy way of doing that.
  sql::Statement trailing_comment(
      db_->GetUniqueStatement("SELECT 1 -- Comment"));

  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("SELECT 1;SELECT 2"))
      << "Extra statement without whitespace";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("SELECT 1; SELECT 2"))
      << "Extra statement separated by whitespace";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("SELECT 1;-- Comment"))
      << "Comment without whitespace";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("SELECT 1; -- Comment"))
      << "Comment separated by whitespace";
}

TEST_P(SQLDatabaseTest, GetCachedStatement_ExtraContents) {
  sql::Statement minimal(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
  sql::Statement extra_semicolon(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1;"));

  // It would be nice to flag trailing comments too, as they cost binary size.
  // However, there's no easy way of doing that.
  sql::Statement trailing_comment(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1 -- Comment"));

  EXPECT_DCHECK_DEATH(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1;SELECT 2"))
      << "Extra statement without whitespace";
  EXPECT_DCHECK_DEATH(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1; SELECT 2"))
      << "Extra statement separated by whitespace";
  EXPECT_DCHECK_DEATH(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1;-- Comment"))
      << "Comment without whitespace";
  EXPECT_DCHECK_DEATH(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1; -- Comment"))
      << "Comment separated by whitespace";
}

TEST_P(SQLDatabaseTest, IsSQLValid_ExtraContents) {
  EXPECT_TRUE(db_->IsSQLValid("SELECT 1"));
  EXPECT_TRUE(db_->IsSQLValid("SELECT 1;"))
      << "Trailing semicolons are currently tolerated";

  // It would be nice to flag trailing comments too, as they cost binary size.
  // However, there's no easy way of doing that.
  EXPECT_TRUE(db_->IsSQLValid("SELECT 1 -- Comment"))
      << "Trailing comments are currently tolerated";

  EXPECT_DCHECK_DEATH(db_->IsSQLValid("SELECT 1;SELECT 2"))
      << "Extra statement without whitespace";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("SELECT 1; SELECT 2"))
      << "Extra statement separated by whitespace";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("SELECT 1;-- Comment"))
      << "Comment without whitespace";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("SELECT 1; -- Comment"))
      << "Comment separated by whitespace";
}

TEST_P(SQLDatabaseTest, GetUniqueStatement_NoContents) {
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("")) << "Empty string";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement(" ")) << "Space";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("\n")) << "Newline";
  EXPECT_DCHECK_DEATH(db_->GetUniqueStatement("-- Comment")) << "Comment";
}

TEST_P(SQLDatabaseTest, GetCachedStatement_NoContents) {
  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(SQL_FROM_HERE, ""))
      << "Empty string";
  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(SQL_FROM_HERE, " ")) << "Space";
  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(SQL_FROM_HERE, "\n"))
      << "Newline";
  EXPECT_DCHECK_DEATH(db_->GetCachedStatement(SQL_FROM_HERE, "-- Comment"))
      << "Comment";
}

TEST_P(SQLDatabaseTest, GetReadonlyStatement) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo (value) VALUES (12)"));

  // PRAGMA statements do not change the database file.
  {
    Statement s(db_->GetReadonlyStatement("PRAGMA analysis_limit"));
    ASSERT_TRUE(s.Step());
  }
  {
    Statement s(db_->GetReadonlyStatement("PRAGMA analysis_limit=100"));
    ASSERT_TRUE(s.is_valid());
  }

  // Create and insert statements should fail, while the same queries as unique
  // statement succeeds.
  {
    Statement s(db_->GetReadonlyStatement(
        "CREATE TABLE IF NOT EXISTS foo (id INTEGER PRIMARY KEY, value)"));
    ASSERT_FALSE(s.is_valid());
    Statement s1(db_->GetUniqueStatement(
        "CREATE TABLE IF NOT EXISTS foo (id INTEGER PRIMARY KEY, value)"));
    ASSERT_TRUE(s1.is_valid());
  }
  {
    Statement s(
        db_->GetReadonlyStatement("INSERT INTO foo (value) VALUES (12)"));
    ASSERT_FALSE(s.is_valid());
    Statement s1(
        db_->GetUniqueStatement("INSERT INTO foo (value) VALUES (12)"));
    ASSERT_TRUE(s1.is_valid());
  }
  {
    Statement s(
        db_->GetReadonlyStatement("CREATE VIRTUAL TABLE bar USING module"));
    ASSERT_FALSE(s.is_valid());
    Statement s1(
        db_->GetUniqueStatement("CREATE VIRTUAL TABLE bar USING module"));
    ASSERT_TRUE(s1.is_valid());
  }

  // Select statement is successful.
  {
    Statement s(db_->GetReadonlyStatement("SELECT * FROM foo"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnInt(1), 12);
  }
}

TEST_P(SQLDatabaseTest, IsSQLValid_NoContents) {
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("")) << "Empty string";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid(" ")) << "Space";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("\n")) << "Newline";
  EXPECT_DCHECK_DEATH(db_->IsSQLValid("-- Comment")) << "Comment";
}

// Test that Database::Raze() results in a database without the
// tables from the original database.
TEST_P(SQLDatabaseTest, Raze) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo (value) VALUES (12)"));

  int pragma_auto_vacuum = 0;
  {
    Statement s(db_->GetUniqueStatement("PRAGMA auto_vacuum"));
    ASSERT_TRUE(s.Step());
    pragma_auto_vacuum = s.ColumnInt(0);
    ASSERT_TRUE(pragma_auto_vacuum == 0 || pragma_auto_vacuum == 1);
  }

  // If auto_vacuum is set, there's an extra page to maintain a freelist.
  const int kExpectedPageCount = 2 + pragma_auto_vacuum;

  {
    Statement s(db_->GetUniqueStatement("PRAGMA page_count"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(kExpectedPageCount, s.ColumnInt(0));
  }

  {
    Statement s(db_->GetUniqueStatement("SELECT * FROM sqlite_schema"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("table", s.ColumnString(0));
    EXPECT_EQ("foo", s.ColumnString(1));
    EXPECT_EQ("foo", s.ColumnString(2));
    // Table "foo" is stored in the last page of the file.
    EXPECT_EQ(kExpectedPageCount, s.ColumnInt(3));
    EXPECT_EQ(kCreateSql, s.ColumnString(4));
  }

  ASSERT_TRUE(db_->Raze());

  {
    Statement s(db_->GetUniqueStatement("PRAGMA page_count"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(1, s.ColumnInt(0));
  }

  ASSERT_EQ(0, SqliteSchemaCount(db_.get()));

  {
    Statement s(db_->GetUniqueStatement("PRAGMA auto_vacuum"));
    ASSERT_TRUE(s.Step());
    // The new database has the same auto_vacuum as a fresh database.
    EXPECT_EQ(pragma_auto_vacuum, s.ColumnInt(0));
  }
}

TEST_P(SQLDatabaseTest, RazeDuringSelect) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(1)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(2)"));

  {
    // SELECT implicitly creates a transaction while it's executing. This
    // implicit transaction will not be caught by Raze()'s checks.
    Statement select(db_->GetUniqueStatement("SELECT id FROM rows"));
    ASSERT_TRUE(select.Step());
    EXPECT_FALSE(db_->Raze()) << "Raze() should fail while SELECT is executing";
  }

  {
    Statement count(db_->GetUniqueStatement("SELECT COUNT(*) FROM rows"));
    ASSERT_TRUE(count.Step());
    EXPECT_EQ(2, count.ColumnInt(0)) << "Raze() deleted some data";
  }
}

// Helper for SQLDatabaseTest.RazePageSize.  Creates a fresh db based on
// db_prefix, with the given initial page size, and verifies it against the
// expected size.  Then changes to the final page size and razes, verifying that
// the fresh database ends up with the expected final page size.
void TestPageSize(const base::FilePath& db_prefix,
                  int initial_page_size,
                  const std::string& expected_initial_page_size,
                  int final_page_size,
                  const std::string& expected_final_page_size) {
  static constexpr char kCreateSql[] = "CREATE TABLE x (t TEXT)";
  static constexpr char kInsertSql1[] =
      "INSERT INTO x VALUES ('This is a test')";
  static constexpr char kInsertSql2[] =
      "INSERT INTO x VALUES ('That was a test')";

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
  Database razed_db({.page_size = final_page_size});
  ASSERT_TRUE(razed_db.Open(db_path));
  // Raze will use the page size set in the connection object, which may not
  // match the file's page size.
  ASSERT_TRUE(razed_db.Raze());

  // SQLite 3.10.2 (at least) has a quirk with the sqlite3_backup() API (used by
  // Raze()) which causes the destination database to remember the previous
  // page_size, even if the overwriting database changed the page_size.  Access
  // the actual database to cause the cached value to be updated.
  EXPECT_EQ("0",
            ExecuteWithResult(&razed_db, "SELECT COUNT(*) FROM sqlite_schema"));

  EXPECT_EQ(expected_final_page_size,
            ExecuteWithResult(&razed_db, "PRAGMA page_size"));
  EXPECT_EQ("1", ExecuteWithResult(&razed_db, "PRAGMA page_count"));
}

// Verify that Recovery maintains the page size, and the virtual table
// works with page sizes other than SQLite's default.  Also verify the case
// where the default page size has changed.
TEST_P(SQLDatabaseTest, RazePageSize) {
  const std::string default_page_size =
      ExecuteWithResult(db_.get(), "PRAGMA page_size");

  // Sync uses 32k pages.
  EXPECT_NO_FATAL_FAILURE(
      TestPageSize(db_path_, 32768, "32768", 32768, "32768"));

  // Many clients use 4k pages.  This is the SQLite default after 3.12.0.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 4096, "4096", 4096, "4096"));

  // 1k is the default page size before 3.12.0.
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 1024, "1024", 1024, "1024"));

  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 2048, "2048", 4096, "4096"));

  // Databases with no page size specified should result in the default
  // page size.  2k has never been the default page size.
  ASSERT_NE("2048", default_page_size);
  EXPECT_NO_FATAL_FAILURE(TestPageSize(db_path_, 2048, "2048",
                                       DatabaseOptions::kDefaultPageSize,
                                       default_page_size));
}

// Test that Raze() results are seen in other connections.
TEST_P(SQLDatabaseTest, RazeMultiple) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));

  Database other_db(GetDBOptions());
  ASSERT_TRUE(other_db.Open(db_path_));

  // Check that the second connection sees the table.
  ASSERT_EQ(1, SqliteSchemaCount(&other_db));

  ASSERT_TRUE(db_->Raze());

  // The second connection sees the updated database.
  ASSERT_EQ(0, SqliteSchemaCount(&other_db));
}

TEST_P(SQLDatabaseTest, Raze_OtherConnectionHasWriteLock) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));

  Database other_db(GetDBOptions());
  ASSERT_TRUE(other_db.Open(db_path_));

  Transaction other_db_transaction(&other_db);
  ASSERT_TRUE(other_db_transaction.Begin());
  ASSERT_TRUE(other_db.Execute("INSERT INTO rows(id) VALUES(1)"));

  EXPECT_FALSE(db_->Raze())
      << "Raze() should fail while another connection has a write lock";

  ASSERT_TRUE(other_db_transaction.Commit());
  EXPECT_TRUE(db_->Raze())
      << "Raze() should succeed after the other connection releases the lock";
}

TEST_P(SQLDatabaseTest, Raze_OtherConnectionHasReadLock) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(1)"));

  if (IsWALEnabled()) {
    // In WAL mode, read transactions in other connections do not block a write
    // transaction.
    return;
  }

  Database other_db(GetDBOptions());
  ASSERT_TRUE(other_db.Open(db_path_));

  Statement select(other_db.GetUniqueStatement("SELECT id FROM rows"));
  ASSERT_TRUE(select.Step());
  EXPECT_FALSE(db_->Raze())
      << "Raze() should fail while another connection has a read lock";

  ASSERT_FALSE(select.Step())
      << "The SELECT statement should not produce more than one row";
  EXPECT_TRUE(db_->Raze())
      << "Raze() should succeed after the other connection releases the lock";
}

TEST_P(SQLDatabaseTest, Raze_EmptyDatabaseFile) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  db_->Close();

  ASSERT_TRUE(TruncateDatabase());
  ASSERT_TRUE(db_->Open(db_path_))
      << "Failed to reopen database after truncating";

  EXPECT_TRUE(db_->Raze()) << "Raze() failed on an empty file";
  EXPECT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "Raze() did not produce a healthy empty database";
}

// Verify that Raze() can handle a file of junk.
// Need exclusive mode off here as there are some subtleties (by design) around
// how the cache is used with it on which causes the test to fail.
TEST_P(SQLDatabaseTest, RazeNOTADB) {
  db_->Close();
  Database::Delete(db_path_);
  ASSERT_FALSE(base::PathExists(db_path_));

  ASSERT_TRUE(OverwriteDatabaseHeader(OverwriteType::kTruncate));
  ASSERT_TRUE(base::PathExists(db_path_));

  // SQLite will successfully open the handle, but fail when running PRAGMA
  // statements that access the database.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_NOTADB);

    EXPECT_FALSE(db_->Open(db_path_));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
  EXPECT_TRUE(db_->Raze());
  db_->Close();

  // Now empty, the open should open an empty database.
  EXPECT_TRUE(db_->Open(db_path_));
  EXPECT_EQ(0, SqliteSchemaCount(db_.get()));
}

// Verify that Raze() can handle a database overwritten with garbage.
TEST_P(SQLDatabaseTest, RazeNOTADB2) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_EQ(1, SqliteSchemaCount(db_.get()));
  db_->Close();

  ASSERT_TRUE(OverwriteDatabaseHeader(OverwriteType::kOverwrite));

  // SQLite will successfully open the handle, but will fail with
  // SQLITE_NOTADB on pragma statemenets which attempt to read the
  // corrupted header.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_NOTADB);
    EXPECT_FALSE(db_->Open(db_path_));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
  EXPECT_TRUE(db_->Raze());
  db_->Close();

  // Now empty, the open should succeed with an empty database.
  EXPECT_TRUE(db_->Open(db_path_));
  EXPECT_EQ(0, SqliteSchemaCount(db_.get()));
}

// Test that a callback from Open() can raze the database.  This is
// essential for cases where the Open() can fail entirely, so the
// Raze() cannot happen later.  Additionally test that when the
// callback does this during Open(), the open is retried and succeeds.
TEST_P(SQLDatabaseTest, RazeCallbackReopen) {
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  ASSERT_EQ(1, SqliteSchemaCount(db_.get()));
  db_->Close();

  // Corrupt the database so that nothing works, including PRAGMAs.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));

  // Open() will succeed, even though the PRAGMA calls within will
  // fail with SQLITE_CORRUPT, as will this PRAGMA.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_FALSE(db_->Open(db_path_));
    ASSERT_FALSE(db_->Execute("PRAGMA auto_vacuum"));
    db_->Close();
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  db_->set_error_callback(
      base::BindRepeating(&RazeErrorCallback, db_.get(), SQLITE_CORRUPT));

  // When the PRAGMA calls in Open() raise SQLITE_CORRUPT, the error
  // callback will call RazeAndPoison().  Open() will then fail and be
  // retried.  The second Open() on the empty database will succeed
  // cleanly.
  ASSERT_TRUE(db_->Open(db_path_));
  ASSERT_TRUE(db_->Execute("PRAGMA auto_vacuum"));
  EXPECT_EQ(0, SqliteSchemaCount(db_.get()));
}

TEST_P(SQLDatabaseTest, RazeAndPoison_DeletesData) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));
  ASSERT_TRUE(db_->RazeAndPoison());

  // We need to call Close() in order to re-Open().
  db_->Close();
  ASSERT_TRUE(db_->Open(db_path_))
      << "RazeAndPoison() did not produce a healthy database";
  EXPECT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "RazeAndPoison() did not produce a healthy empty database";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_IsOpen) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));
  ASSERT_TRUE(db_->RazeAndPoison());

  EXPECT_FALSE(db_->is_open())
      << "RazeAndPoison() did not mark the database as closed";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_Reopen_NoChanges) {
  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "Execute() should return false after RazeAndPoison()";

  // We need to call Close() in order to re-Open().
  db_->Close();
  ASSERT_TRUE(db_->Open(db_path_))
      << "RazeAndPoison() did not produce a healthy database";
  EXPECT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "Execute() returned false but went through after RazeAndPoison()";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_OpenTransaction) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO rows(id) VALUES(12)"));

  Transaction transaction(db_.get());
  ASSERT_TRUE(transaction.Begin());
  ASSERT_TRUE(db_->RazeAndPoison());

  EXPECT_FALSE(db_->is_open())
      << "RazeAndPoison() did not mark the database as closed";
  EXPECT_FALSE(transaction.Commit())
      << "RazeAndPoison() did not cancel the transaction";

  // We need to call Close() in order to re-Open().
  db_->Close();

  ASSERT_TRUE(db_->Open(db_path_));
  EXPECT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "RazeAndPoison() did not produce a healthy empty database";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_Preload_NoCrash) {
  db_->Preload();
  db_->RazeAndPoison();
  db_->Preload();
}

TEST_P(SQLDatabaseTest, RazeAndPoison_DoesTableExist) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->DoesTableExist("rows")) << "Incorrect test setup";

  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(db_->DoesTableExist("rows"))
      << "DoesTableExist() should return false after RazeAndPoison()";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_IsSQLValid) {
  ASSERT_TRUE(db_->IsSQLValid("SELECT 1")) << "Incorrect test setup";

  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(db_->IsSQLValid("SELECT 1"))
      << "IsSQLValid() should return false after RazeAndPoison()";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_Execute) {
  ASSERT_TRUE(db_->Execute("SELECT 1")) << "Incorrect test setup";

  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(db_->Execute("SELECT 1"))
      << "Execute() should return false after RazeAndPoison()";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_GetUniqueStatement) {
  {
    Statement select(db_->GetUniqueStatement("SELECT 1"));
    ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  }

  ASSERT_TRUE(db_->RazeAndPoison());
  {
    Statement select(db_->GetUniqueStatement("SELECT 1"));
    EXPECT_FALSE(select.Step())
        << "GetUniqueStatement() should return an invalid Statement after "
        << "RazeAndPoison()";
  }
}

TEST_P(SQLDatabaseTest, RazeAndPoison_GetCachedStatement) {
  {
    Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
    ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  }

  ASSERT_TRUE(db_->RazeAndPoison());
  {
    Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
    EXPECT_FALSE(select.Step())
        << "GetCachedStatement() should return an invalid Statement after "
        << "RazeAndPoison()";
  }
}

TEST_P(SQLDatabaseTest, RazeAndPoison_InvalidatesUniqueStatement) {
  Statement select(db_->GetUniqueStatement("SELECT 1"));
  ASSERT_TRUE(select.is_valid()) << "Incorrect test setup";
  ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  select.Reset(/*clear_bound_vars=*/true);

  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(select.is_valid())
      << "RazeAndPoison() should invalidate live Statements";
  EXPECT_FALSE(select.Step())
      << "RazeAndPoison() should invalidate live Statements";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_InvalidatesCachedStatement) {
  Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
  ASSERT_TRUE(select.is_valid()) << "Incorrect test setup";
  ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  select.Reset(/*clear_bound_vars=*/true);

  ASSERT_TRUE(db_->RazeAndPoison());
  EXPECT_FALSE(select.is_valid())
      << "RazeAndPoison() should invalidate live Statements";
  EXPECT_FALSE(select.Step())
      << "RazeAndPoison() should invalidate live Statements";
}

TEST_P(SQLDatabaseTest, RazeAndPoison_TransactionBegin) {
  {
    Transaction transaction(db_.get());
    ASSERT_TRUE(transaction.Begin()) << "Incorrect test setup";
    ASSERT_TRUE(transaction.Commit()) << "Incorrect test setup";
  }

  ASSERT_TRUE(db_->RazeAndPoison());
  {
    Transaction transaction(db_.get());
    EXPECT_FALSE(transaction.Begin())
        << "Transaction::Begin() should return false after RazeAndPoison()";
    EXPECT_FALSE(transaction.IsActiveForTesting())
        << "RazeAndPoison() should block transactions from starting";
  }
}

TEST_P(SQLDatabaseTest, Close_IsSQLValid) {
  ASSERT_TRUE(db_->IsSQLValid("SELECT 1")) << "Incorrect test setup";

  db_->Close();

  EXPECT_DCHECK_DEATH_WITH({ std::ignore = db_->IsSQLValid("SELECT 1"); },
                           "Illegal use of Database without a db");
}

// On Windows, truncate silently fails against a memory-mapped file.  One goal
// of Raze() is to truncate the file to remove blocks which generate I/O errors.
// Test that Raze() turns off memory mapping so that the file is truncated.
// [This would not cover the case of multiple connections where one of the other
// connections is memory-mapped.  That is infrequent in Chromium.]
TEST_P(SQLDatabaseTest, RazeTruncate) {
  // The empty database has 0 or 1 pages.  Raze() should leave it with exactly 1
  // page.  Not checking directly because auto_vacuum on Android adds a freelist
  // page.
  ASSERT_TRUE(db_->Raze());
  int64_t expected_size;
  ASSERT_TRUE(base::GetFileSize(db_path_, &expected_size));
  ASSERT_GT(expected_size, 0);

  // Cause the database to take a few pages.
  static constexpr char kCreateSql[] =
      "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db_->Execute(kCreateSql));
  for (size_t i = 0; i < 24; ++i) {
    ASSERT_TRUE(
        db_->Execute("INSERT INTO foo (value) VALUES (randomblob(1024))"));
  }

  // In WAL mode, writes don't reach the database file until a checkpoint
  // happens.
  ASSERT_TRUE(db_->CheckpointDatabase());

  int64_t db_size;
  ASSERT_TRUE(base::GetFileSize(db_path_, &db_size));
  ASSERT_GT(db_size, expected_size);

  // Make a query covering most of the database file to make sure that the
  // blocks are actually mapped into memory.  Empirically, the truncate problem
  // doesn't seem to happen if no blocks are mapped.
  EXPECT_EQ("24576",
            ExecuteWithResult(db_.get(), "SELECT SUM(LENGTH(value)) FROM foo"));

  ASSERT_TRUE(db_->Raze());
  ASSERT_TRUE(base::GetFileSize(db_path_, &db_size));
  ASSERT_EQ(expected_size, db_size);
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(SQLDatabaseTest, SetTempDirForSQL) {
  MetaTable meta_table;
  // Below call needs a temporary directory in sqlite3
  // On Android, it can pass only when the temporary directory is set.
  // Otherwise, sqlite3 doesn't find the correct directory to store
  // temporary files and will report the error 'unable to open
  // database file'.
  ASSERT_TRUE(meta_table.Init(db_.get(), 4, 4));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_P(SQLDatabaseTest, Delete) {
  EXPECT_TRUE(db_->Execute("CREATE TABLE x (x)"));
  db_->Close();

  base::FilePath journal_path = Database::JournalPath(db_path_);
  base::FilePath wal_path = Database::WriteAheadLogPath(db_path_);

  // Should have both a main database file and a journal file if
  // journal_mode is TRUNCATE. There is no WAL file as it is deleted on Close.
  ASSERT_TRUE(base::PathExists(db_path_));
  if (!IsWALEnabled()) {  // TRUNCATE mode
    ASSERT_TRUE(base::PathExists(journal_path));
  }

  Database::Delete(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));
  EXPECT_FALSE(base::PathExists(journal_path));
  EXPECT_FALSE(base::PathExists(wal_path));
}

#if BUILDFLAG(IS_POSIX)  // This test operates on POSIX file permissions.
TEST_P(SQLDatabaseTest, PosixFilePermissions) {
  db_->Close();
  Database::Delete(db_path_);
  ASSERT_FALSE(base::PathExists(db_path_));

  // If the bots all had a restrictive umask setting such that databases are
  // always created with only the owner able to read them, then the code could
  // break without breaking the tests. Temporarily provide a more permissive
  // umask.
  ScopedUmaskSetter permissive_umask(S_IWGRP | S_IWOTH);

  ASSERT_TRUE(db_->Open(db_path_));

  // Cause the journal file to be created. If the default journal_mode is
  // changed back to DELETE, this test will need to be updated.
  EXPECT_TRUE(db_->Execute("CREATE TABLE x (x)"));

  int mode;
  ASSERT_TRUE(base::PathExists(db_path_));
  EXPECT_TRUE(base::GetPosixFilePermissions(db_path_, &mode));
  ASSERT_EQ(mode, 0600);

  if (IsWALEnabled()) {  // WAL mode
    // The WAL file is created lazily on first change.
    ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));

    base::FilePath wal_path = Database::WriteAheadLogPath(db_path_);
    ASSERT_TRUE(base::PathExists(wal_path));
    EXPECT_TRUE(base::GetPosixFilePermissions(wal_path, &mode));
    ASSERT_EQ(mode, 0600);

    // The shm file doesn't exist in exclusive locking mode.
    if (ExecuteWithResult(db_.get(), "PRAGMA locking_mode") == "normal") {
      base::FilePath shm_path = Database::SharedMemoryFilePath(db_path_);
      ASSERT_TRUE(base::PathExists(shm_path));
      EXPECT_TRUE(base::GetPosixFilePermissions(shm_path, &mode));
      ASSERT_EQ(mode, 0600);
    }
  } else {  // Truncate mode
    base::FilePath journal_path = Database::JournalPath(db_path_);
    DLOG(ERROR) << "journal_path: " << journal_path;
    ASSERT_TRUE(base::PathExists(journal_path));
    EXPECT_TRUE(base::GetPosixFilePermissions(journal_path, &mode));
    ASSERT_EQ(mode, 0600);
  }
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_P(SQLDatabaseTest, Poison_IsOpen) {
  db_->Poison();
  EXPECT_FALSE(db_->is_open())
      << "Poison() did not mark the database as closed";
}

TEST_P(SQLDatabaseTest, Poison_Close_Reopen_NoChanges) {
  db_->Poison();
  EXPECT_FALSE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "Execute() should return false after Poison()";

  db_->Close();
  ASSERT_TRUE(db_->Open(db_path_)) << "Poison() damaged the database";
  EXPECT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"))
      << "Execute() returned false but went through after Poison()";
}

TEST_P(SQLDatabaseTest, Poison_Preload_NoCrash) {
  db_->Preload();
  db_->Poison();
  db_->Preload();
}

TEST_P(SQLDatabaseTest, Poison_DoesTableExist) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->DoesTableExist("rows")) << "Incorrect test setup";

  db_->Poison();
  EXPECT_FALSE(db_->DoesTableExist("rows"))
      << "DoesTableExist() should return false after Poison()";
}

TEST_P(SQLDatabaseTest, Poison_IsSQLValid) {
  ASSERT_TRUE(db_->IsSQLValid("SELECT 1")) << "Incorrect test setup";

  db_->Poison();
  EXPECT_FALSE(db_->IsSQLValid("SELECT 1"))
      << "IsSQLValid() should return false after Poison()";
}

TEST_P(SQLDatabaseTest, Poison_Execute) {
  ASSERT_TRUE(db_->Execute("SELECT 1")) << "Incorrect test setup";

  db_->Poison();
  EXPECT_FALSE(db_->Execute("SELECT 1"))
      << "Execute() should return false after Poison()";
}

TEST_P(SQLDatabaseTest, Poison_GetUniqueStatement) {
  {
    Statement select(db_->GetUniqueStatement("SELECT 1"));
    ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  }

  db_->Poison();
  {
    Statement select(db_->GetUniqueStatement("SELECT 1"));
    EXPECT_FALSE(select.Step())
        << "GetUniqueStatement() should return an invalid Statement after "
        << "Poison()";
  }
}

TEST_P(SQLDatabaseTest, Poison_GetCachedStatement) {
  {
    Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
    ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  }

  db_->Poison();
  {
    Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
    EXPECT_FALSE(select.Step())
        << "GetCachedStatement() should return an invalid Statement after "
        << "Poison()";
  }
}

TEST_P(SQLDatabaseTest, Poison_InvalidatesUniqueStatement) {
  Statement select(db_->GetUniqueStatement("SELECT 1"));
  ASSERT_TRUE(select.is_valid()) << "Incorrect test setup";
  ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  select.Reset(/*clear_bound_vars=*/true);

  db_->Poison();
  EXPECT_FALSE(select.is_valid())
      << "Poison() should invalidate live Statements";
  EXPECT_FALSE(select.Step()) << "Poison() should invalidate live Statements";
}

TEST_P(SQLDatabaseTest, Poison_InvalidatesCachedStatement) {
  Statement select(db_->GetCachedStatement(SQL_FROM_HERE, "SELECT 1"));
  ASSERT_TRUE(select.is_valid()) << "Incorrect test setup";
  ASSERT_TRUE(select.Step()) << "Incorrect test setup";
  select.Reset(/*clear_bound_vars=*/true);

  db_->Poison();
  EXPECT_FALSE(select.is_valid())
      << "Poison() should invalidate live Statements";
  EXPECT_FALSE(select.Step()) << "Poison() should invalidate live Statements";
}

TEST_P(SQLDatabaseTest, Poison_TransactionBegin) {
  {
    Transaction transaction(db_.get());
    ASSERT_TRUE(transaction.Begin()) << "Incorrect test setup";
    ASSERT_TRUE(transaction.Commit()) << "Incorrect test setup";
  }

  db_->Poison();
  {
    Transaction transaction(db_.get());
    EXPECT_FALSE(transaction.Begin())
        << "Transaction::Begin() should return false after Poison()";
    EXPECT_FALSE(transaction.IsActiveForTesting())
        << "Poison() should block transactions from starting";
  }
}

TEST_P(SQLDatabaseTest, Poison_OpenTransaction) {
  Transaction transaction(db_.get());
  ASSERT_TRUE(transaction.Begin());

  db_->Poison();
  EXPECT_FALSE(transaction.Commit())
      << "Poison() did not cancel the transaction";
}

TEST_P(SQLDatabaseTest, AttachDatabase) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));

  // Create a database to attach to.
  base::FilePath attach_path =
      db_path_.DirName().AppendASCII("attach_database_test.db");
  static constexpr char kAttachmentPoint[] = "other";
  {
    Database other_db;
    ASSERT_TRUE(other_db.Open(attach_path));
    ASSERT_TRUE(
        other_db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
    ASSERT_TRUE(other_db.Execute("INSERT INTO rows VALUES(42)"));
  }

  // Cannot see the attached database, yet.
  EXPECT_FALSE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));

  EXPECT_TRUE(db_->AttachDatabase(attach_path, kAttachmentPoint));
  EXPECT_TRUE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));

  // Queries can touch both databases after the ATTACH.
  EXPECT_TRUE(db_->Execute("INSERT INTO rows SELECT id FROM other.rows"));
  {
    Statement select(db_->GetUniqueStatement("SELECT COUNT(*) FROM rows"));
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(1, select.ColumnInt(0));
  }

  EXPECT_TRUE(db_->DetachDatabase(kAttachmentPoint));
  EXPECT_FALSE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));
}

TEST_P(SQLDatabaseTest, AttachDatabaseWithOpenTransaction) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));

  // Create a database to attach to.
  base::FilePath attach_path =
      db_path_.DirName().AppendASCII("attach_database_test.db");
  static constexpr char kAttachmentPoint[] = "other";
  {
    Database other_db;
    ASSERT_TRUE(other_db.Open(attach_path));
    ASSERT_TRUE(
        other_db.Execute("CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL)"));
    ASSERT_TRUE(other_db.Execute("INSERT INTO rows VALUES(42)"));
  }

  // Cannot see the attached database, yet.
  EXPECT_FALSE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));

  // Attach succeeds in a transaction.
  Transaction transaction(db_.get());
  EXPECT_TRUE(transaction.Begin());
  EXPECT_TRUE(db_->AttachDatabase(attach_path, kAttachmentPoint));
  EXPECT_TRUE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));

  // Queries can touch both databases after the ATTACH.
  EXPECT_TRUE(db_->Execute("INSERT INTO rows SELECT id FROM other.rows"));
  {
    Statement select(db_->GetUniqueStatement("SELECT COUNT(*) FROM rows"));
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(1, select.ColumnInt(0));
  }

  // Detaching the same database fails, database is locked in the transaction.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(db_->DetachDatabase(kAttachmentPoint));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
  EXPECT_TRUE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));

  // Detach succeeds when the transaction is closed.
  transaction.Rollback();
  EXPECT_TRUE(db_->DetachDatabase(kAttachmentPoint));
  EXPECT_FALSE(db_->IsSQLValid("SELECT COUNT(*) from other.rows"));
}

TEST_P(SQLDatabaseTest, FullIntegrityCheck) {
  static constexpr char kTableSql[] =
      "CREATE TABLE rows(id INTEGER PRIMARY KEY NOT NULL, value TEXT NOT NULL)";
  ASSERT_TRUE(db_->Execute(kTableSql));
  ASSERT_TRUE(db_->Execute("CREATE INDEX rows_by_value ON rows(value)"));

  {
    std::vector<std::string> messages;
    EXPECT_TRUE(db_->FullIntegrityCheck(&messages))
        << "FullIntegrityCheck() failed before database was corrupted";
    EXPECT_THAT(messages, testing::ElementsAre("ok"))
        << "FullIntegrityCheck() should report ok before database is corrupted";
  }

  db_->Close();
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(db_path_, "rows_by_value"));
  ASSERT_TRUE(db_->Open(db_path_));

  {
    std::vector<std::string> messages;
    EXPECT_TRUE(db_->FullIntegrityCheck(&messages))
        << "FullIntegrityCheck() failed on corrupted database";
    EXPECT_THAT(messages, testing::Not(testing::ElementsAre("ok")))
        << "FullIntegrityCheck() should not report ok for a corrupted database";
  }
}

TEST_P(SQLDatabaseTest, OnMemoryDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  ASSERT_TRUE(db_->memory_dump_provider_->OnMemoryDump(args, &pmd));
  EXPECT_GE(pmd.allocator_dumps().size(), 1u);
}

// Test that the functions to collect diagnostic data run to completion, without
// worrying too much about what they generate (since that will change).
TEST_P(SQLDatabaseTest, CollectDiagnosticInfo) {
  const std::string corruption_info = db_->CollectCorruptionInfo();
  EXPECT_TRUE(base::Contains(corruption_info, "SQLITE_CORRUPT"));
  EXPECT_TRUE(base::Contains(corruption_info, "integrity_check"));

  // A statement to see in the results.
  static constexpr char kSimpleSql[] = "SELECT 'mountain'";
  Statement s(db_->GetCachedStatement(SQL_FROM_HERE, kSimpleSql));

  // Error includes the statement.
  {
    DatabaseDiagnostics diagnostics;
    const std::string readonly_info =
        db_->CollectErrorInfo(SQLITE_READONLY, &s, &diagnostics);
    EXPECT_TRUE(base::Contains(readonly_info, kSimpleSql));
    EXPECT_EQ(diagnostics.sql_statement, kSimpleSql);
  }

  // Some other error doesn't include the statement.
  {
    DatabaseDiagnostics diagnostics;
    const std::string full_info =
        db_->CollectErrorInfo(SQLITE_FULL, nullptr, &diagnostics);
    EXPECT_FALSE(base::Contains(full_info, kSimpleSql));
    EXPECT_TRUE(diagnostics.sql_statement.empty());
  }

  // A table to see in the SQLITE_ERROR results.
  EXPECT_TRUE(db_->Execute("CREATE TABLE volcano (x)"));

  // Version info to see in the SQLITE_ERROR results.
  MetaTable meta_table;
  ASSERT_TRUE(meta_table.Init(db_.get(), 4, 4));

  {
    DatabaseDiagnostics diagnostics;
    const std::string error_info =
        db_->CollectErrorInfo(SQLITE_ERROR, &s, &diagnostics);
    EXPECT_TRUE(base::Contains(error_info, kSimpleSql));
    EXPECT_TRUE(base::Contains(error_info, "volcano"));
    EXPECT_TRUE(base::Contains(error_info, "version: 4"));
    EXPECT_EQ(diagnostics.sql_statement, kSimpleSql);
    EXPECT_EQ(diagnostics.version, 4);

    ASSERT_EQ(diagnostics.schema_sql_rows.size(), 2U);
    EXPECT_EQ(diagnostics.schema_sql_rows[0], "CREATE TABLE volcano (x)");
    EXPECT_EQ(diagnostics.schema_sql_rows[1],
              "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, "
              "value LONGVARCHAR)");

    ASSERT_EQ(diagnostics.schema_other_row_names.size(), 1U);
    EXPECT_EQ(diagnostics.schema_other_row_names[0], "sqlite_autoindex_meta_1");
  }

  // Test that an error message is included in the diagnostics.
  {
    sql::test::ScopedErrorExpecter error_expecter;
    error_expecter.ExpectError(SQLITE_ERROR);
    EXPECT_FALSE(
        db_->Execute("INSERT INTO volcano VALUES ('bound_value1', 42, 1234)"));
    EXPECT_TRUE(error_expecter.SawExpectedErrors());

    DatabaseDiagnostics diagnostics;
    const std::string error_info =
        db_->CollectErrorInfo(SQLITE_ERROR, &s, &diagnostics);
    // Expect that the error message contains the table name and a column error.
    EXPECT_TRUE(base::Contains(diagnostics.error_message, "table"));
    EXPECT_TRUE(base::Contains(diagnostics.error_message, "volcano"));
    EXPECT_TRUE(base::Contains(diagnostics.error_message, "column"));

    // Expect that bound values are not present.
    EXPECT_FALSE(base::Contains(diagnostics.error_message, "bound_value1"));
    EXPECT_FALSE(base::Contains(diagnostics.error_message, "42"));
    EXPECT_FALSE(base::Contains(diagnostics.error_message, "1234"));
  }
}

// Test that a fresh database has mmap enabled by default, if mmap'ed I/O is
// enabled by SQLite.
TEST_P(SQLDatabaseTest, MmapInitiallyEnabled) {
  {
    Statement s(db_->GetUniqueStatement("PRAGMA mmap_size"));
    ASSERT_TRUE(s.Step())
        << "All supported SQLite versions should have mmap support";

    // If mmap I/O is not on, attempt to turn it on.  If that succeeds, then
    // Open() should have turned it on.  If mmap support is disabled, 0 is
    // returned.  If the VFS does not understand SQLITE_FCNTL_MMAP_SIZE (for
    // instance MojoVFS), -1 is returned.
    if (s.ColumnInt(0) <= 0) {
      ASSERT_TRUE(db_->Execute("PRAGMA mmap_size = 1048576"));
      s.Reset(true);
      ASSERT_TRUE(s.Step());
      EXPECT_LE(s.ColumnInt(0), 0);
    }
  }

  // Test that explicit disable prevents mmap'ed I/O.
  db_->Close();
  Database::Delete(db_path_);
  db_->set_mmap_disabled();
  ASSERT_TRUE(db_->Open(db_path_));
  EXPECT_EQ("0", ExecuteWithResult(db_.get(), "PRAGMA mmap_size"));
}

// Test whether a fresh database gets mmap enabled when using alternate status
// storage.
TEST_P(SQLDatabaseTest, MmapInitiallyEnabledAltStatus) {
  // Re-open fresh database with alt-status flag set.
  db_->Close();
  Database::Delete(db_path_);

  DatabaseOptions options = GetDBOptions();
  options.mmap_alt_status_discouraged = true;
  options.enable_views_discouraged = true;
  db_ = std::make_unique<Database>(options);
  ASSERT_TRUE(db_->Open(db_path_));

  {
    Statement s(db_->GetUniqueStatement("PRAGMA mmap_size"));
    ASSERT_TRUE(s.Step())
        << "All supported SQLite versions should have mmap support";

    // If mmap I/O is not on, attempt to turn it on.  If that succeeds, then
    // Open() should have turned it on.  If mmap support is disabled, 0 is
    // returned.  If the VFS does not understand SQLITE_FCNTL_MMAP_SIZE (for
    // instance MojoVFS), -1 is returned.
    if (s.ColumnInt(0) <= 0) {
      ASSERT_TRUE(db_->Execute("PRAGMA mmap_size = 1048576"));
      s.Reset(true);
      ASSERT_TRUE(s.Step());
      EXPECT_LE(s.ColumnInt(0), 0);
    }
  }

  // Test that explicit disable overrides set_mmap_alt_status().
  db_->Close();
  Database::Delete(db_path_);
  db_->set_mmap_disabled();
  ASSERT_TRUE(db_->Open(db_path_));
  EXPECT_EQ("0", ExecuteWithResult(db_.get(), "PRAGMA mmap_size"));
}

TEST_P(SQLDatabaseTest, ComputeMmapSizeForOpen) {
  const size_t kMmapAlot = 25 * 1024 * 1024;
  int64_t mmap_status = MetaTable::kMmapFailure;

  // If there is no meta table (as for a fresh database), assume that everything
  // should be mapped, and the status of the meta table is not affected.
  ASSERT_TRUE(!db_->DoesTableExist("meta"));
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_TRUE(!db_->DoesTableExist("meta"));

  // When the meta table is first created, it sets up to map everything.
  ASSERT_TRUE(MetaTable().Init(db_.get(), 1, 1));
  ASSERT_TRUE(db_->DoesTableExist("meta"));
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_TRUE(MetaTable::GetMmapStatus(db_.get(), &mmap_status));
  ASSERT_EQ(MetaTable::kMmapSuccess, mmap_status);

  // Preload with partial progress of one page.  Should map everything.
  ASSERT_TRUE(db_->Execute("REPLACE INTO meta VALUES ('mmap_status', 1)"));
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_TRUE(MetaTable::GetMmapStatus(db_.get(), &mmap_status));
  ASSERT_EQ(MetaTable::kMmapSuccess, mmap_status);

  // Failure status maps nothing.
  ASSERT_TRUE(db_->Execute("REPLACE INTO meta VALUES ('mmap_status', -2)"));
  ASSERT_EQ(0UL, db_->ComputeMmapSizeForOpen());

  // Re-initializing the meta table does not re-create the key if the table
  // already exists.
  ASSERT_TRUE(db_->Execute("DELETE FROM meta WHERE key = 'mmap_status'"));
  ASSERT_TRUE(MetaTable().Init(db_.get(), 1, 1));
  ASSERT_EQ(MetaTable::kMmapSuccess, mmap_status);
  ASSERT_TRUE(MetaTable::GetMmapStatus(db_.get(), &mmap_status));
  ASSERT_EQ(0, mmap_status);

  // With no key, map everything and create the key.
  // TODO(shess): This really should be "maps everything after validating it",
  // but that is more complicated to structure.
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_TRUE(MetaTable::GetMmapStatus(db_.get(), &mmap_status));
  ASSERT_EQ(MetaTable::kMmapSuccess, mmap_status);
}

TEST_P(SQLDatabaseTest, ComputeMmapSizeForOpenAltStatus) {
  const size_t kMmapAlot = 25 * 1024 * 1024;

  // At this point, Database still expects a future [meta] table.
  ASSERT_FALSE(db_->DoesTableExist("meta"));
  ASSERT_FALSE(db_->DoesViewExist("MmapStatus"));
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_FALSE(db_->DoesTableExist("meta"));
  ASSERT_FALSE(db_->DoesViewExist("MmapStatus"));

  // Using alt status, everything should be mapped, with state in the view.
  DatabaseOptions options = GetDBOptions();
  options.mmap_alt_status_discouraged = true;
  options.enable_views_discouraged = true;
  db_ = std::make_unique<Database>(options);
  ASSERT_TRUE(db_->Open(db_path_));

  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  ASSERT_FALSE(db_->DoesTableExist("meta"));
  ASSERT_TRUE(db_->DoesViewExist("MmapStatus"));
  EXPECT_EQ(base::NumberToString(MetaTable::kMmapSuccess),
            ExecuteWithResult(db_.get(), "SELECT * FROM MmapStatus"));

  // Also maps everything when kMmapSuccess is already in the view.
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);

  // Preload with partial progress of one page.  Should map everything.
  ASSERT_TRUE(db_->Execute("DROP VIEW MmapStatus"));
  ASSERT_TRUE(db_->Execute("CREATE VIEW MmapStatus (value) AS SELECT 1"));
  ASSERT_GT(db_->ComputeMmapSizeForOpen(), kMmapAlot);
  EXPECT_EQ(base::NumberToString(MetaTable::kMmapSuccess),
            ExecuteWithResult(db_.get(), "SELECT * FROM MmapStatus"));

  // Failure status leads to nothing being mapped.
  ASSERT_TRUE(db_->Execute("DROP VIEW MmapStatus"));
  ASSERT_TRUE(db_->Execute("CREATE VIEW MmapStatus (value) AS SELECT -2"));
  ASSERT_EQ(0UL, db_->ComputeMmapSizeForOpen());
  EXPECT_EQ(base::NumberToString(MetaTable::kMmapFailure),
            ExecuteWithResult(db_.get(), "SELECT * FROM MmapStatus"));
}

TEST_P(SQLDatabaseTest, GetMemoryUsage) {
  // Databases with mmap enabled may not follow the assumptions below.
  db_->Close();
  db_->set_mmap_disabled();
  ASSERT_TRUE(db_->Open(db_path_));

  int initial_memory = db_->GetMemoryUsage();
  EXPECT_GT(initial_memory, 0)
      << "SQLite should always use some memory for a database";

  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo(a, b) VALUES (12, 13)"));

  int post_query_memory = db_->GetMemoryUsage();
  EXPECT_GT(post_query_memory, initial_memory)
      << "Page cache usage should go up after executing queries";

  db_->TrimMemory();
  int post_trim_memory = db_->GetMemoryUsage();
  EXPECT_GT(post_query_memory, post_trim_memory)
      << "Page cache usage should go down after calling TrimMemory()";
}

TEST_P(SQLDatabaseTest, DoubleQuotedStringLiteralsDisabledByDefault) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE data(item TEXT NOT NULL);"));

  struct TestCase {
    const std::string sql;
    bool is_valid;
  };
  std::vector<TestCase> test_cases = {
      // DML tests.
      {"SELECT item FROM data WHERE item >= 'string literal'", true},
      {"SELECT item FROM data WHERE item >= \"string literal\"", false},
      {"INSERT INTO data(item) VALUES('string literal')", true},
      {"INSERT INTO data(item) VALUES(\"string literal\")", false},
      {"UPDATE data SET item = 'string literal'", true},
      {"UPDATE data SET item = \"string literal\"", false},
      {"DELETE FROM data WHERE item >= 'string literal'", true},
      {"DELETE FROM data WHERE item >= \"string literal\"", false},

      // DDL tests.
      {"CREATE INDEX data_item ON data(item) WHERE item >= 'string literal'",
       true},
      {"CREATE INDEX data_item ON data(item) WHERE item >= \"string literal\"",
       false},
      {"CREATE TABLE data2(item TEXT DEFAULT 'string literal')", true},

      // This should be an invalid DDL statement, due to the double-quoted
      // string literal. However, SQLite currently parses it.
      {"CREATE TABLE data2(item TEXT DEFAULT \"string literal\")", true},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.sql);

    EXPECT_EQ(test_case.is_valid, db_->IsSQLValid(test_case.sql));
  }
}

TEST_P(SQLDatabaseTest, ForeignKeyEnforcementDisabledByDefault) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE targets(id INTEGER PRIMARY KEY)"));
  // sqlite3_db_config() currently only disables foreign key enforcement. Schema
  // operations on foreign keys are still allowed.
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE refs("
                   "id INTEGER PRIMARY KEY,"
                   "target_id INTEGER REFERENCES targets(id))"));

  ASSERT_TRUE(db_->Execute("INSERT INTO targets(id) VALUES(42)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO refs(id, target_id) VALUES(42, 42)"));

  EXPECT_TRUE(db_->Execute("DELETE FROM targets WHERE id=42"))
      << "Foreign key enforcement is not disabled";
}

TEST_P(SQLDatabaseTest, TriggersDisabledByDefault) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE data(id INTEGER)"));

  // sqlite3_db_config() currently only disables running triggers. Schema
  // operations on triggers are still allowed.
  EXPECT_TRUE(
      db_->Execute("CREATE TRIGGER trigger AFTER INSERT ON data "
                   "BEGIN DELETE FROM data; END"));

  ASSERT_TRUE(db_->Execute("INSERT INTO data(id) VALUES(42)"));

  Statement select(db_->GetUniqueStatement("SELECT id FROM data"));
  EXPECT_TRUE(select.Step())
      << "If the trigger did not run, the table should not be empty.";
  EXPECT_EQ(42, select.ColumnInt64(0));

  // sqlite3_db_config() currently only disables running triggers. Schema
  // operations on triggers are still allowed.
  EXPECT_TRUE(db_->Execute("DROP TRIGGER IF EXISTS trigger"));
}

// This test ensures that a database can be open/create with a journal mode and
// can be re-open later with a different journal mode.
TEST_P(SQLDatabaseTest, ReOpenWithDifferentJournalMode) {
  const bool is_wal = IsWALEnabled();
  const base::FilePath journal_path = Database::JournalPath(db_path_);
  const base::FilePath wal_path = Database::WriteAheadLogPath(db_path_);

  ASSERT_TRUE(db_->Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, value)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo (value) VALUES (12)"));

  // Last insert row ID should be valid.
  int64_t row = db_->GetLastInsertRowId();
  EXPECT_LT(0, row);

  // It should be the primary key of the row we just inserted.
  {
    Statement s(db_->GetUniqueStatement("SELECT value FROM foo WHERE id=?"));
    s.BindInt64(0, row);
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // Ensure appropriate journal mode and the journal file exists.
  EXPECT_TRUE(IsOpenedInCorrectJournalMode(db_.get(), is_wal));
  EXPECT_EQ(base::PathExists(wal_path), is_wal);

  db_->Close();
  if (is_wal) {
    // The WAL journal file is removed on database close. Database that enable
    // WAL mode can use a different journal mode on a subsequent database open.
    EXPECT_FALSE(base::PathExists(wal_path));
  } else {
    // The Rollback journal should have a zero size when pending operations
    // are completed.
    int64_t journal_size = 0;
    base::GetFileSize(journal_path, &journal_size);
    EXPECT_EQ(journal_size, 0);
  }

  // Re-open the database with a different mode (Rollback vs WAL).
  DatabaseOptions options = GetDBOptions();
  options.wal_mode = !is_wal;
#if BUILDFLAG(IS_FUCHSIA)
  // Exclusive mode needs to be enabled to enter WAL mode on Fuchsia.
  if (options.wal_mode) {
    options.exclusive_locking = true;
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  db_ = std::make_unique<Database>(options);
  ASSERT_TRUE(db_->Open(db_path_));

  // The value for the last inserted row should be valid.
  {
    Statement s(db_->GetUniqueStatement("SELECT value FROM foo WHERE id=?"));
    s.BindInt64(0, row);
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // Ensure appropriate journal file exists.
  EXPECT_TRUE(IsOpenedInCorrectJournalMode(db_.get(), options.wal_mode));
  EXPECT_EQ(base::PathExists(wal_path), options.wal_mode);
}

#if BUILDFLAG(IS_WIN)

class SQLDatabaseTestExclusiveFileLockMode
    : public testing::Test,
      public testing::WithParamInterface<::testing::tuple<bool, bool>> {
 public:
  ~SQLDatabaseTestExclusiveFileLockMode() override = default;

  void SetUp() override {
    db_ = std::make_unique<Database>(GetDBOptions());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("maybelocked.sqlite");
    ASSERT_TRUE(db_->Open(db_path_));
  }

  DatabaseOptions GetDBOptions() {
    DatabaseOptions options;
    options.wal_mode = IsWALEnabled();
    options.exclusive_locking = true;
    options.exclusive_database_file_lock = IsExclusivelockEnabled();
    return options;
  }

  bool IsWALEnabled() { return std::get<0>(GetParam()); }
  bool IsExclusivelockEnabled() { return std::get<1>(GetParam()); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<Database> db_;
};

TEST_P(SQLDatabaseTestExclusiveFileLockMode, BasicStatement) {
  ASSERT_TRUE(db_->Execute("CREATE TABLE data(contents TEXT)"));
  EXPECT_EQ(SQLITE_OK, db_->GetErrorCode());

  ASSERT_TRUE(base::PathExists(db_path_));
  base::File open_db(db_path_, base::File::Flags::FLAG_OPEN_ALWAYS |
                                   base::File::Flags::FLAG_READ);

  // If exclusive lock is enabled, then the test should not be able to re-open
  // the database file, on Windows only.
  EXPECT_EQ(IsExclusivelockEnabled(), !open_db.IsValid());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SQLDatabaseTestExclusiveFileLockMode,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](const auto& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "WALEnabled" : "WALDisabled",
           std::get<1>(info.param) ? "ExclusiveLock" : "NoExclusiveLock"});
    });

#else

TEST(SQLInvalidDatabaseFlagsDeathTest, ExclusiveDatabaseLock) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto db_path = temp_dir.GetPath().AppendASCII("database_test_locked.sqlite");

  Database db({.exclusive_database_file_lock = true});

  EXPECT_CHECK_DEATH_WITH(
      { std::ignore = db.Open(db_path); },
      "exclusive_database_file_lock is only supported on Windows");
}

#endif  // BUILDFLAG(IS_WIN)

class SQLDatabaseTestExclusiveMode : public testing::Test,
                                     public testing::WithParamInterface<bool> {
 public:
  ~SQLDatabaseTestExclusiveMode() override = default;

  void SetUp() override {
    db_ = std::make_unique<Database>(GetDBOptions());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("recovery_test.sqlite");
    ASSERT_TRUE(db_->Open(db_path_));
  }

  DatabaseOptions GetDBOptions() {
    DatabaseOptions options;
    options.wal_mode = IsWALEnabled();
    options.exclusive_locking = true;
    return options;
  }

  bool IsWALEnabled() { return GetParam(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<Database> db_;
};

TEST_P(SQLDatabaseTestExclusiveMode, LockingModeExclusive) {
  EXPECT_EQ(ExecuteWithResult(db_.get(), "PRAGMA locking_mode"), "exclusive");
}

TEST_P(SQLDatabaseTest, LockingModeNormal) {
  EXPECT_EQ(ExecuteWithResult(db_.get(), "PRAGMA locking_mode"), "normal");
}

TEST_P(SQLDatabaseTest, OpenedInCorrectMode) {
  EXPECT_TRUE(IsOpenedInCorrectJournalMode(db_.get(), IsWALEnabled()));
}

TEST_P(SQLDatabaseTest, CheckpointDatabase) {
  if (!IsWALEnabled())
    return;

  base::FilePath wal_path = Database::WriteAheadLogPath(db_path_);

  int64_t wal_size = 0;
  // WAL file initially empty.
  EXPECT_TRUE(base::PathExists(wal_path));
  base::GetFileSize(wal_path, &wal_size);
  EXPECT_EQ(wal_size, 0);

  ASSERT_TRUE(
      db_->Execute("CREATE TABLE foo (id INTEGER UNIQUE, value INTEGER)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo VALUES (1, 1)"));
  ASSERT_TRUE(db_->Execute("INSERT INTO foo VALUES (2, 2)"));

  // Writes reach WAL file but not db file.
  base::GetFileSize(wal_path, &wal_size);
  EXPECT_GT(wal_size, 0);

  int64_t db_size = 0;
  base::GetFileSize(db_path_, &db_size);
  EXPECT_EQ(db_size, db_->page_size());

  // Checkpoint database to immediately propagate writes to DB file.
  EXPECT_TRUE(db_->CheckpointDatabase());

  base::GetFileSize(db_path_, &db_size);
  EXPECT_GT(db_size, db_->page_size());
  EXPECT_EQ(ExecuteWithResult(db_.get(), "SELECT value FROM foo where id=1"),
            "1");
  EXPECT_EQ(ExecuteWithResult(db_.get(), "SELECT value FROM foo where id=2"),
            "2");
}

TEST_P(SQLDatabaseTest, OpenFailsAfterCorruptSizeInHeader) {
  // The database file ends up empty if we don't create at least one table.
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(i INTEGER PRIMARY KEY NOT NULL)"));
  db_->Close();

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_FALSE(db_->Open(db_path_));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_P(SQLDatabaseTest, OpenWithRecoveryHandlesCorruption) {
  for (const bool corrupt_after_recovery : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "corrupt_after_recovery: " << corrupt_after_recovery);
    // Ensure that `db_` is fresh in this iteration.
    CreateFreshDB();
    // The database file ends up empty if we don't create at least one table.
    ASSERT_TRUE(
        db_->Execute("CREATE TABLE rows(i INTEGER PRIMARY KEY NOT NULL)"));
    db_->Close();

    ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));

    size_t error_count = 0;
    auto callback = base::BindLambdaForTesting([&](int error, Statement* stmt) {
      error_count++;
      ASSERT_TRUE(Recovery::RecoverIfPossible(
          db_.get(), error, sql::Recovery::Strategy::kRecoverOrRaze));
      if (corrupt_after_recovery) {
        // Corrupt the file again after temporarily recovering it.
        ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));
      }
    });
    db_->set_error_callback(std::move(callback));

    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(SQLITE_CORRUPT);

      // When `corrupt_after_recovery` is true, `Database::Open()` will return
      // false because both attempts at opening the database will fail. When the
      // database is *not* corrupted after recovery, recovery will succeed and
      // thus `Database::Open()`'s second attempt at opening the database will
      // succeed.
      ASSERT_EQ(db_->Open(db_path_), !corrupt_after_recovery);
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ(error_count, 1u);
    EXPECT_FALSE(db_->has_error_callback());
  }
}

TEST_P(SQLDatabaseTest, ExecuteFailsAfterCorruptSizeInHeader) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(i INTEGER PRIMARY KEY NOT NULL)"));
  constexpr static char kSelectSql[] = "SELECT * from rows";
  EXPECT_TRUE(db_->Execute(kSelectSql))
      << "The test Execute() statement fails before the header is corrupted";
  db_->Close();

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_FALSE(db_->Open(db_path_));
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Database::Open() did not encounter SQLITE_CORRUPT";
  }
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(db_->Execute(kSelectSql));
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Database::Execute() did not encounter SQLITE_CORRUPT";
  }
}

TEST_P(SQLDatabaseTest, SchemaFailsAfterCorruptSizeInHeader) {
  ASSERT_TRUE(
      db_->Execute("CREATE TABLE rows(i INTEGER PRIMARY KEY NOT NULL)"));
  ASSERT_TRUE(db_->DoesTableExist("rows"))
      << "The test schema check fails before the header is corrupted";
  db_->Close();

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path_));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ASSERT_FALSE(db_->Open(db_path_));
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Database::Open() did not encounter SQLITE_CORRUPT";
  }
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    EXPECT_FALSE(db_->DoesTableExist("rows"));
    EXPECT_TRUE(expecter.SawExpectedErrors())
        << "Database::DoesTableExist() did not encounter SQLITE_CORRUPT";
  }
}

TEST(SQLEmptyPathDatabaseTest, EmptyPathTest) {
  Database db;
  EXPECT_TRUE(db.OpenInMemory());
  EXPECT_TRUE(db.is_open());
  EXPECT_TRUE(db.DbPath().empty());
}

// WAL mode is currently not supported on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
INSTANTIATE_TEST_SUITE_P(JournalMode, SQLDatabaseTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(JournalMode,
                         SQLDatabaseTestExclusiveMode,
                         testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(JournalMode, SQLDatabaseTest, testing::Values(false));
INSTANTIATE_TEST_SUITE_P(JournalMode,
                         SQLDatabaseTestExclusiveMode,
                         testing::Values(false));
#endif
}  // namespace sql
