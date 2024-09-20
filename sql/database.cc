// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sql/database.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cinttypes>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"  // IWYU pragma: keep
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "sql/database_memory_dump_provider.h"
#include "sql/initialization.h"
#include "sql/internal_api_token.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/contains.h"
#endif

namespace sql {

namespace {

bool enable_mmap_by_default_ = true;

// The name of the main database associated with a sqlite3* connection.
//
// SQLite has the ability to ATTACH multiple databases to the same connection.
// As a consequence, some SQLite APIs require the connection-specific database
// name. This is the right name to be passed to such APIs.
static constexpr char kSqliteMainDatabaseName[] = "main";

// Magic path value telling sqlite3_open_v2() to open an in-memory database.
static constexpr char kSqliteOpenInMemoryPath[] = ":memory:";

// Spin for up to a second waiting for the lock to clear when setting
// up the database.
// TODO(shess): Better story on this.  http://crbug.com/56559
const int kBusyTimeoutSeconds = 1;

// RAII-style wrapper that enables `writable_schema` until it goes out of scope.
// No error checking on the PRAGMA statements because it is reasonable to just
// forge ahead in case of an error. If turning it on fails, then most likely
// nothing will work, whereas if turning it off fails, it only matters if some
// code attempts to continue working with the database and tries to modify the
// sqlite_schema table (none of our code does this).
class ScopedWritableSchema {
 public:
  explicit ScopedWritableSchema(base::WeakPtr<Database> db)
      : db_(std::move(db)) {
    CHECK(db_->is_open());
    std::ignore = db_->Execute("PRAGMA writable_schema=1");
  }
  ~ScopedWritableSchema() {
    // Database invalidates its WeakPtrs before closing the SQLite connection.
    if (db_) {
      CHECK(db_->is_open());
      std::ignore = db_->Execute("PRAGMA writable_schema=0");
    }
  }

 private:
  const base::WeakPtr<Database> db_;
};

// Raze() helper that uses SQLite's online backup API.
//
// Returns the SQLite error code produced by sqlite3_backup_step(). SQLITE_DONE
// signals success. SQLITE_OK will never be returned.
//
// The implementation is tailored for the Raze() use case. In particular, the
// SQLite API use and and error handling is optimized for 1-page databases.
SqliteResultCode BackupDatabaseForRaze(sqlite3* source_db,
                                       sqlite3* destination_db) {
  DCHECK(source_db);
  DCHECK(destination_db);
  DCHECK_NE(source_db, destination_db);

  // https://www.sqlite.org/backup.html has a high-level overview of SQLite's
  // backup support. https://www.sqlite.org/c3ref/backup_finish.html describes
  // the API.
  static constexpr char kMainDatabaseName[] = "main";
  sqlite3_backup* backup = sqlite3_backup_init(
      destination_db, kMainDatabaseName, source_db, kMainDatabaseName);
  if (!backup) {
    // sqlite3_backup_init() fails if a transaction is ongoing. In particular,
    // SQL statements that return multiple rows keep a read transaction open
    // until all the Step() calls are executed.
    return ToSqliteResultCode(chrome_sqlite3_extended_errcode(destination_db));
  }

  constexpr int kUnlimitedPageCount = -1;  // Back up entire database.
  auto sqlite_result_code =
      ToSqliteResultCode(sqlite3_backup_step(backup, kUnlimitedPageCount));
  DCHECK_NE(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_backup_step() returned SQLITE_OK (instead of SQLITE_DONE) "
      << "when asked to back up the entire database";

#if DCHECK_IS_ON()
  if (sqlite_result_code == SqliteResultCode::kDone) {
    // If successful, exactly one page should have been backed up.
    DCHECK_EQ(sqlite3_backup_pagecount(backup), 1)
        << __func__ << " was intended to be used with 1-page databases";
  }
#endif  // DCHECK_IS_ON()

  // sqlite3_backup_finish() releases the sqlite3_backup object.
  //
  // It returns an error code only if the backup encountered a permanent error.
  // We use the the sqlite3_backup_step() result instead, because it also tells
  // us about temporary errors, like SQLITE_BUSY.
  //
  // We pass the sqlite3_backup_finish() result code through
  // ToSqliteResultCode() to catch codes that should never occur, like
  // SQLITE_MISUSE.
  std::ignore = ToSqliteResultCode(sqlite3_backup_finish(backup));

  return sqlite_result_code;
}

bool ValidAttachmentPoint(std::string_view attachment_point) {
  // SQLite could handle a much wider character set, with appropriate quoting.
  //
  // Chrome's constraint is easy to remember, and sufficient for the few
  // existing use cases. ATTACH is a discouraged feature, so no new use cases
  // are expected.
  return base::ranges::all_of(attachment_point,
                              [](char ch) { return base::IsAsciiLower(ch); });
}

std::string AsUTF8ForSQL(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(path.value());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return path.value();
#endif
}

}  // namespace

// static
Database::ScopedErrorExpecterCallback* Database::current_expecter_cb_ = nullptr;

// static
bool Database::IsExpectedSqliteError(int sqlite_error_code) {
  DCHECK_NE(sqlite_error_code, SQLITE_OK)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_DONE)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_ROW)
      << __func__ << " received non-error result code";

  if (!current_expecter_cb_) {
    return false;
  }
  return current_expecter_cb_->Run(sqlite_error_code);
}

// static
void Database::SetScopedErrorExpecter(
    Database::ScopedErrorExpecterCallback* cb,
    base::PassKey<test::ScopedErrorExpecter>) {
  CHECK(!current_expecter_cb_);
  current_expecter_cb_ = cb;
}

// static
void Database::ResetScopedErrorExpecter(
    base::PassKey<test::ScopedErrorExpecter>) {
  CHECK(current_expecter_cb_);
  current_expecter_cb_ = nullptr;
}

// static
base::FilePath Database::JournalPath(const base::FilePath& db_path) {
  return base::FilePath(db_path.value() + FILE_PATH_LITERAL("-journal"));
}

// static
base::FilePath Database::WriteAheadLogPath(const base::FilePath& db_path) {
  return base::FilePath(db_path.value() + FILE_PATH_LITERAL("-wal"));
}

// static
base::FilePath Database::SharedMemoryFilePath(const base::FilePath& db_path) {
  return base::FilePath(db_path.value() + FILE_PATH_LITERAL("-shm"));
}

base::WeakPtr<Database> Database::GetWeakPtr(InternalApiToken) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

Database::StatementRef::StatementRef(Database* database,
                                     sqlite3_stmt* stmt,
                                     bool was_valid)
    : database_(database), stmt_(stmt), was_valid_(was_valid) {
  DCHECK_EQ(database == nullptr, stmt == nullptr);
  if (database) {
    database_->StatementRefCreated(this);
  }
}

Database::StatementRef::~StatementRef() {
  if (database_) {
    database_->StatementRefDeleted(this);
  }
  Close(false);
}

void Database::StatementRef::Close(bool forced) {
  if (stmt_) {
    // Call to InitScopedBlockingCall() cannot go at the beginning of the
    // function because Close() is called unconditionally from destructor to
    // clean database_. And if this is inactive statement this won't cause any
    // disk access and destructor most probably will be called on thread not
    // allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    std::optional<base::ScopedBlockingCall> scoped_blocking_call;
    InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

    // `stmt_` references memory loaned from the sqlite3 library. Stop
    // referencing it from the raw_ptr<> before returning it. This avoids the
    // raw_ptr<> becoming dangling.
    sqlite3_stmt* statement = stmt_;
    stmt_ = nullptr;

    // sqlite3_finalize()'s result code is ignored because it reports the same
    // error as the most recent sqlite3_step(). The result code is passed
    // through ToSqliteResultCode() to catch issues like SQLITE_MISUSE.
    std::ignore = ToSqliteResultCode(sqlite3_finalize(statement));
  }
  database_ = nullptr;  // The Database may be getting deleted.

  // Forced close is expected to happen from a statement error
  // handler.  In that case maintain the sense of |was_valid_| which
  // previously held for this ref.
  was_valid_ = was_valid_ && forced;
}

static_assert(DatabaseOptions::kDefaultPageSize == SQLITE_DEFAULT_PAGE_SIZE,
              "DatabaseOptions::kDefaultPageSize must match the value "
              "configured into SQLite");

DatabaseDiagnostics::DatabaseDiagnostics() = default;
DatabaseDiagnostics::~DatabaseDiagnostics() = default;

void DatabaseDiagnostics::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> context) const {
  context->set_reported_sqlite_error_code(reported_sqlite_error_code);
  context->set_error_code(error_code);
  context->set_last_errno(last_errno);
  context->set_sql_statement(sql_statement);
  context->set_version(version);
  for (const auto& sql : schema_sql_rows) {
    context->add_schema_sql_rows(sql);
  }
  for (const auto& name : schema_other_row_names) {
    context->add_schema_other_row_names(name);
  }
  context->set_has_valid_header(has_valid_header);
  context->set_has_valid_schema(has_valid_schema);
  context->set_error_message(error_message);
}

Database::Database() : Database(DatabaseOptions{}) {}

Database::Database(DatabaseOptions options)
    : options_(options), mmap_disabled_(!enable_mmap_by_default_) {
  DCHECK_GE(options.page_size, 512);
  DCHECK_LE(options.page_size, 65536);
  DCHECK(!(options.page_size & (options.page_size - 1)))
      << "page_size must be a power of two";
  DCHECK(!options_.mmap_alt_status_discouraged ||
         options_.enable_views_discouraged)
      << "mmap_alt_status requires views";

  // It's valid to construct a database on a sequence and then pass it to a
  // different sequence before usage.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Database::~Database() {
  Close();
}

// static
void Database::DisableMmapByDefault() {
  enable_mmap_by_default_ = false;
}

bool Database::Open(const base::FilePath& path) {
  std::string path_string = AsUTF8ForSQL(path);
  TRACE_EVENT1("sql", "Database::Open", "path", path_string);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!path.empty());
  DCHECK_NE(path_string, kSqliteOpenInMemoryPath)
      << "Path conflicts with SQLite magic identifier";

  if (OpenInternal(path_string)) {
    return true;
  }
  // OpenInternal() may have run the error callback before returning false. If
  // the error callback poisoned `this`, the database may have been recovered or
  // razed, so a second attempt may succeed.
  if (poisoned_) {
    Close();
    return OpenInternal(path_string);
  }
  // Otherwise, do not attempt to reopen.
  return false;
}

