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

#include <algorithm>
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
#include "base/feature_list.h"
#include "base/files/drive_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
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
#include "sql/streaming_blob_handle.h"
#include "sql/transaction.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace sql {

namespace {

// Features to evaluate the hypothesis that preloading sql::Database causes
// memory contention (using Browser.MainThreadsCongestion as a proxy) for
// minimal gains.
//
// Context: We previously validated that preloading the main DLL causes memory
// contention, and the benefits don't outweigh this downside on fixed SSDs.
//
// When enabled, the "preload" option is ignored unconditionally.
BASE_FEATURE(kInhibitSQLPreload, base::FEATURE_DISABLED_BY_DEFAULT);
//
// When enabled, the "preload" option is ignored *only if the database is on a
// fixed SSD*.
BASE_FEATURE(kInhibitSQLPreloadOnFixedSSD, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the call to ReleaseCacheMemoryIfNeeded are ignored.
BASE_FEATURE(kInhibitSQLReleaseCacheMemoryIfNeeded,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Returns true if `path` is on a drive that has no seek penalty and isn't
// removable, or if that information cannot be obtained (most drives are fixed
// and have no seek penalty, so `true` is the result that is most likely to be
// correct).
bool FilePathIsFixedSSD(const base::FilePath& path) {
  std::optional<base::DriveInfo> drive_info = base::GetFileDriveInfo(path);
  if (!drive_info) {
    return true;
  }

  return !drive_info->has_seek_penalty.value_or(false)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
         && !drive_info->is_removable.value_or(false)
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
         && !drive_info->is_usb.value_or(false)
#endif
      ;
}

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

constexpr int kPrepareFlags = SQLITE_PREPARE_NO_VTAB;

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
  return std::ranges::all_of(attachment_point,
                             [](char ch) { return base::IsAsciiLower(ch); });
}

std::string AsUTF8ForSQL(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(path.value());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return path.value();
#endif
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OpenDatabaseFailedReason)
enum class OpenDatabaseFailedReason {
  kAlreadyOpened = 0,
  kIncorrectPath = 1,
  kSqliteOpenFailed = 2,
  kLockingModeFailed = 3,
  kMetadataLoadingFailed = 4,
  kPageSizeFailed = 5,
  kPragmaSynchronousFailed = 6,
  kPragmaJournalFailed = 7,
  kMaxValue = kPragmaJournalFailed
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/enums.xml)
// Reports the reason for a failure in Database::Open(...).
void RecordOpenDatabaseFailureReason(const std::string& histogram_tag,
                                     OpenDatabaseFailedReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sql.Database.Open.FailureReason.", histogram_tag}),
      reason);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(RazeDatabaseFailedReason)
enum class RazeDatabaseFailedReason {
  kPoisoned = 0,
  kPendingTransaction = 1,
  kCantOpenInMemory = 2,
  kAutoVacuumFailed = 3,
  kSchemaFailed = 4,
  kLocked = 5,
  kTruncateFailed = 6,
  kBackupFailed = 7,
  kPageSizeFailed = 8,
  kUnknownError = 9,
  kCheckpointFailed = 10,
  kMaxValue = kCheckpointFailed
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/sql/enums.xml)
// Reports the reason for a failure in Database::Raze(...).
void RecordRazeDatabaseFailureReason(const std::string& histogram_tag,
                                     RazeDatabaseFailedReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sql.Database.Raze.FailureReason.", histogram_tag}),
      reason);
}

}  // namespace

DatabaseOptions::DatabaseOptions() = default;
DatabaseOptions::DatabaseOptions(const DatabaseOptions&) = default;
DatabaseOptions::DatabaseOptions(DatabaseOptions&&) = default;
DatabaseOptions& DatabaseOptions::operator=(const DatabaseOptions&) = default;
DatabaseOptions& DatabaseOptions::operator=(DatabaseOptions&&) = default;
DatabaseOptions::~DatabaseOptions() = default;

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