bool Database::OpenInMemory() {
  TRACE_EVENT0("sql", "Database::OpenInMemory");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  in_memory_ = true;
  return OpenInternal(kSqliteOpenInMemoryPath);
}

void Database::DetachFromSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void Database::CloseInternal(bool forced) {
  TRACE_EVENT0("sql", "Database::CloseInternal");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(shess): Calling "PRAGMA journal_mode = DELETE" at this point
  // will delete the -journal file.  For ChromiumOS or other more
  // embedded systems, this is probably not appropriate, whereas on
  // desktop it might make some sense.

  // sqlite3_close() needs all prepared statements to be finalized.

  // Release cached statements.
  statement_cache_.clear();

  // With cached statements released, in-use statements will remain.
  // Closing the database while statements are in use is an API
  // violation, except for forced close (which happens from within a
  // statement's error handler).
  DCHECK(forced || open_statements_.empty());

  // Deactivate any outstanding statements so sqlite3_close() works.
  for (StatementRef* statement_ref : open_statements_) {
    statement_ref->Close(forced);
  }
  open_statements_.clear();

  if (is_open()) {
    // Call to InitScopedBlockingCall() cannot go at the beginning of the
    // function because Close() must be called from destructor to clean
    // statement_cache_, it won't cause any disk access and it most probably
    // will happen on thread not allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    std::optional<base::ScopedBlockingCall> scoped_blocking_call;
    InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

    // Resetting acquires a lock to ensure no dump is happening on the database
    // at the same time. Unregister takes ownership of provider and it is safe
    // since the db is reset. memory_dump_provider_ could be null if db_ was
    // poisoned.
    if (memory_dump_provider_) {
      memory_dump_provider_->ResetDatabase();
      base::trace_event::MemoryDumpManager::GetInstance()
          ->UnregisterAndDeleteDumpProviderSoon(
              std::move(memory_dump_provider_));
    }

    // Invalidate any `WeakPtr`s held by scoping helpers.
    weak_factory_.InvalidateWeakPtrs();

    sqlite3* raw_db = db_;
    db_ = nullptr;
    auto sqlite_result_code = ToSqliteResultCode(sqlite3_close(raw_db));

    DCHECK_NE(sqlite_result_code, SqliteResultCode::kBusy)
        << "sqlite3_close() called while prepared statements are still alive";
    DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
        << "sqlite3_close() failed in an unexpected way: "
        << sqlite3_errmsg(raw_db);

    // Closing a SQLite database connection implicitly rolls back transactions.
    // (See https://www.sqlite.org/c3ref/close.html for details.) Callers need
    // not call `RollbackAllTransactions()`, but we still must account for the
    // implicit rollback in our internal bookkeeping.
    transaction_nesting_ = 0;
  }
}

bool Database::is_open() const {
  return static_cast<bool>(db_) && !poisoned_;
}

void Database::Close() {
  TRACE_EVENT0("sql", "Database::Close");
  // If the database was already closed by RazeAndPoison(), then no
  // need to close again.  Clear the |poisoned_| bit so that incorrect
  // API calls are caught.
  if (poisoned_) {
    poisoned_ = false;
    return;
  }

  CloseInternal(false);
}

void Database::Preload() {
  TRACE_EVENT0("sql", "Database::Preload");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    DCHECK(poisoned_) << "Cannot preload null db";
    return;
  }

  CHECK(!options_.exclusive_database_file_lock)
      << "Cannot preload an exclusively locked database.";

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  // Maximum number of bytes that will be prefetched from the database.
  //
  // This limit is very aggressive. The main trade-off involved is that having
  // SQLite block on reading from disk has a high impact on Chrome startup cost
  // for the databases that are on the critical path to startup. So, the limit
  // must exceed the expected sizes of databases on the critical path.
  constexpr int kPreReadSize = 128 * 1024 * 1024;  // 128 MB
  base::PreReadFile(DbPath(), /*is_executable=*/false, /*sequential=*/false,
                    kPreReadSize);
}

// SQLite keeps unused pages associated with a database in a cache.  It asks
// the cache for pages by an id, and if the page is present and the database is
// unchanged, it considers the content of the page valid and doesn't read it
// from disk.  When memory-mapped I/O is enabled, on read SQLite uses page
// structures created from the memory map data before consulting the cache.  On
// write SQLite creates a new in-memory page structure, copies the data from the
// memory map, and later writes it, releasing the updated page back to the
// cache.
//
// This means that in memory-mapped mode, the contents of the cached pages are
// not re-used for reads, but they are re-used for writes if the re-written page
// is still in the cache. The implementation of sqlite3_db_release_memory() as
// of SQLite 3.8.7.4 frees all pages from pcaches associated with the
// database, so it should free these pages.
//
// Unfortunately, the zero page is also freed.  That page is never accessed
// using memory-mapped I/O, and the cached copy can be re-used after verifying
// the file change counter on disk.  Also, fresh pages from cache receive some
// pager-level initialization before they can be used.  Since the information
// involved will immediately be accessed in various ways, it is unclear if the
// additional overhead is material, or just moving processor cache effects
// around.
//
// TODO(shess): It would be better to release the pages immediately when they
// are no longer needed.  This would basically happen after SQLite commits a
// transaction.  I had implemented a pcache wrapper to do this, but it involved
// layering violations, and it had to be setup before any other sqlite call,
// which was brittle.  Also, for large files it would actually make sense to
// maintain the existing pcache behavior for blocks past the memory-mapped
// segment.  I think drh would accept a reasonable implementation of the overall
// concept for upstreaming to SQLite core.
//
// TODO(shess): Another possibility would be to set the cache size small, which
// would keep the zero page around, plus some pre-initialized pages, and SQLite
// can manage things.  The downside is that updates larger than the cache would
// spill to the journal.  That could be compensated by setting cache_spill to
// false.  The downside then is that it allows open-ended use of memory for
// large transactions.
void Database::ReleaseCacheMemoryIfNeeded(bool implicit_change_performed) {
  TRACE_EVENT0("sql", "Database::ReleaseCacheMemoryIfNeeded");
  // The database could have been closed during a transaction as part of error
  // recovery.
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return;
  }

  // If memory-mapping is not enabled, the page cache helps performance.
  if (!mmap_enabled_) {
    return;
  }

  // On caller request, force the change comparison to fail.  Done before the
  // transaction-nesting test so that the signal can carry to transaction
  // commit.
  if (implicit_change_performed) {
    --total_changes_at_last_release_;
  }

  // Cached pages may be re-used within the same transaction.
  DCHECK_GE(transaction_nesting_, 0);
  if (transaction_nesting_) {
    return;
  }

  // If no changes have been made, skip flushing.  This allows the first page of
  // the database to remain in cache across multiple reads.
  const int64_t total_changes = sqlite3_total_changes64(db_);
  if (total_changes == total_changes_at_last_release_) {
    return;
  }

  total_changes_at_last_release_ = total_changes;

  // Passing the result code through ToSqliteResultCode() to catch issues such
  // as SQLITE_MISUSE.
  std::ignore = ToSqliteResultCode(sqlite3_db_release_memory(db_));
}

base::FilePath Database::DbPath() const {
  if (!is_open()) {
    return base::FilePath();
  }

  const char* path = sqlite3_db_filename(db_, "main");
  if (!path) {
    return base::FilePath();
  }
  const std::string_view db_path(path);
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(db_path));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::FilePath(db_path);
#else
  NOTREACHED();
#endif
}

std::string Database::CollectErrorInfo(int sqlite_error_code,
                                       Statement* stmt,
                                       DatabaseDiagnostics* diagnostics) const {
  TRACE_EVENT0("sql", "Database::CollectErrorInfo");

  DCHECK_NE(sqlite_error_code, SQLITE_OK)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_DONE)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_ROW)
      << __func__ << " received non-error result code";

  // Buffer for accumulating debugging info about the error.  Place
  // more-relevant information earlier, in case things overflow the
  // fixed-size reporting buffer.
  std::string debug_info;

  // The error message from the failed operation.
  int error_code = GetErrorCode();
  base::StringAppendF(&debug_info, "db error: %d/%s\n", error_code,
                      GetErrorMessage());
  if (diagnostics) {
    diagnostics->error_code = error_code;
    diagnostics->error_message = GetErrorMessage();
  }

  // TODO(shess): |error| and |GetErrorCode()| should always be the same, but
  // reading code does not entirely convince me.  Remove if they turn out to be
  // the same.
  if (sqlite_error_code != GetErrorCode()) {
    base::StringAppendF(&debug_info, "reported error: %d\n", sqlite_error_code);
  }

// System error information.  Interpretation of Windows errors is different
// from posix.
#if BUILDFLAG(IS_WIN)
  int last_errno = GetLastErrno();
  base::StringAppendF(&debug_info, "LastError: %d\n", last_errno);
  if (diagnostics) {
    diagnostics->last_errno = last_errno;
  }
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  int last_errno = GetLastErrno();
  base::StringAppendF(&debug_info, "errno: %d\n", last_errno);
  if (diagnostics) {
    diagnostics->last_errno = last_errno;
  }
#else
  NOTREACHED();  // Add appropriate log info.
#endif

  if (stmt) {
    std::string sql_string = stmt->GetSQLStatement();
    base::StringAppendF(&debug_info, "statement: %s\n", sql_string.c_str());
    if (diagnostics) {
      diagnostics->sql_statement = sql_string;
    }
  } else {
    base::StringAppendF(&debug_info, "statement: NULL\n");
  }

  // SQLITE_ERROR often indicates some sort of mismatch between the statement
  // and the schema, possibly due to a failed schema migration.
  if (sqlite_error_code == SQLITE_ERROR) {
    static constexpr char kVersionSql[] =
        "SELECT value FROM meta WHERE key='version'";
    sqlite3_stmt* sqlite_statement;
    // When the number of bytes passed to sqlite3_prepare_v3() includes the null
    // terminator, SQLite avoids a buffer copy.
    int rc = sqlite3_prepare_v3(db_, kVersionSql, sizeof(kVersionSql),
                                SQLITE_PREPARE_NO_VTAB, &sqlite_statement,
                                /* pzTail= */ nullptr);
    if (rc == SQLITE_OK) {
      rc = sqlite3_step(sqlite_statement);
      if (rc == SQLITE_ROW) {
        int version = sqlite3_column_int(sqlite_statement, 0);
        base::StringAppendF(&debug_info, "version: %d\n", version);
        if (diagnostics) {
          diagnostics->version = version;
        }
      } else if (rc == SQLITE_DONE) {
        debug_info += "version: none\n";
      } else {
        base::StringAppendF(&debug_info, "version: error %d\n", rc);
      }
      sqlite3_finalize(sqlite_statement);
    } else {
      base::StringAppendF(&debug_info, "version: prepare error %d\n", rc);
    }

    // Get all the SQL from sqlite_schema.
    debug_info += "schema:\n";
    static constexpr char kSchemaSql[] =
        "SELECT sql FROM sqlite_schema WHERE sql IS NOT NULL ORDER BY ROWID";
    rc = sqlite3_prepare_v3(db_, kSchemaSql, sizeof(kSchemaSql),
                            SQLITE_PREPARE_NO_VTAB, &sqlite_statement,
                            /* pzTail= */ nullptr);
    if (rc == SQLITE_OK) {
      while ((rc = sqlite3_step(sqlite_statement)) == SQLITE_ROW) {
        std::string text;
        base::StringAppendF(&text, "%s",
                            reinterpret_cast<const char*>(
                                sqlite3_column_text(sqlite_statement, 0)));
        debug_info += text + "\n";
        if (diagnostics) {
          diagnostics->schema_sql_rows.push_back(text);
        }
      }

      if (rc != SQLITE_DONE) {
        base::StringAppendF(&debug_info, "error %d\n", rc);
      }
      sqlite3_finalize(sqlite_statement);
    } else {
      base::StringAppendF(&debug_info, "prepare error %d\n", rc);
    }

    // Automatically generated indices have a NULL 'sql' column. For those rows,
    // we log the name column instead.
    debug_info += "schema rows with only name:\n";
    static constexpr char kSchemaOtherRowNamesSql[] =
        "SELECT name FROM sqlite_schema WHERE sql IS NULL ORDER BY ROWID";
    rc = sqlite3_prepare_v3(db_, kSchemaOtherRowNamesSql,
                            sizeof(kSchemaOtherRowNamesSql),
                            SQLITE_PREPARE_NO_VTAB, &sqlite_statement,
                            /* pzTail= */ nullptr);
    if (rc == SQLITE_OK) {
      while ((rc = sqlite3_step(sqlite_statement)) == SQLITE_ROW) {
        std::string text;
        base::StringAppendF(&text, "%s",
                            reinterpret_cast<const char*>(
                                sqlite3_column_text(sqlite_statement, 0)));
        debug_info += text + "\n";
        if (diagnostics) {
          diagnostics->schema_other_row_names.push_back(text);
        }
      }

      if (rc != SQLITE_DONE) {
        base::StringAppendF(&debug_info, "error %d\n", rc);
      }
      sqlite3_finalize(sqlite_statement);
    } else {
      base::StringAppendF(&debug_info, "prepare error %d\n", rc);
    }
  }

  return debug_info;
}

// TODO(shess): Since this is only called in an error situation, it might be
// prudent to rewrite in terms of SQLite API calls, and mark the function const.
std::string Database::CollectCorruptionInfo() {
  TRACE_EVENT0("sql", "Database::CollectCorruptionInfo");
  // If the file cannot be accessed it is unlikely that an integrity check will
  // turn up actionable information.
  const base::FilePath db_path = DbPath();
  int64_t db_size = -1;
  if (!base::GetFileSize(db_path, &db_size) || db_size < 0) {
    return std::string();
  }

  // Buffer for accumulating debugging info about the error.  Place
  // more-relevant information earlier, in case things overflow the
  // fixed-size reporting buffer.
  std::string debug_info;
  base::StringAppendF(&debug_info, "SQLITE_CORRUPT, db size %" PRId64 "\n",
                      db_size);

  // Only check files up to 8M to keep things from blocking too long.
  const int64_t kMaxIntegrityCheckSize = 8192 * 1024;
  if (db_size > kMaxIntegrityCheckSize) {
    debug_info += "integrity_check skipped due to size\n";
  } else {
    std::vector<std::string> messages;

    // TODO(shess): FullIntegrityCheck() splits into a vector while this joins
    // into a string.  Probably should be refactored.
    const base::TimeTicks before = base::TimeTicks::Now();
    FullIntegrityCheck(&messages);
    base::StringAppendF(
        &debug_info, "integrity_check %" PRId64 " ms, %" PRIuS " records:\n",
        (base::TimeTicks::Now() - before).InMilliseconds(), messages.size());

    // SQLite returns up to 100 messages by default, trim deeper to
    // keep close to the 2000-character size limit for dumping.
    const size_t kMaxMessages = 20;
    for (size_t i = 0; i < kMaxMessages && i < messages.size(); ++i) {
      base::StringAppendF(&debug_info, "%s\n", messages[i].c_str());
    }
  }

  return debug_info;
}

bool Database::GetMmapAltStatus(int64_t* status) {
  TRACE_EVENT0("sql", "Database::GetMmapAltStatus");

  // The [meta] version uses a missing table as a signal for a fresh database.
  // That will not work for the view, which would not exist in either a new or
  // an existing database.  A new database _should_ be only one page long, so
  // just don't bother optimizing this case (start at offset 0).
  // TODO(shess): Could the [meta] case also get simpler, then?
  if (!DoesViewExist("MmapStatus")) {
    *status = 0;
    return true;
  }

  static constexpr char kMmapStatusSql[] = "SELECT * FROM MmapStatus";
  Statement s(GetUniqueStatement(kMmapStatusSql));
  if (s.Step()) {
    *status = s.ColumnInt64(0);
  }
  return s.Succeeded();
}

bool Database::SetMmapAltStatus(int64_t status) {
  Transaction transaction(this);
  if (!transaction.Begin()) {
    return false;
  }

  // View may not exist on first run.
  if (!Execute("DROP VIEW IF EXISTS MmapStatus")) {
    return false;
  }

  // Views live in the schema, so they cannot be parameterized.  For an integer
  // value, this construct should be safe from SQL injection, if the value
  // becomes more complicated use "SELECT quote(?)" to generate a safe quoted
  // value.
  const std::string create_view_sql = base::StringPrintf(
      "CREATE VIEW MmapStatus (value) AS SELECT %" PRId64, status);
  if (!Execute(create_view_sql)) {
    return false;
  }

  return transaction.Commit();
}

size_t Database::ComputeMmapSizeForOpen() {
  TRACE_EVENT0("sql", "Database::ComputeMmapSizeForOpen");

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  // How much to map if no errors are found.  50MB encompasses the 99th
  // percentile of Chrome databases in the wild, so this should be good.
  const size_t kMmapEverything = 256 * 1024 * 1024;

  // Progress information is tracked in the [meta] table for databases which use
  // sql::MetaTable, otherwise it is tracked in a special view.
  // TODO(pwnall): Migrate all databases to using a meta table.
  int64_t mmap_ofs = 0;
  if (options_.mmap_alt_status_discouraged) {
    if (!GetMmapAltStatus(&mmap_ofs)) {
      return 0;
    }
  } else {
    // If [meta] doesn't exist, yet, it's a new database, assume the best.
    // sql::MetaTable::Init() will preload kMmapSuccess.
    if (!MetaTable::DoesTableExist(this)) {
      return kMmapEverything;
    }

    if (!MetaTable::GetMmapStatus(this, &mmap_ofs)) {
      return 0;
    }
  }

  // Database read failed in the past, don't memory map.
  if (mmap_ofs == MetaTable::kMmapFailure) {
    return 0;
  }

  if (mmap_ofs != MetaTable::kMmapSuccess) {
    // Continue reading from previous offset.
    DCHECK_GE(mmap_ofs, 0);

    // GetSqliteVfsFile() returns null for in-memory and temporary databases.
    // This is fine, we don't want to enable memory-mapping in those cases
    // anyway.
    //
    // First, memory-mapping is a no-op for in-memory databases.
    //
    // Second, temporary databases are only used for corruption recovery, which
    // occurs in response to I/O errors. An environment with heightened I/O
    // errors translates into a higher risk of mmap-induced Chrome crashes.
    sqlite3_int64 db_size = 0;
    sqlite3_file* file = GetSqliteVfsFile();
    if (!file || file->pMethods->xFileSize(file, &db_size) != SQLITE_OK) {
      return 0;
    }

    // Read more of the database looking for errors.  The VFS interface is used
    // to assure that the reads are valid for SQLite.  |g_reads_allowed| is used
    // to limit checking to 20MB per run of Chromium.
    //
    // Read the data left, or |g_reads_allowed|, whichever is smaller.
    // |g_reads_allowed| limits the total amount of I/O to spend verifying data
    // in a single Chromium run.
    sqlite3_int64 amount = db_size - mmap_ofs;
    if (amount < 0) {
      amount = 0;
    }
    if (amount > 0) {
      static base::NoDestructor<base::Lock> lock;
      base::AutoLock auto_lock(*lock);
      static sqlite3_int64 g_reads_allowed = 20 * 1024 * 1024;
      if (g_reads_allowed < amount) {
        amount = g_reads_allowed;
      }
      g_reads_allowed -= amount;
    }

    // |amount| can be <= 0 if |g_reads_allowed| ran out of quota, or if the
    // database was truncated after a previous pass.
    if (amount <= 0 && mmap_ofs < db_size) {
      DCHECK_EQ(0, amount);
    } else {
      static const int kPageSize = 4096;
      char buf[kPageSize];
      while (amount > 0) {
        int rc = file->pMethods->xRead(file, buf, sizeof(buf), mmap_ofs);
        if (rc == SQLITE_OK) {
          mmap_ofs += sizeof(buf);
          amount -= sizeof(buf);
        } else if (rc == SQLITE_IOERR_SHORT_READ) {
          // Reached EOF for a database with page size < |kPageSize|.
          mmap_ofs = db_size;
          break;
        } else {
          // TODO(shess): Consider calling OnSqliteError().
          mmap_ofs = MetaTable::kMmapFailure;
          break;
        }
      }

      // Log these events after update to distinguish meta update failure.
      if (mmap_ofs >= db_size) {
        mmap_ofs = MetaTable::kMmapSuccess;
      } else {
        DCHECK(mmap_ofs > 0 || mmap_ofs == MetaTable::kMmapFailure);
      }

      if (options_.mmap_alt_status_discouraged) {
        if (!SetMmapAltStatus(mmap_ofs)) {
          return 0;
        }
      } else {
        if (!MetaTable::SetMmapStatus(this, mmap_ofs)) {
          return 0;
        }
      }
    }
  }

  if (mmap_ofs == MetaTable::kMmapFailure) {
    return 0;
  }
  if (mmap_ofs == MetaTable::kMmapSuccess) {
    return kMmapEverything;
  }
  return mmap_ofs;
}