// static
int Database::WalHookCallback(void* db_ptr,
                              sqlite3* db_handle,
                              const char* db_name,
                              int pages) {
  Database* self = reinterpret_cast<Database*>(db_ptr);
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
  self->options_.wal_commit_callback_.Run(pages);
  return SQLITE_OK;
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

void Database::StatementRef::Reset(bool clear_bound_variables) {
  if (clear_bound_variables) {
    std::ignore = ToSqliteResultCode(sqlite3_clear_bindings(stmt()));
    bound_blobs_.clear();
  }

  // ToSqliteResultCode() is called to ensure that sqlite3_reset() doesn't
  // return a concerning code, such as SQLITE_MISUSE. The processed error code
  // is ignored because sqlite3_reset() returns an error code if the last
  // sqlite3_step() failed, and that error was already reported when we ran
  // sqlite3_step(), via Statement::Run() or Statement::Step().
  std::ignore = ToSqliteResultCode(sqlite3_reset(stmt()));
}

base::span<const uint8_t> Database::StatementRef::TakeBlobMemory(
    int param_index,
    scoped_refptr<base::RefCountedMemory> blob) {
  auto inserted = bound_blobs_.emplace(param_index, std::move(blob));
  CHECK(inserted.second) << "Parameter unexpectedly bound twice: "
                         << param_index;
  return *inserted.first->second;
}

void Database::StatementRef::ClearBlobMemory(int param_index) {
  bound_blobs_.erase(param_index);
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

    bound_blobs_.clear();
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

Database::Database(Database::Tag tag) : Database(DatabaseOptions{}, tag) {}

Database::Database(DatabaseOptions options, Database::Tag tag)
    : options_(options),
      mmap_disabled_(!options.mmap_enabled_),
      histogram_tag_(tag.value),
      tracing_track_name_(base::StrCat({"Database: ", histogram_tag_})) {
  DCHECK_GE(options.page_size_, 512);
  DCHECK_LE(options.page_size_, 65536);
  DCHECK(!(options.page_size_ & (options.page_size_ - 1)))
      << "page_size must be a power of two";
  DCHECK(!options_.mmap_alt_status_discouraged_ ||
         options_.enable_views_discouraged_)
      << "mmap_alt_status requires views";

  // It's valid to construct a database on a sequence and then pass it to a
  // different sequence before usage.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Database::~Database() {
  Close();
}

bool Database::Open(const base::FilePath& path) {
  std::string path_string = AsUTF8ForSQL(path);
  TRACE_EVENT1("sql", "Database::Open", "path", path_string);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!path.empty());
  DCHECK_NE(path_string, kSqliteOpenInMemoryPath)
      << "Path conflicts with SQLite magic identifier";

  // Preload the database before opening it to ensure it's working with the
  // exclusive mode.
  if (options_.preload_) {
    PreloadInternal(path);
  }

  {
    ScopedOpenErrorReporter reporter(this,
                                     "Sql.Database.Open.FirstAttempt.Error");
    if (OpenInternal(path_string)) {
      return true;
    }
  }
  // OpenInternal() may have run the error callback before returning false. If
  // the error callback poisoned `this`, the database may have been recovered or
  // razed, so a second attempt may succeed.
  if (poisoned_) {
    Close();
    {
      ScopedOpenErrorReporter reporter(this,
                                       "Sql.Database.Open.SecondAttempt.Error");
      return OpenInternal(path_string);
    }
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

  CHECK_EQ(outstanding_blob_count_, 0U)
      << "All StreamingBlobHandles should be destroyed before closing "
         "sql::Database";

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

    CHECK_NE(sqlite_result_code, SqliteResultCode::kBusy,
             base::NotFatalUntil::M141)
        << "sqlite3_close() called while resources (statements, blobs, etc) "
           "are still alive";
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
  if (base::FeatureList::IsEnabled(kInhibitSQLReleaseCacheMemoryIfNeeded)) {
    return;
  }

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
  std::optional<int64_t> db_size = GetFileSize(db_path);
  if (db_size && *db_size < 0) {
    return std::string();
  }

  // Buffer for accumulating debugging info about the error.  Place
  // more-relevant information earlier, in case things overflow the
  // fixed-size reporting buffer.
  std::string debug_info;
  base::StringAppendF(&debug_info, "SQLITE_CORRUPT, db size %" PRId64 "\n",
                      *db_size);

  // Only check files up to 8M to keep things from blocking too long.
  const int64_t kMaxIntegrityCheckSize = 8192 * 1024;
  if (*db_size > kMaxIntegrityCheckSize) {
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

void Database::RecordTimingHistogram(std::string_view name_prefix,
                                     base::TimeDelta timing) const {
  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat({name_prefix, histogram_tag()}), timing,
      base::Microseconds(0), base::Minutes(1), 100);
}

perfetto::NamedTrack Database::GetTracingNamedTrack() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return perfetto::NamedTrack(perfetto::DynamicString(tracing_track_name_),
                              reinterpret_cast<uint64_t>(this),
                              perfetto::ThreadTrack::Current());
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
bool Database::RazeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  if (!db_) {
    DCHECK(poisoned_) << "Cannot raze null db";
    RecordRazeDatabaseFailureReason(histogram_tag_,
                                    RazeDatabaseFailedReason::kPoisoned);
    return false;
  }

  DCHECK_GE(transaction_nesting_, 0);
  if (transaction_nesting_ > 0) {
    RecordRazeDatabaseFailureReason(
        histogram_tag_, RazeDatabaseFailedReason::kPendingTransaction);
    return false;
  }

  Database null_db(
      DatabaseOptions()
          .set_exclusive_locking(true)
          .set_page_size(options_.page_size_)
          .set_enable_views_discouraged(options_.enable_views_discouraged_),
      "RazeNullDB");
  if (!null_db.OpenInMemory()) {
    DLOG(FATAL) << "Unable to open in-memory database.";
    RecordRazeDatabaseFailureReason(
        histogram_tag_, RazeDatabaseFailedReason::kCantOpenInMemory);
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
    RecordRazeDatabaseFailureReason(
        histogram_tag_, RazeDatabaseFailedReason::kAutoVacuumFailed);
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
    RecordRazeDatabaseFailureReason(histogram_tag_,
                                    RazeDatabaseFailedReason::kSchemaFailed);
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
    RecordRazeDatabaseFailureReason(histogram_tag_,
                                    RazeDatabaseFailedReason::kLocked);
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
      RecordRazeDatabaseFailureReason(
          histogram_tag_, RazeDatabaseFailedReason::kTruncateFailed);
      return false;
    }

    sqlite_result_code = BackupDatabaseForRaze(null_db.db_, db_);
    if (sqlite_result_code != SqliteResultCode::kDone) {
      RecordRazeDatabaseFailureReason(histogram_tag_,
                                      RazeDatabaseFailedReason::kBackupFailed);
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
        {"PRAGMA page_size=", base::NumberToString(options_.page_size_)});
    if (!Execute(page_size_sql)) {
      RecordRazeDatabaseFailureReason(
          histogram_tag_, RazeDatabaseFailedReason::kPageSizeFailed);
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
      RecordRazeDatabaseFailureReason(histogram_tag_,
                                      RazeDatabaseFailedReason::kBackupFailed);

      return false;
    }
  }

  if (sqlite_result_code != SqliteResultCode::kDone) {
    NOTIMPLEMENTED() << "Unhandled sqlite3_backup_step() error: "
                     << sqlite_result_code;
    RecordRazeDatabaseFailureReason(histogram_tag_,
                                    RazeDatabaseFailedReason::kUnknownError);
    base::UmaHistogramSparse(
        base::StrCat(
            {"Sql.Database.Raze.UnhandledErrorCode.", histogram_tag()}),
        static_cast<int>(sqlite_result_code));
    return false;
  }

  // Checkpoint to propagate transactions to the database file and empty the WAL
  // file.
  // The database can still contain old data if the Checkpoint fails so fail the
  // Raze.
  if (!CheckpointDatabase()) {
    RecordRazeDatabaseFailureReason(
        histogram_tag_, RazeDatabaseFailedReason::kCheckpointFailed);
    return false;
  }

  return true;
}

bool Database::Raze() {
  TRACE_EVENT0("sql", "Database::Raze");

  base::ElapsedTimer raze_timer;
  bool result = RazeInternal();
  RecordTimingHistogram("Sql.Database.RazeTime.", raze_timer.Elapsed());

  return result;
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

  // The commit can fail with error code like SQLITE_BUSY or SQLITE_ERROR. In
  // these cases, the transaction is not rollback and is kept alive. The call
  // to sqlite3_get_autocommit(...) can be used to know if there is still a
  // pending transaction or if the connection is back to normal with the
  // autocommit mode (no pending transaction).
  if (!succeeded && is_open() && sqlite3_get_autocommit(db_) == 0) {
    // In modern SQLite (post 3.7.11), rollback is design to be robust and
    // reliable and it will bring back the connection in a clean state.
    DoRollback();
    return false;
  }

  // Release dirty cache pages after the transaction closes.
  ReleaseCacheMemoryIfNeeded(false);

  // There should be no pending transactions.
  if (is_open()) {
    CHECK_NE(sqlite3_get_autocommit(db_), 0);
  }

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
        sqlite3_prepare_v3(db_, sql, /* nByte= */ -1, kPrepareFlags,
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
    MaybeReportErrorDuringOpen(sqlite_result_code);
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
        db_, sql, /*nByte=*/-1, kPrepareFlags, &sqlite_statement, &sql));
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
    StatementRef& statement = *it->second;
    // Statement is in the cache. It should still be valid. We're the only
    // entity invalidating cached statements, and we remove them from the cache
    // when we do that.
    DCHECK(statement.is_valid());
    DCHECK_EQ(base::cstring_view(sqlite3_sql(statement.stmt())), sql)
        << "GetCachedStatement used with same ID but different SQL";

    // Reset the statement so it can be reused.
    statement.Reset(/*clear_bound_variables=*/true);
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
      db_, sql.c_str(), /* nByte= */ -1, kPrepareFlags,
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

std::optional<StreamingBlobHandle> Database::GetStreamingBlob(
    base::cstring_view table,
    base::cstring_view column,
    int64_t row_id,
    bool readonly) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    DCHECK(poisoned_) << "Illegal use of Database without a db";
    return std::nullopt;
  }

  sqlite3_blob* blob_handle = nullptr;
  auto sqlite_result_code =
      sqlite3_blob_open(db_, kSqliteMainDatabaseName, table.c_str(),
                        column.c_str(), row_id, readonly ? 0 : 1, &blob_handle);
  if (sqlite_result_code != SQLITE_OK) {
    OnSqliteError(ToSqliteErrorCode(ToSqliteResultCode((sqlite_result_code))),
                  nullptr, "-- sqlite3_blob_open()");

    return std::nullopt;
  }

  CHECK(blob_handle);
  ++outstanding_blob_count_;
  return StreamingBlobHandle(base::PassKey<Database>(), blob_handle,
                             base::BindOnce(&Database::OnStreamingBlobClosed,
                                            weak_factory_.GetWeakPtr()));
}