int Database::SqlitePrepareFlags() const {
  return enable_virtual_tables_ ? 0 : SQLITE_PREPARE_NO_VTAB;
}

sqlite3_file* Database::GetSqliteVfsFile() {
  CHECK(db_) << "Database not opened";

  // sqlite3_file_control() accepts a null pointer to mean the "main" database
  // attached to a connection. https://www.sqlite.org/c3ref/file_control.html
  constexpr const char* kMainDatabaseName = nullptr;

  sqlite3_file* result = nullptr;
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_file_control(
      db_, kMainDatabaseName, SQLITE_FCNTL_FILE_POINTER, &result));

  // SQLITE_FCNTL_FILE_POINTER is handled directly by SQLite, not by the VFS. It
  // is only supposed to fail with SQLITE_ERROR if the database name is not
  // recognized. However, "main" should always be recognized.
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_file_control(SQLITE_FCNTL_FILE_POINTER) failed";

  // SQLite does not return null when called on an in-memory or temporary
  // database. Instead, it returns returns a VFS file object with a null
  // pMethods member.
  DCHECK(result)
      << "sqlite3_file_control() succeded but returned a null sqlite3_file*";
  if (!result->pMethods) {
    // If this assumption fails, sql::Database will still function correctly,
    // but will miss some configuration optimizations. The DCHECK is here to
    // alert us (via test failures and ASAN canary builds) of such cases.
    DCHECK_EQ(DbPath().AsUTF8Unsafe(), "")
        << "sqlite3_file_control() returned a sqlite3_file* with null pMethods "
        << "in a case when it shouldn't have.";

    return nullptr;
  }

  return result;
}

void Database::TrimMemory() {
  TRACE_EVENT0("sql", "Database::TrimMemory");

  if (!db_) {
    return;
  }

  // Passing the result code through ToSqliteResultCode() to catch issues such
  // as SQLITE_MISUSE.
  std::ignore = ToSqliteResultCode(sqlite3_db_release_memory(db_));

  // It is tempting to use sqlite3_release_memory() here as well. However, the
  // API is documented to be a no-op unless SQLite is built with
  // SQLITE_ENABLE_MEMORY_MANAGEMENT. We do not use this option, because it is
  // incompatible with per-database page cache pools. Behind the scenes,
  // SQLITE_ENABLE_MEMORY_MANAGEMENT causes SQLite to use a global page cache
  // pool, and sqlite3_release_memory() releases unused pages from this global
  // pool.
#if defined(SQLITE_ENABLE_MEMORY_MANAGEMENT)
#error "This method assumes SQLITE_ENABLE_MEMORY_MANAGEMENT is not defined"
#endif  // defined(SQLITE_ENABLE_MEMORY_MANAGEMENT)
}

// Create an in-memory database with the existing database's page
// size, then backup that database over the existing database.
bool Database::Raze() {
  TRACE_EVENT0("sql", "Database::Raze");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  if (!db_) {
    DCHECK(poisoned_) << "Cannot raze null db";
    return false;
  }

  DCHECK_GE(transaction_nesting_, 0);
  if (transaction_nesting_ > 0) {
    DLOG(FATAL) << "Cannot raze within a transaction";
    return false;
  }

  sql::Database null_db(sql::DatabaseOptions{
      .exclusive_locking = true,
      .page_size = options_.page_size,
      .cache_size = 0,
      .enable_views_discouraged = options_.enable_views_discouraged,
  });
  if (!null_db.OpenInMemory()) {
    DLOG(FATAL) << "Unable to open in-memory database.";
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // Android compiles with SQLITE_DEFAULT_AUTOVACUUM.  Unfortunately,
  // in-memory databases do not respect this define.
  // TODO(shess): Figure out a way to set this without using platform
  // specific code.  AFAICT from sqlite3.c, the only way to do it
  // would be to create an actual filesystem database, which is
  // unfortunate.
  if (!null_db.Execute("PRAGMA auto_vacuum = 1")) {
    return false;
  }
#endif

  // The page size doesn't take effect until a database has pages, and
  // at this point the null database has none.  Changing the schema
  // version will create the first page.  This will not affect the
  // schema version in the resulting database, as SQLite's backup
  // implementation propagates the schema version from the original
  // database to the new version of the database, incremented by one
  // so that other readers see the schema change and act accordingly.
  if (!null_db.Execute("PRAGMA schema_version = 1")) {
    return false;
  }

  // SQLite tracks the expected number of database pages in the first
  // page, and if it does not match the total retrieved from a
  // filesystem call, treats the database as corrupt.  This situation
  // breaks almost all SQLite calls.  "PRAGMA writable_schema" can be
  // used to hint to SQLite to soldier on in that case, specifically
  // for purposes of recovery.  [See SQLITE_CORRUPT_BKPT case in
  // sqlite3.c lockBtree().]
  // TODO(shess): With this, "PRAGMA auto_vacuum" and "PRAGMA
  // page_size" can be used to query such a database.
  ScopedWritableSchema writable_schema(weak_factory_.GetWeakPtr());

#if BUILDFLAG(IS_WIN)
  // On Windows, truncate silently fails when applied to memory-mapped files.
  // Disable memory-mapping so that the truncate succeeds.  Note that other
  // Database connections may have memory-mapped the file, so this may not
  // entirely prevent the problem.
  // [Source: <https://sqlite.org/mmap.html> plus experiments.]
  std::ignore = Execute("PRAGMA mmap_size = 0");
#endif

  SqliteResultCode sqlite_result_code = BackupDatabaseForRaze(null_db.db_, db_);

  // The destination database was locked.
  if (sqlite_result_code == SqliteResultCode::kBusy) {
    return false;
  }

  // SQLITE_NOTADB can happen if page 1 of db_ exists, but is not
  // formatted correctly.  SQLITE_IOERR_SHORT_READ can happen if db_
  // isn't even big enough for one page.  Either way, reach in and
  // truncate it before trying again.
  // TODO(shess): Maybe it would be worthwhile to just truncate from
  // the get-go?
  if (sqlite_result_code == SqliteResultCode::kNotADatabase ||
      sqlite_result_code == SqliteResultCode::kIoShortRead) {
    sqlite3_file* file = GetSqliteVfsFile();
    if (!file || file->pMethods->xTruncate(file, 0) != SQLITE_OK) {
      DLOG(FATAL) << "Failed to truncate file.";
      return false;
    }

    sqlite_result_code = BackupDatabaseForRaze(null_db.db_, db_);
    if (sqlite_result_code != SqliteResultCode::kDone) {
      return false;
    }
  }

  // Page size of |db_| and |null_db| differ.
  if (sqlite_result_code == SqliteResultCode::kReadOnly) {
    // Enter TRUNCATE mode to change page size.
    // TODO(shuagga@microsoft.com): Need a guarantee here that there is no other
    // database connection open.
    std::ignore = Execute("PRAGMA journal_mode=TRUNCATE;");
    const std::string page_size_sql = base::StrCat(
        {"PRAGMA page_size=", base::NumberToString(options_.page_size)});
    if (!Execute(page_size_sql)) {
      return false;
    }
    // Page size isn't changed until the database is vacuumed.
    std::ignore = Execute("VACUUM");
    // Re-enter WAL mode.
    if (UseWALMode()) {
      std::ignore = Execute("PRAGMA journal_mode=WAL;");
    }

    sqlite_result_code = BackupDatabaseForRaze(null_db.db_, db_);
    if (sqlite_result_code != SqliteResultCode::kDone) {
      return false;
    }
  }

  if (sqlite_result_code != SqliteResultCode::kDone) {
    NOTIMPLEMENTED() << "Unhandled sqlite3_backup_step() error: "
                     << sqlite_result_code;
    return false;
  }

  // Checkpoint to propagate transactions to the database file and empty the WAL
  // file.
  // The database can still contain old data if the Checkpoint fails so fail the
  // Raze.
  return CheckpointDatabase();
}

bool Database::RazeAndPoison() {
  TRACE_EVENT0("sql", "Database::RazeAndPoison");

  if (!db_) {
    DCHECK(poisoned_) << "Cannot raze null db";
    return false;
  }

  // Raze() cannot run in a transaction.
  RollbackAllTransactions();

  bool result = Raze();

  CloseInternal(true);

  // Mark the database so that future API calls fail appropriately,
  // but don't DCHECK (because after calling this function they are
  // expected to fail).
  poisoned_ = true;

  return result;
}

void Database::Poison() {
  TRACE_EVENT0("sql", "Database::Poison");

  if (!db_) {
    DCHECK(poisoned_) << "Cannot poison null db";
    return;
  }

  CloseInternal(true);

  // Mark the database so that future API calls fail appropriately,
  // but don't DCHECK (because after calling this function they are
  // expected to fail).
  poisoned_ = true;
}

// TODO(shess): To the extent possible, figure out the optimal
// ordering for these deletes which will prevent other Database connections
// from seeing odd behavior.  For instance, it may be necessary to
// manually lock the main database file in a SQLite-compatible fashion
// (to prevent other processes from opening it), then delete the
// journal files, then delete the main database file.  Another option
// might be to lock the main database file and poison the header with
// junk to prevent other processes from opening it successfully (like
// Gears "SQLite poison 3" trick).
//
// static
bool Database::Delete(const base::FilePath& path) {
  TRACE_EVENT1("sql", "Database::Delete", "path", path.MaybeAsASCII());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath journal_path = Database::JournalPath(path);
  base::FilePath wal_path = Database::WriteAheadLogPath(path);

  std::string journal_str = AsUTF8ForSQL(journal_path);
  std::string wal_str = AsUTF8ForSQL(wal_path);
  std::string path_str = AsUTF8ForSQL(path);

  EnsureSqliteInitialized();

  sqlite3_vfs* vfs = sqlite3_vfs_find(nullptr);
  CHECK(vfs);
  CHECK(vfs->xDelete);
  CHECK(vfs->xAccess);

  vfs->xDelete(vfs, journal_str.c_str(), 0);
  vfs->xDelete(vfs, wal_str.c_str(), 0);
  vfs->xDelete(vfs, path_str.c_str(), 0);

  int journal_exists = 0;
  vfs->xAccess(vfs, journal_str.c_str(), SQLITE_ACCESS_EXISTS, &journal_exists);

  int wal_exists = 0;
  vfs->xAccess(vfs, wal_str.c_str(), SQLITE_ACCESS_EXISTS, &wal_exists);

  int path_exists = 0;
  vfs->xAccess(vfs, path_str.c_str(), SQLITE_ACCESS_EXISTS, &path_exists);

  return !journal_exists && !wal_exists && !path_exists;
}

bool Database::BeginTransaction(InternalApiToken) {
  TRACE_EVENT0("sql", "Database::BeginTransaction");

  if (needs_rollback_) {
    DCHECK_GT(transaction_nesting_, 0);

    // When we're going to rollback, fail on this begin and don't actually
    // mark us as entering the nested transaction.
    return false;
  }

  bool success = true;
  DCHECK_GE(transaction_nesting_, 0);
  if (!transaction_nesting_) {
    needs_rollback_ = false;

    Statement begin(GetCachedStatement(SQL_FROM_HERE, "BEGIN TRANSACTION"));
    if (!begin.Run()) {
      return false;
    }
  }
  ++transaction_nesting_;
  return success;
}

void Database::RollbackTransaction(InternalApiToken) {
  TRACE_EVENT0("sql", "Database::RollbackTransaction");

  DCHECK_GE(transaction_nesting_, 0);
  if (!transaction_nesting_) {
    DCHECK(poisoned_) << "Rolling back a nonexistent transaction";
    return;
  }

  DCHECK_GT(transaction_nesting_, 0);
  --transaction_nesting_;

  if (transaction_nesting_ > 0) {
    // Mark the outermost transaction as needing rollback.
    needs_rollback_ = true;
    return;
  }

  DoRollback();
}

bool Database::CommitTransaction(InternalApiToken) {
  TRACE_EVENT0("sql", "Database::CommitTransaction");

  DCHECK_GE(transaction_nesting_, 0);
  if (!transaction_nesting_) {
    DCHECK(poisoned_) << "Committing a nonexistent transaction";
    return false;
  }

  DCHECK_GT(transaction_nesting_, 0);
  --transaction_nesting_;

  if (transaction_nesting_ > 0) {
    // Mark any nested transactions as failing after we've already got one.
    return !needs_rollback_;
  }

  if (needs_rollback_) {
    DoRollback();
    return false;
  }

  Statement commit(GetCachedStatement(SQL_FROM_HERE, "COMMIT"));

  bool succeeded = commit.Run();

  // Release dirty cache pages after the transaction closes.
  ReleaseCacheMemoryIfNeeded(false);

  return succeeded;
}

bool Database::BeginTransactionDeprecated() {
  return BeginTransaction(InternalApiToken());
}

bool Database::CommitTransactionDeprecated() {
  return CommitTransaction(InternalApiToken());
}

void Database::RollbackTransactionDeprecated() {
  RollbackTransaction(InternalApiToken());
}

void Database::RollbackAllTransactions() {
  TRACE_EVENT0("sql", "Database::RollbackAllTransactions");

  DCHECK_GE(transaction_nesting_, 0);
  if (transaction_nesting_ > 0) {
    transaction_nesting_ = 0;
    DoRollback();
  }
}

bool Database::AttachDatabase(const base::FilePath& other_db_path,
                              std::string_view attachment_point) {
  TRACE_EVENT0("sql", "Database::AttachDatabase");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ValidAttachmentPoint(attachment_point));

  Statement statement(GetUniqueStatement("ATTACH ? AS ?"));
#if BUILDFLAG(IS_WIN)
  statement.BindString16(0, base::AsStringPiece16(other_db_path.value()));
#else
  statement.BindString(0, other_db_path.value());
#endif
  statement.BindString(1, attachment_point);
  return statement.Run();
}

bool Database::DetachDatabase(std::string_view attachment_point) {
  TRACE_EVENT0("sql", "Database::DetachDatabase");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ValidAttachmentPoint(attachment_point));

  Statement statement(GetUniqueStatement("DETACH ?"));
  statement.BindString(0, attachment_point);
  return statement.Run();
}

// TODO(crbug.com/40779018): Change this to execute exactly one statement.
SqliteResultCode Database::ExecuteAndReturnResultCode(
    base::cstring_view initial_sql) {
  TRACE_EVENT0("sql", "Database::ExecuteAndReturnErrorCode");

  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return SqliteResultCode::kError;
  }

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);
  const char* sql = initial_sql.c_str();

  SqliteResultCode sqlite_result_code = SqliteResultCode::kOk;
  while ((sqlite_result_code == SqliteResultCode::kOk) && *sql) {
    sqlite3_stmt* sqlite_statement;
    const char* leftover_sql;
    sqlite_result_code = ToSqliteResultCode(
        sqlite3_prepare_v3(db_, sql, /* nByte= */ -1, SqlitePrepareFlags(),
                           &sqlite_statement, &leftover_sql));

#if DCHECK_IS_ON()
    // Report SQL compilation errors. On developer machines, the errors are most
    // likely caused by invalid SQL in an under-development feature. In
    // production, SQL compilation errors are caused by database schema
    // corruption.
    //
    // DCHECK would not be appropriate here, because on-disk data is always
    // subject to corruption, so Chrome cannot assume that the database schema
    // will remain intact.
    if (sqlite_result_code == SqliteResultCode::kError) {
      DLOG(ERROR) << "SQL compilation error: " << GetErrorMessage()
                  << ". Statement: " << sql;
    }
#endif  // DCHECK_IS_ON()

    // Stop if compiling the SQL statement fails.
    if (sqlite_result_code != SqliteResultCode::kOk) {
      DCHECK_NE(sqlite_result_code, SqliteResultCode::kDone)
          << "sqlite3_prepare_v3() returned unexpected non-error result code";
      DCHECK_NE(sqlite_result_code, SqliteResultCode::kRow)
          << "sqlite3_prepare_v3() returned unexpected non-error result code";
      break;
    }

    sql = leftover_sql;

    // This happens if |sql| originally only contained comments or whitespace.
    // TODO(shess): Audit to see if this can become a DCHECK().  Having
    // extraneous comments and whitespace in the SQL statements increases
    // runtime cost and can easily be shifted out to the C++ layer.
    if (!sqlite_statement) {
      continue;
    }

    while (true) {
      sqlite_result_code = ToSqliteResultCode(sqlite3_step(sqlite_statement));
      if (sqlite_result_code != SqliteResultCode::kRow) {
        break;
      }

      // TODO(shess): Audit to see if this can become a DCHECK.  I think PRAGMA
      // is the only legitimate case for this. Previously recorded histograms
      // show significant use of this code path.
    }

    // sqlite3_finalize() returns SQLITE_OK if the most recent sqlite3_step()
    // returned SQLITE_DONE or SQLITE_ROW, otherwise the error code.
    sqlite_result_code = ToSqliteResultCode(sqlite3_finalize(sqlite_statement));
    DCHECK_NE(sqlite_result_code, SqliteResultCode::kDone)
        << "sqlite3_finalize() returned unexpected non-error result code";
    DCHECK_NE(sqlite_result_code, SqliteResultCode::kRow)
        << "sqlite3_finalize() returned unexpected non-error result code";

    // sqlite3_exec() does this, presumably to avoid spinning the parser for
    // trailing whitespace.
    // TODO(shess): Audit to see if this can become a DCHECK.
    while (base::IsAsciiWhitespace(*sql)) {
      sql++;
    }
  }

  // Most calls to Execute() modify the database.  The main exceptions would be
  // calls such as CREATE TABLE IF NOT EXISTS which could modify the database
  // but sometimes don't.
  ReleaseCacheMemoryIfNeeded(true);

  DCHECK_NE(sqlite_result_code, SqliteResultCode::kDone)
      << __func__ << " about to return unexpected non-error result code";
  DCHECK_NE(sqlite_result_code, SqliteResultCode::kRow)
      << __func__ << " about to return unexpected non-error result code";
  return sqlite_result_code;
}

bool Database::Execute(base::cstring_view sql) {
  TRACE_EVENT0("sql", "Database::Execute");

  return ExecuteWithTimeout(sql, base::TimeDelta());
}

bool Database::ExecuteWithTimeout(base::cstring_view sql,
                                  base::TimeDelta timeout) {
  TRACE_EVENT1("sql", "Database::ExecuteWithTimeout", "query", sql);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return false;
  }

  // Passing zero or a negative value to sqlite3_busy_timeout() would clear any
  // busy handlers defined prior to this point.
  if (timeout.is_positive()) {
    DCHECK_LT(timeout.InMilliseconds(), INT_MAX);
    sqlite3_busy_timeout(db_, static_cast<int>(timeout.InMilliseconds()));
  }
  SqliteResultCode sqlite_result_code = ExecuteAndReturnResultCode(sql);
  sqlite3_busy_timeout(db_, 0);
  if (sqlite_result_code != SqliteResultCode::kOk) {
    OnSqliteError(ToSqliteErrorCode(sqlite_result_code), nullptr, sql.c_str());
    // At this point, `this` may have been modified or even deleted as a result
    // of the caller-provided error callback.
  }
  return sqlite_result_code == SqliteResultCode::kOk;
}