void Database::OnStreamingBlobClosed(SqliteResultCode result,
                                     const char* error_source) {
  --outstanding_blob_count_;
  if (handling_error_nesting_ == 0 && !IsSqliteSuccessCode(result)) {
    OnSqliteError(ToSqliteErrorCode(result), nullptr, error_source);
  }
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
    schema += statement.ColumnStringView(0);
    schema += '|';
    schema += statement.ColumnStringView(1);
    schema += '|';
    schema += statement.ColumnStringView(2);
    schema += '|';
    schema += statement.ColumnStringView(3);
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
      db_, sql.c_str(), /* nByte= */ -1, kPrepareFlags,
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

Database::ScopedOpenErrorReporter::ScopedOpenErrorReporter(
    Database* db,
    std::string_view histogram)
    : db_(db), histogram_(histogram) {
  db_->open_error_reporting_callback_ =
      base::BindRepeating(&Database::ScopedOpenErrorReporter::OnErrorDuringOpen,
                          base::Unretained(this));
}

Database::ScopedOpenErrorReporter::~ScopedOpenErrorReporter() {
  db_->open_error_reporting_callback_.Reset();
}

void Database::ScopedOpenErrorReporter::OnErrorDuringOpen(
    SqliteResultCode code) {
  // Use `base::UmaHistogramSparse` because sqlite result codes aren't
  // sequential. The large integers they represent make it so that the
  // non-sparse histograms end up with too many buckets.
  if (db_->histogram_tag().empty()) {
    base::UmaHistogramSparse(base::StrCat({histogram_, ".NoTag"}),
                             static_cast<int>(code));
  } else {
    base::UmaHistogramSparse(
        base::StrCat({histogram_, ".", db_->histogram_tag()}),
        static_cast<int>(code));
  }
}

void Database::MaybeReportErrorDuringOpen(SqliteResultCode code) {
  if (open_error_reporting_callback_) {
    open_error_reporting_callback_.Run(code);
  }
}

bool Database::OpenInternal(const std::string& db_file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("sql", "Database::OpenInternal", "path", db_file_path);
  base::ElapsedTimer timer;

  if (is_open()) {
    DLOG(FATAL) << "sql::Database is already open.";
    RecordOpenDatabaseFailureReason(histogram_tag_,
                                    OpenDatabaseFailedReason::kAlreadyOpened);
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
  int open_flags = SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_PRIVATECACHE;

  if (options_.read_only_) {
    open_flags |= (SQLITE_OPEN_READONLY);
  } else {
    open_flags |= (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  }

  std::string uri_file_path = db_file_path;
  if (options_.exclusive_database_file_lock_) {
#if BUILDFLAG(IS_WIN)
    const bool in_memory = db_file_path == kSqliteOpenInMemoryPath;
    if (!in_memory) {
      // Do not allow query injection.
      if (base::Contains(db_file_path, '?')) {
        RecordOpenDatabaseFailureReason(
            histogram_tag_, OpenDatabaseFailedReason::kIncorrectPath);
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
  SqliteResultCode sqlite_result_code;
  {
    TRACE_EVENT1("sql", "Database::OpenInternal sqlite3_open_v2", "path",
                 db_file_path);
    base::ElapsedTimer library_call_timer;

    sqlite_result_code = ToSqliteResultCode(
        sqlite3_open_v2(uri_file_path.c_str(), &db, open_flags,
                        options_.vfs_name_discouraged_));

    // If SQLITE_OPEN_READWRITE is specified, the database must not be opened in
    // read-only mode. If it is, set the result code to
    // SqliteResultCode::kReadOnly to prevent subsequent statements from
    // executing and to disallow database use. This is crucial because on
    // Windows, SQLite attempts to open the database in read-only mode if the
    // initial read/write attempt fails. See the winOpen SQLite function for
    // details:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/src/src/os_win.c;l=5266-5269;drc=9bf5bea60709d4afa37a085b86de3651b0ddd5c9
    if (sqlite_result_code == SqliteResultCode::kOk && db) {
      const bool is_readonly =
          sqlite3_db_readonly(db, kSqliteMainDatabaseName) == 1;
      if (options_.read_only_) {
        DCHECK(is_readonly);
      } else if (is_readonly) {
        sqlite_result_code = SqliteResultCode::kReadOnly;
      }
    }

    RecordTimingHistogram("Sql.Database.Success.SqliteOpenTime.",
                          library_call_timer.Elapsed());
  }

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

    RecordOpenDatabaseFailureReason(
        histogram_tag_, OpenDatabaseFailedReason::kSqliteOpenFailed);
    MaybeReportErrorDuringOpen(sqlite_result_code);
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
  if (!options_.exclusive_locking_) {
    if (!Execute("PRAGMA locking_mode=NORMAL")) {
      RecordOpenDatabaseFailureReason(
          histogram_tag_, OpenDatabaseFailedReason::kLockingModeFailed);
      return false;
    }
  }

  if (!options_.read_only_) {
    // The sqlite3_open*() methods only perform I/O on the database file if a
    // hot journal is found. Force SQLite to parse the header and database
    // schema, so we can signal irrecoverable corruption early.
    //
    // sqlite3_table_column_metadata() causes SQLite to parse the database
    // schema. Since the schema is stored inside a table B-tree, parsing the
    // schema implies parsing the database header.
    //
    // sqlite3_table_column_metadata() can be used with a null database name,
    // but that will cause it to search for the table in all databases that are
    // ATTACHed to the connection. While Chrome features (almost) never use
    // ATTACHed databases, we prefer to be explicit here.
    //
    // sqlite3_table_column_metadata() can be used with a null column name, and
    // will report on the existence of the table with the given name. This is
    // sufficient for the purpose of getting SQLite to parse the database
    // schema. See https://www.sqlite.org/c3ref/table_column_metadata.html for
    // details.
    static constexpr char kSqliteSchemaTable[] = "sqlite_schema";
    sqlite_result_code = ToSqliteResultCode(sqlite3_table_column_metadata(
        db_, kSqliteMainDatabaseName, kSqliteSchemaTable,
        /*zColumnName=*/nullptr,
        /*pzDataType=*/nullptr, /*pzCollSeq=*/nullptr, /*pNotNull=*/nullptr,
        /*pPrimaryKey=*/nullptr, /*pAutoinc=*/nullptr));
    if (sqlite_result_code != SqliteResultCode::kOk) {
      MaybeReportErrorDuringOpen(sqlite_result_code);
      OnSqliteError(ToSqliteErrorCode(sqlite_result_code), nullptr,
                    "-- sqlite3_table_column_metadata()");
      RecordOpenDatabaseFailureReason(
          histogram_tag_, OpenDatabaseFailedReason::kMetadataLoadingFailed);
      return false;
    }
  }

  const base::TimeDelta kBusyTimeout = base::Seconds(kBusyTimeoutSeconds);

  if (options_.read_only_) {
    // This options isn't compatible with read-only mode.
    CHECK_EQ(options_.page_size_, DatabaseOptions::kDefaultPageSize);
  } else {
    // Needs to happen before entering WAL mode. Will only work if this the
    // first time the database is being opened in WAL mode.
    const std::string page_size_sql =
        base::StringPrintf("PRAGMA page_size=%d", options_.page_size_);
    if (!ExecuteWithTimeout(page_size_sql, kBusyTimeout)) {
      RecordOpenDatabaseFailureReason(
          histogram_tag_, OpenDatabaseFailedReason::kPageSizeFailed);
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
      // Set the synchronous flag, which controls how aggressively SQLite writes
      // data to disk.
      //
      // If `no_sync_on_wal_mode_` is true, this is set to OFF. With
      // synchronous=OFF, SQLite hands data to the OS for writing but doesn't
      // wait for it to complete. This is very fast, but an OS crash or power
      // failure can lead to database corruption. Data is safe from an
      // application crash.
      //
      // Otherwise, this is set to NORMAL. In WAL mode, synchronous=NORMAL means
      // SQLite syncs at critical moments (like checkpoints), but not for every
      // individual transaction. An OS crash or power failure may cause the loss
      // of transactions that occurred since the last checkpoint, but the
      // database file itself will not be corrupted.
      // See https://www.sqlite.org/pragma.html#pragma_synchronous for more
      // details.
      //
      // TODO(shuagga@microsoft.com): Evaluate if this loss of durability is a
      // concern.
      if (!Execute(options_.no_sync_on_wal_mode_
                       ? base::cstring_view("PRAGMA synchronous=OFF")
                       : base::cstring_view("PRAGMA synchronous=NORMAL"))) {
        RecordOpenDatabaseFailureReason(
            histogram_tag_, OpenDatabaseFailedReason::kPragmaSynchronousFailed);
        return false;
      }

      // Opening the db in WAL mode can fail (eg if the underlying VFS doesn't
      // support shared memory and we are not in exclusive locking mode).
      if (!Execute("PRAGMA journal_mode=WAL")) {
        RecordOpenDatabaseFailureReason(
            histogram_tag_, OpenDatabaseFailedReason::kPragmaJournalFailed);
        return false;
      }

      // Register a WAL commit hook to call the caller's `wal_commit_callback_`.
      if (options_.wal_commit_callback_) {
        sqlite3_wal_hook(db_, &Database::WalHookCallback, this);
      }
    } else {
      // For speed, change the journal mode from the default DELETE to TRUNCATE.
      // Both modes will delete the rollback journal at the conclusion of every
      // transaction, but TRUNCATE is faster because it avoids touching the
      // journal's parent directory[0].
      //
      // PERSIST may be even faster because it zeroes out the journal's header
      // without fully deleting its contents. Chrome used PERSIST until 2015,
      // but switched to TRUNCATE to ensure that potentially-sensitive
      // information is deleted from disk[1].
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
        RecordOpenDatabaseFailureReason(
            histogram_tag_, OpenDatabaseFailedReason::kPragmaJournalFailed);
        return false;
      }
    }
  }

  CHECK(db_);

  if (options_.read_only_) {
    // These options are not compatible with read-only mode.
    CHECK(!options_.flush_to_media_);
    CHECK_EQ(options_.cache_size_, 0);
  } else {
    if (options_.flush_to_media_) {
      std::ignore = Execute("PRAGMA fullfsync=1");
    }

    if (options_.cache_size_ != 0) {
      const std::string cache_size_sql = base::StrCat(
          {"PRAGMA cache_size=", base::NumberToString(options_.cache_size_)});
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

    // GetSqliteVfsFile() returns null for in-memory and temporary databases.
    // This is fine, because these databases start out empty, so the heuristic
    // below would never set a chunk size on them anyway.
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
  }

  // How much to map if no errors are found. 50MB encompasses the 99th
  // percentile of Chrome databases in the wild, so this should be good.
  static constexpr size_t kMmapEverything = 256 * 1024 * 1024;
  size_t mmap_size =
      (mmap_disabled_ || !db_ || poisoned_) ? 0 : kMmapEverything;

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

  RecordTimingHistogram("Sql.Database.Success.OpenInternalTime.",
                        timer.Elapsed());

  return is_open();
}

void Database::PreloadInternal(const base::FilePath& path) {
  TRACE_EVENT0("sql", "Database::PreloadInternal");

  // TODO(crbug.com/40904059): Consider moving this to a DCHECK after fixing
  // or migrating callsites that call Preload(...) on in-memory databases.
  if (in_memory_) {
    return;
  }

  if (base::FeatureList::IsEnabled(kInhibitSQLPreload)) {
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (base::FeatureList::IsEnabled(kInhibitSQLPreloadOnFixedSSD) &&
      FilePathIsFixedSSD(path)) {
    return;
  }

  // Maximum number of bytes that will be prefetched from the database.
  //
  // This limit is very aggressive. The main trade-off involved is that having
  // SQLite block on reading from disk has a high impact on Chrome startup cost
  // for the databases that are on the critical path to startup. So, the limit
  // must exceed the expected sizes of databases on the critical path.
  static constexpr int kPreReadSize = 128 * 1024 * 1024;  // 128 MB
  base::PreReadFile(path, /*is_executable=*/false, /*sequential=*/false,
                    kPreReadSize);
}

void Database::ConfigureSqliteDatabaseObject() {
  auto sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_FKEY, 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_FKEY) should not fail";

  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_TRIGGER,
                        options_.enable_triggers_ ? 1 : 0, nullptr));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_config() should not fail";

  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_config(db_, SQLITE_DBCONFIG_ENABLE_VIEW,
                        options_.enable_views_discouraged_ ? 1 : 0, nullptr));
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
  TRACE_EVENT1("sql", "Database::OnSqliteError", "sqlite_error_code",
               sqlite_error_code);

  DCHECK_NE(statement != nullptr, sql_statement != nullptr)
      << __func__ << " should either get a Statement or a raw SQL string";

  base::WeakPtr<Database> weak_this =
      weak_factory_lifetime_tracker_.GetWeakPtr();
  ++handling_error_nesting_;

  // Use `base::UmaHistogramSparse` because sqlite result codes aren't
  // sequential. The large integers they represent make it so that the
  // non-sparse histograms end up with too many buckets.
  if (!histogram_tag().empty()) {
    base::UmaHistogramSparse(
        base::StrCat({"Sql.Database.Statement.Error.", histogram_tag()}),
        static_cast<int>(sqlite_error_code));
  }

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
  }

  if (weak_this) {
    --weak_this->handling_error_nesting_;
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
                         kPrepareFlags, &statement, /*pzTail=*/nullptr));
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
  return options_.wal_mode_ && options_.exclusive_locking_;
#else
  return options_.wal_mode_;
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