bool Database::ExecuteScriptForTesting(base::cstring_view sql_script) {
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return false;
  }

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  const char* sql = sql_script.c_str();
  while (*sql) {
    sqlite3_stmt* sqlite_statement;
    auto sqlite_result_code = ToSqliteResultCode(sqlite3_prepare_v3(
        db_, sql, /*nByte=*/-1, SqlitePrepareFlags(), &sqlite_statement, &sql));
    if (sqlite_result_code != SqliteResultCode::kOk) {
      return false;
    }

    if (!sqlite_statement) {
      // Trailing comment or whitespace after the last semicolon.
      return true;
    }

    // TODO(pwnall): Investigate restricting ExecuteScriptForTesting() to
    //               statements that don't produce any result rows.
    do {
      sqlite_result_code = ToSqliteResultCode(sqlite3_step(sqlite_statement));
    } while (sqlite_result_code == SqliteResultCode::kRow);

    // sqlite3_finalize() returns SQLITE_OK if the most recent sqlite3_step()
    // returned SQLITE_DONE or SQLITE_ROW, otherwise the error code.
    sqlite_result_code = ToSqliteResultCode(sqlite3_finalize(sqlite_statement));
    if (sqlite_result_code != SqliteResultCode::kOk) {
      return false;
    }
  }

  return true;
}

scoped_refptr<Database::StatementRef> Database::GetCachedStatement(
    StatementID id,
    base::cstring_view sql) {
  auto it = statement_cache_.find(id);
  if (it != statement_cache_.end()) {
    // Statement is in the cache. It should still be valid. We're the only
    // entity invalidating cached statements, and we remove them from the cache
    // when we do that.
    DCHECK(it->second->is_valid());
    DCHECK_EQ(std::string(sqlite3_sql(it->second->stmt())), std::string(sql))
        << "GetCachedStatement used with same ID but different SQL";

    // Reset the statement so it can be reused.
    //
    // ToSqliteResultCode() is called to ensure that sqlite3_reset() doesn't
    // return a concerning code, such as SQLITE_MISUSE. The processed error code
    // is ignored because sqlite3_reset() returns an error code if the last
    // sqlite3_step() failed, and that error was already reported when we ran
    // sqlite3_step(), via Statement::Run() or Statement::Step().
    std::ignore = ToSqliteResultCode(sqlite3_reset(it->second->stmt()));
    return it->second;
  }

  scoped_refptr<StatementRef> statement = GetUniqueStatement(sql);
  if (statement->is_valid()) {
    statement_cache_[id] = statement;  // Only cache valid statements.
    DCHECK_EQ(std::string(sqlite3_sql(statement->stmt())), std::string(sql))
        << "Input SQL does not match SQLite's normalized version";
  }
  return statement;
}

scoped_refptr<Database::StatementRef> Database::GetUniqueStatement(
    base::cstring_view sql) {
  return GetStatementImpl(sql, /*is_readonly=*/false);
}

scoped_refptr<Database::StatementRef> Database::GetReadonlyStatement(
    base::cstring_view sql) {
  return GetStatementImpl(sql, /*is_readonly=*/true);
}

scoped_refptr<Database::StatementRef> Database::GetStatementImpl(
    base::cstring_view sql,
    bool is_readonly) {
  // Return inactive statement.
  if (!db_) {
    return base::MakeRefCounted<StatementRef>(nullptr, nullptr, poisoned_);
  }

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

#if DCHECK_IS_ON()
  const char* unused_sql = nullptr;
  const char** unused_sql_ptr = &unused_sql;
#else
  constexpr const char** unused_sql_ptr = nullptr;
#endif  // DCHECK_IS_ON()
  // TODO(pwnall): Cached statements (but not unique statements) should be
  //               prepared with prepFlags set to SQLITE_PREPARE_PERSISTENT.
  sqlite3_stmt* sqlite_statement;
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_prepare_v3(
      db_, sql.c_str(), /* nByte= */ -1, SqlitePrepareFlags(),
      &sqlite_statement, unused_sql_ptr));

#if DCHECK_IS_ON()
  // Report SQL compilation errors. On developer machines, the errors are most
  // likely caused by invalid SQL in an under-development feature. In
  // production, SQL compilation errors are caused by database schema
  // corruption.
  //
  // DCHECK would not be appropriate here, because on-disk data is always
  // subject to corruption, so Chrome cannot assume that the database schema
  // will remain intact.
  if (sqlite_result_code == SqliteResultCode::kError) {
    DLOG(ERROR) << "SQL compilation error: " << GetErrorMessage()
                << ". Statement: " << sql;
  }
#endif  // DCHECK_IS_ON()

  if (sqlite_result_code != SqliteResultCode::kOk) {
    DCHECK_NE(sqlite_result_code, SqliteResultCode::kDone)
        << "sqlite3_prepare_v3() returned unexpected non-error result code";
    DCHECK_NE(sqlite_result_code, SqliteResultCode::kRow)
        << "sqlite3_prepare_v3() returned unexpected non-error result code";
    OnSqliteError(ToSqliteErrorCode(sqlite_result_code), nullptr, sql.c_str());
    return base::MakeRefCounted<StatementRef>(nullptr, nullptr, false);
  }

  // If readonly statement is expected and the statement is not readonly, return
  // an invalid statement and close the created statement.
  if (is_readonly && sqlite3_stmt_readonly(sqlite_statement) == 0) {
    DLOG(ERROR) << "Readonly SQL statement failed readonly test " << sql;
    // Make a `StatementRef` that will close the created statement.
    base::MakeRefCounted<StatementRef>(this, sqlite_statement, true);

    return base::MakeRefCounted<StatementRef>(nullptr, nullptr, false);
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(unused_sql, sql.c_str() + sql.size())
      << "Unused text: " << std::string(unused_sql) << "\n"
      << "in prepared SQL statement: " << std::string(sql);
#endif  // DCHECK_IS_ON()

  DCHECK(sqlite_statement) << "No SQL statement in string: " << sql;

  return base::MakeRefCounted<StatementRef>(this, sqlite_statement, true);
}

std::string Database::GetSchema() {
  // The ORDER BY should not be necessary, but relying on organic
  // order for something like this is questionable.
  static constexpr char kSql[] =
      "SELECT type, name, tbl_name, sql "
      "FROM sqlite_schema ORDER BY 1, 2, 3, 4";
  Statement statement(GetUniqueStatement(kSql));

  std::string schema;
  while (statement.Step()) {
    schema += statement.ColumnString(0);
    schema += '|';
    schema += statement.ColumnString(1);
    schema += '|';
    schema += statement.ColumnString(2);
    schema += '|';
    schema += statement.ColumnString(3);
    schema += '\n';
  }

  return schema;
}

bool Database::IsSQLValid(base::cstring_view sql) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return false;
  }

#if DCHECK_IS_ON()
  const char* unused_sql = nullptr;
  const char** unused_sql_ptr = &unused_sql;
#else
  constexpr const char** unused_sql_ptr = nullptr;
#endif  // DCHECK_IS_ON()

  sqlite3_stmt* sqlite_statement = nullptr;
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_prepare_v3(
      db_, sql.c_str(), /* nByte= */ -1, SqlitePrepareFlags(),
      &sqlite_statement, unused_sql_ptr));
  if (sqlite_result_code != SqliteResultCode::kOk) {
    return false;
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(unused_sql, sql.c_str() + sql.size())
      << "Unused text: " << std::string(unused_sql) << "\n"
      << "in SQL statement: " << std::string(sql);
#endif  // DCHECK_IS_ON()

  DCHECK(sqlite_statement) << "No SQL statement in string: " << sql;

  sqlite_result_code = ToSqliteResultCode(sqlite3_finalize(sqlite_statement));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_finalize() failed for valid statement";
  return true;
}

bool Database::DoesIndexExist(std::string_view index_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DoesSchemaItemExist(index_name, "index");
}

bool Database::DoesTableExist(std::string_view table_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DoesSchemaItemExist(table_name, "table");
}

bool Database::DoesViewExist(std::string_view view_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DoesSchemaItemExist(view_name, "view");
}

bool Database::DoesSchemaItemExist(std::string_view name,
                                   std::string_view type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kSql[] =
      "SELECT 1 FROM sqlite_schema WHERE type=? AND name=?";
  Statement statement(GetUniqueStatement(kSql));

  if (!statement.is_valid()) {
    // The database is corrupt.
    return false;
  }

  statement.BindString(0, type);
  statement.BindString(1, name);

  return statement.Step();  // Table exists if any row was returned.
}

bool Database::DoesColumnExist(base::cstring_view table_name,
                               base::cstring_view column_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return false;
  }

  // sqlite3_table_column_metadata uses out-params to return column definition
  // details, such as the column type and whether it allows NULL values. These
  // aren't needed to compute the current method's result, so we pass in nullptr
  // for all the out-params.
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_table_column_metadata(
      db_, "main", table_name.c_str(), column_name.c_str(),
      /* pzDataType= */ nullptr,
      /* pzCollSeq= */ nullptr, /* pNotNull= */ nullptr,
      /* pPrimaryKey= */ nullptr, /* pAutoinc= */ nullptr));
  return sqlite_result_code == SqliteResultCode::kOk;
}

int64_t Database::GetLastInsertRowId() const {
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return 0;
  }
  int64_t last_rowid = sqlite3_last_insert_rowid(db_);
  DCHECK(last_rowid != 0) << "No successful INSERT in a table with ROWID";
  return last_rowid;
}

int64_t Database::GetLastChangeCount() {
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return 0;
  }
  return sqlite3_changes64(db_);
}

int Database::GetMemoryUsage() {
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return 0;
  }

  // The following calls all set the high watermark to zero.
  // See https://www.sqlite.org/c3ref/c_dbstatus_options.html
  int high_watermark = 0;

  int cache_memory = 0, schema_memory = 0, statement_memory = 0;

  auto sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_CACHE_USED, &cache_memory, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_CACHE_USED) failed";

#if DCHECK_IS_ON()
  int shared_cache_memory = 0;
  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_status(db_, SQLITE_DBSTATUS_CACHE_USED_SHARED,
                        &shared_cache_memory, &high_watermark, /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_CACHE_USED_SHARED) failed";
  DCHECK_EQ(shared_cache_memory, cache_memory)
      << "Memory counting assumes that each database uses a private page cache";
#endif  // DCHECK_IS_ON()

  sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_SCHEMA_USED, &schema_memory, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_SCHEMA_USED) failed";

  sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_STMT_USED, &statement_memory, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_STMT_USED) failed";

  return cache_memory + schema_memory + statement_memory;
}

int Database::GetErrorCode() const {
  if (!db_) {
    return SQLITE_ERROR;
  }
  return sqlite3_extended_errcode(db_);
}

int Database::GetLastErrno() const {
  if (!db_) {
    return -1;
  }

  int err = 0;
  if (SQLITE_OK !=
      sqlite3_file_control(db_, nullptr, SQLITE_FCNTL_LAST_ERRNO, &err)) {
    return -2;
  }

  return err;
}

const char* Database::GetErrorMessage() const {
  if (!db_) {
    return "sql::Database is not opened.";
  }
  return sqlite3_errmsg(db_);
}

bool Database::OpenInternal(const std::string& db_file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("sql", "Database::OpenInternal", "path", db_file_path);

  if (is_open()) {
    DLOG(FATAL) << "sql::Database is already open.";
    return false;
  }

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  EnsureSqliteInitialized();

  // If |poisoned_| is set, it means an error handler called
  // RazeAndPoison().  Until regular Close() is called, the caller
  // should be treating the database as open, but is_open() currently
  // only considers the sqlite3 handle's state.
  DCHECK(!poisoned_) << "sql::Database is already open.";
  poisoned_ = false;

  // The flags are documented at https://www.sqlite.org/c3ref/open.html.
  //
  // Chrome uses SQLITE_OPEN_PRIVATECACHE because SQLite is used by many
  // disparate features with their own databases, and having separate page
  // caches makes it easier to reason about each feature's performance in
  // isolation.
  //
  // SQLITE_OPEN_EXRESCODE enables the full range of SQLite error codes. See
  // https://www.sqlite.org/rescode.html for details.
  int open_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                   SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_PRIVATECACHE;
  std::string uri_file_path = db_file_path;
  if (options_.exclusive_database_file_lock) {
#if BUILDFLAG(IS_WIN)
    const bool in_memory = db_file_path == kSqliteOpenInMemoryPath;
    if (!in_memory) {
      // Do not allow query injection.
      if (base::Contains(db_file_path, '?')) {
        return false;
      }
      open_flags |= SQLITE_OPEN_URI;
      uri_file_path = base::StrCat({"file:", db_file_path, "?exclusive=true"});
    }
#else
    NOTREACHED()
        << "exclusive_database_file_lock is only supported on Windows.";
#endif  // BUILDFLAG(IS_WIN)
  }

  sqlite3* db = nullptr;
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_open_v2(
      uri_file_path.c_str(), &db, open_flags, /*zVfs=*/nullptr));
  if (sqlite_result_code == SqliteResultCode::kOk) {
    db_ = db;
  } else {
    // sqlite3_open_v2() will usually create a database connection handle, even
    // if an error occurs (see https://www.sqlite.org/c3ref/open.html).
    if (db) {
      // Deallocate resources allocated during the failed open.
      // See https://www.sqlite.org/c3ref/close.html.
      sqlite3_close(db);
    }

    OnSqliteError(ToSqliteErrorCode(sqlite_result_code), nullptr,
                  "-- sqlite3_open_v2()");
    return false;
  }

  ConfigureSqliteDatabaseObject();

  // If indicated, enable shared mode ("NORMAL") on the database, so it can be
  // opened by multiple processes. This needs to happen before WAL mode is
  // enabled.
  //
  // TODO(crbug.com/40146017): Remove support for non-exclusive mode.
  static_assert(
      SQLITE_DEFAULT_LOCKING_MODE == 1,
      "Chrome assumes SQLite is configured to default to EXCLUSIVE locking");
  if (!options_.exclusive_locking) {
    if (!Execute("PRAGMA locking_mode=NORMAL")) {
      return false;
    }
  }

  // The sqlite3_open*() methods only perform I/O on the database file if a hot
  // journal is found. Force SQLite to parse the header and database schema, so
  // we can signal irrecoverable corruption early.
  //
  // sqlite3_table_column_metadata() causes SQLite to parse the database schema.
  // Since the schema is stored inside a table B-tree, parsing the schema
  // implies parsing the database header.
  //
  // sqlite3_table_column_metadata() can be used with a null database name, but
  // that will cause it to search for the table in all databases that are
  // ATTACHed to the connection. While Chrome features (almost) never use
  // ATTACHed databases, we prefer to be explicit here.
  //
  // sqlite3_table_column_metadata() can be used with a null column name, and
  // will report on the existence of the table with the given name. This is
  // sufficient for the purpose of getting SQLite to parse the database schema.
  // See https://www.sqlite.org/c3ref/table_column_metadata.html for details.
  static constexpr char kSqliteSchemaTable[] = "sqlite_schema";
  sqlite_result_code = ToSqliteResultCode(sqlite3_table_column_metadata(
      db_, kSqliteMainDatabaseName, kSqliteSchemaTable, /*zColumnName=*/nullptr,
      /*pzDataType=*/nullptr, /*pzCollSeq=*/nullptr, /*pNotNull=*/nullptr,
      /*pPrimaryKey=*/nullptr, /*pAutoinc=*/nullptr));
  if (sqlite_result_code != SqliteResultCode::kOk) {
    OnSqliteError(ToSqliteErrorCode(sqlite_result_code), nullptr,
                  "-- sqlite3_table_column_metadata()");
    return false;
  }

  const base::TimeDelta kBusyTimeout = base::Seconds(kBusyTimeoutSeconds);

  // Needs to happen before entering WAL mode. Will only work if this the first
  // time the database is being opened in WAL mode.
  const std::string page_size_sql =
      base::StringPrintf("PRAGMA page_size=%d", options_.page_size);
  if (!ExecuteWithTimeout(page_size_sql, kBusyTimeout)) {
    return false;
  }

  // https://www.sqlite.org/pragma.html#pragma_journal_mode
  // WAL - Use a write-ahead log instead of a journal file.
  // DELETE (default) - delete -journal file to commit.
  // TRUNCATE - truncate -journal file to commit.
  // PERSIST - zero out header of -journal file to commit.
  // TRUNCATE should be faster than DELETE because it won't need directory
  // changes for each transaction.  PERSIST may break the spirit of using
  // secure_delete.
  //
  // Needs to be performed after setting exclusive locking mode. Otherwise can
  // fail if underlying VFS doesn't support shared memory.
  if (UseWALMode()) {
    // Set the synchronous flag to NORMAL. This means that writers don't flush
    // the WAL file after every write. The WAL file is only flushed on a
    // checkpoint. In this case, transcations might lose durability on a power
    // loss (but still durable after an application crash).
    // TODO(shuagga@microsoft.com): Evaluate if this loss of durability is a
    // concern.
    if (!Execute("PRAGMA synchronous=NORMAL")) {
      return false;
    }

    // Opening the db in WAL mode can fail (eg if the underlying VFS doesn't
    // support shared memory and we are not in exclusive locking mode).
    if (!Execute("PRAGMA journal_mode=WAL")) {
      return false;
    }
  } else {
    // For speed, change the journal mode from the default DELETE to TRUNCATE.
    // Both modes will delete the rollback journal at the conclusion of every
    // transaction, but TRUNCATE is faster because it avoids touching the
    // journal's parent directory[0].
    //
    // PERSIST may be even faster because it zeroes out the journal's header
    // without fully deleting its contents. Chrome used PERSIST until 2015, but
    // switched to TRUNCATE to ensure that potentially-sensitive information is
    // deleted from disk[1].
    //
    // Per the SQLite docs[2], setting the journal mode has a sharp edge: the
    // operation may succeed without actually changing the mode! It only makes
    // sense to tolerate this successful failure because the default mode also
    // deletes the journal's contents.
    //
    // [0]: https://crbug.com/118470#c4
    // [1]: https://crbug.com/493008
    // [2]: https://www.sqlite.org/pragma.html#pragma_journal_mode
    if (!Execute("PRAGMA journal_mode=TRUNCATE")) {
      return false;
    }
  }
  CHECK(db_);

  if (options_.flush_to_media) {
    std::ignore = Execute("PRAGMA fullfsync=1");
  }

  if (options_.cache_size != 0) {
    const std::string cache_size_sql = base::StrCat(
        {"PRAGMA cache_size=", base::NumberToString(options_.cache_size)});
    std::ignore = ExecuteWithTimeout(cache_size_sql, kBusyTimeout);
  }

  static_assert(SQLITE_SECURE_DELETE == 1,
                "Chrome assumes secure_delete is on by default.");

  // When SQLite needs to grow a database file, it uses a configurable
  // increment. Larger values reduce filesystem fragmentation and mmap()
  // churn, as the database file is grown less often. Smaller values waste
  // less disk space.
  //
  // We currently set different values for small vs large files.
  //
  // TODO(crbug.com/40827336): Replace file size-based heuristic with a
  // DatabaseOptions member. Use the DatabaseOptions value for temporary
  // databases as well.
  sqlite3_file* file = GetSqliteVfsFile();

  // GetSqliteVfsFile() returns null for in-memory and temporary databases. This
  // is fine, because these databases start out empty, so the heuristic below
  // would never set a chunk size on them anyway.
  if (file) {
    sqlite3_int64 db_size = 0;
    sqlite_result_code =
        ToSqliteResultCode(file->pMethods->xFileSize(file, &db_size));
    if (sqlite_result_code == SqliteResultCode::kOk && db_size > 16 * 1024) {
      int chunk_size = 4 * 1024;
      if (db_size > 128 * 1024) {
        chunk_size = 32 * 1024;
      }

      sqlite3_file_control(db_, /*zDbName=*/nullptr, SQLITE_FCNTL_CHUNK_SIZE,
                           &chunk_size);
    }
  }

  size_t mmap_size = mmap_disabled_ ? 0 : ComputeMmapSizeForOpen();

  // We explicitly issue a "PRGAMA mmap_size=0" to disable memory-mapping. We
  // could skip executing the PRAGMA in that case, and use a static_assert to
  // ensure that SQLITE_DEFAULT_MMAP_SIZE > 0. We didn't choose this alternative
  // because would cost us a bit more logic, and the optimization would apply to
  // edge cases, such as in-memory databases.  More details at
  // https://www.sqlite.org/pragma.html#pragma_mmap_size.
  std::ignore = Execute(
      base::StrCat({"PRAGMA mmap_size=", base::NumberToString(mmap_size)}));

  // Determine if memory-mapping has actually been enabled.  The Execute() above
  // can succeed without changing the amount mapped.
  mmap_enabled_ = false;
  {
    Statement pragma_mmap_size(GetUniqueStatement("PRAGMA mmap_size"));
    if (pragma_mmap_size.Step() && pragma_mmap_size.ColumnInt64(0) > 0) {
      mmap_enabled_ = true;
    }
  }

  DCHECK(!memory_dump_provider_);
  memory_dump_provider_ =
      std::make_unique<DatabaseMemoryDumpProvider>(db_, histogram_tag_);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      memory_dump_provider_.get(), "sql::Database", /*task_runner=*/nullptr);

  return true;
}

void Database::ConfigureSqliteDatabaseObject() {
  // The use of SQLite's non-standard string quoting is not allowed in Chrome.
  //
  // Allowing double-quoted string literals is now considered a misfeature by
  // SQLite authors. See https://www.sqlite.org/quirks.html#dblquote
  auto sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_DQS_DDL, 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config(SQLITE_DBCONFIG_DQS_DDL) should not fail";
  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_DQS_DML, 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config(SQLITE_DBCONFIG_DQS_DML) should not fail";

  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_FKEY, 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_FKEY) should not fail";

  // The use of triggers is discouraged for Chrome code. Thanks to this
  // configuration change, triggers are not executed. CREATE TRIGGER and DROP
  // TRIGGER still succeed.
  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_TRIGGER, 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config() should not fail";

  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_VIEW,
                        options_.enable_views_discouraged ? 1 : 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config() should not fail";
}

void Database::DoRollback() {
  TRACE_EVENT0("sql", "Database::DoRollback");

  Statement rollback(GetCachedStatement(SQL_FROM_HERE, "ROLLBACK"));

  rollback.Run();

  // The cache may have been accumulating dirty pages for commit.  Note that in
  // some cases sql::Transaction can fire rollback after a database is closed.
  if (is_open()) {
    ReleaseCacheMemoryIfNeeded(false);
  }

  needs_rollback_ = false;
}

void Database::StatementRefCreated(StatementRef* ref) {
  DCHECK(!open_statements_.count(ref))
      << __func__ << " already called with this statement";
  open_statements_.insert(ref);
}

void Database::StatementRefDeleted(StatementRef* ref) {
  DCHECK(open_statements_.count(ref))
      << __func__ << " called with non-existing statement";
  open_statements_.erase(ref);
}

void Database::OnSqliteError(SqliteErrorCode sqlite_error_code,
                             sql::Statement* statement,
                             const char* sql_statement) {
  TRACE_EVENT0("sql", "Database::OnSqliteError");

  DCHECK_NE(statement != nullptr, sql_statement != nullptr)
      << __func__ << " should either get a Statement or a raw SQL string";

  // Log errors for developers.
  //
  // This block is wrapped around a DCHECK_IS_ON() check so we don't waste CPU
  // cycles computing the strings that make up the log message in production.
#if DCHECK_IS_ON()
  std::string logged_statement;
  if (statement) {
    logged_statement = statement->GetSQLStatement();
  } else {
    logged_statement = sql_statement;
  }

  std::string database_id = histogram_tag_;
  if (database_id.empty()) {
    database_id = DbPath().BaseName().AsUTF8Unsafe();
  }

  // This logging block cannot be a DCHECK, because valid usage of sql::Database
  // can still encounter SQLite errors in production. For example, valid SQL
  // statements can fail when a database is corrupted.
  //
  // This logging block should not use LOG(ERROR) because many features built on
  // top of sql::Database can recover from most errors.
  DVLOG(1) << "SQLite error! This may indicate a programming error!\n"
           << "Database: " << database_id
           << " sqlite_error_code: " << sqlite_error_code
           << " errno: " << GetLastErrno()
           << "\nSQLite error description: " << GetErrorMessage()
           << "\nSQL statement: " << logged_statement;
#endif  // DCHECK_IS_ON()

  // Inform the error expecter that we've encountered the error.
  std::ignore = IsExpectedSqliteError(static_cast<int>(sqlite_error_code));

  if (!error_callback_.is_null()) {
    // Create an additional reference to the state in `error_callback_`, so the
    // state doesn't go away if the callback changes `error_callback_` by
    // calling set_error_callback() or reset_error_callback(). This avoids a
    // subtle source of use-after-frees. See https://crbug.com/254584.
    ErrorCallback error_callback_copy = error_callback_;
    error_callback_copy.Run(static_cast<int>(sqlite_error_code), statement);
    return;
  }
}

std::string Database::GetDiagnosticInfo(int sqlite_error_code,
                                        Statement* statement,
                                        DatabaseDiagnostics* diagnostics) {
  DCHECK_NE(sqlite_error_code, SQLITE_OK)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_DONE)
      << __func__ << " received non-error result code";
  DCHECK_NE(sqlite_error_code, SQLITE_ROW)
      << __func__ << " received non-error result code";

  // Prevent reentrant calls to the error callback.
  ErrorCallback original_callback = std::move(error_callback_);
  error_callback_.Reset();

  if (diagnostics) {
    diagnostics->reported_sqlite_error_code = sqlite_error_code;
  }

  // Trim extended error codes.
  const int primary_error_code = sqlite_error_code & 0xff;

  // CollectCorruptionInfo() is implemented in terms of sql::Database,
  // TODO(shess): Rewrite IntegrityCheckHelper() in terms of raw SQLite.
  std::string result =
      (primary_error_code == SQLITE_CORRUPT)
          ? CollectCorruptionInfo()
          : CollectErrorInfo(sqlite_error_code, statement, diagnostics);

  // The following queries must be executed after CollectErrorInfo() above, so
  // if they result in their own errors, they don't interfere with
  // CollectErrorInfo().
  const bool has_valid_header = Execute("PRAGMA auto_vacuum");
  const bool has_valid_schema = Execute("SELECT COUNT(*) FROM sqlite_schema");

  // Restore the original error callback.
  error_callback_ = std::move(original_callback);

  base::StringAppendF(&result, "Has valid header: %s\n",
                      (has_valid_header ? "Yes" : "No"));
  base::StringAppendF(&result, "Has valid schema: %s\n",
                      (has_valid_schema ? "Yes" : "No"));
  if (diagnostics) {
    diagnostics->has_valid_header = has_valid_header;
    diagnostics->has_valid_schema = has_valid_schema;
  }

  return result;
}

bool Database::FullIntegrityCheck(std::vector<std::string>* messages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  messages->clear();

  // The PRAGMA below has the side effect of setting SQLITE_RecoveryMode, which
  // allows SQLite to process through certain cases of corruption.
  if (!Execute("PRAGMA writable_schema=ON")) {
    // The "PRAGMA integrity_check" statement executed below may return less
    // useful information. However, incomplete information is still better than
    // nothing, so we press on.
    messages->push_back("PRAGMA writable_schema=ON failed");
  }

  // We need to bypass sql::Statement and use raw SQLite C API calls here.
  //
  // "PRAGMA integrity_check" reports SQLITE_CORRUPT when the database is
  // corrupt. Reporting SQLITE_CORRUPT is undesirable in this case, because it
  // causes our sql::Statement infrastructure to call the database error
  // handler, which triggers feature-level error handling. However,
  // FullIntegrityCheck() callers presumably already know that the database is
  // corrupted, and are trying to collect diagnostic information for reporting.
  sqlite3_stmt* statement = nullptr;

  // https://www.sqlite.org/c3ref/prepare.html states that SQLite will perform
  // slightly better if sqlite_prepare_v3() receives a zero-terminated statement
  // string, and a statement size that includes the zero byte. Fortunately,
  // C++'s string literal and sizeof() operator do exactly that.
  constexpr char kIntegrityCheckSql[] = "PRAGMA integrity_check";
  const auto prepare_result_code = ToSqliteResultCode(
      sqlite3_prepare_v3(db_, kIntegrityCheckSql, sizeof(kIntegrityCheckSql),
                         SqlitePrepareFlags(), &statement, /*pzTail=*/nullptr));
  if (prepare_result_code != SqliteResultCode::kOk) {
    return false;
  }

  // "PRAGMA integrity_check" currently returns multiple lines as a single row.
  //
  // However, since https://www.sqlite.org/pragma.html#pragma_integrity_check
  // states that multiple records may be returned, the code below can handle
  // multiple records, each of which has multiple lines.
  std::vector<std::string> result_lines;

  while (ToSqliteResultCode(sqlite3_step(statement)) ==
         SqliteResultCode::kRow) {
    const uint8_t* row = chrome_sqlite3_column_text(statement, /*iCol=*/0);
    DCHECK(row) << "PRAGMA integrity_check should never return NULL rows";

    const int row_size = sqlite3_column_bytes(statement, /*iCol=*/0);
    std::string_view row_string(reinterpret_cast<const char*>(row), row_size);

    const std::vector<std::string_view> row_lines = base::SplitStringPiece(
        row_string, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (std::string_view row_line : row_lines) {
      result_lines.emplace_back(row_line);
    }
  }

  const auto finalize_result_code =
      ToSqliteResultCode(sqlite3_finalize(statement));
  // sqlite3_finalize() may return SQLITE_CORRUPT when the integrity check
  // discovers any problems. We still consider this case a success, as long as
  // the statement produced at least one diagnostic message.
  const bool success = (result_lines.size() > 0) ||
                       (finalize_result_code == SqliteResultCode::kOk);
  *messages = std::move(result_lines);

  // Best-effort attempt to undo the "PRAGMA writable_schema=ON" executed above.
  std::ignore = Execute("PRAGMA writable_schema=OFF");

  return success;
}

bool Database::ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                                 const std::string& dump_name) {
  return memory_dump_provider_ &&
         memory_dump_provider_->ReportMemoryUsage(pmd, dump_name);
}

bool Database::UseWALMode() const {
#if BUILDFLAG(IS_FUCHSIA)
  // WAL mode is only enabled on Fuchsia for databases with exclusive
  // locking, because this case does not require shared memory support.
  // At the time this was implemented (May 2020), Fuchsia's shared
  // memory support was insufficient for SQLite's needs.
  return options_.wal_mode && options_.exclusive_locking;
#else
  return options_.wal_mode;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

bool Database::CheckpointDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  auto sqlite_result_code = ToSqliteResultCode(sqlite3_wal_checkpoint_v2(
      db_, kSqliteMainDatabaseName, SQLITE_CHECKPOINT_PASSIVE,
      /*pnLog=*/nullptr, /*pnCkpt=*/nullptr));

  return sqlite_result_code == SqliteResultCode::kOk;
}

}  // namespace sql
