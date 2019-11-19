/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/database.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webdatabase/change_version_data.h"
#include "third_party/blink/renderer/modules/webdatabase/change_version_wrapper.h"
#include "third_party/blink/renderer/modules/webdatabase/database_authorizer.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database_manager.h"
#include "third_party/blink/renderer/modules/webdatabase/database_task.h"
#include "third_party/blink/renderer/modules/webdatabase/database_thread.h"
#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_backend.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_client.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_coordinator.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_statement.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

// Registering "opened" databases with the DatabaseTracker
// =======================================================
// The DatabaseTracker maintains a list of databases that have been
// "opened" so that the client can call interrupt or delete on every database
// associated with a DatabaseContext.
//
// We will only call DatabaseTracker::addOpenDatabase() to add the database
// to the tracker as opened when we've succeeded in opening the database,
// and will set m_opened to true. Similarly, we only call
// DatabaseTracker::removeOpenDatabase() to remove the database from the
// tracker when we set m_opened to false in closeDatabase(). This sets up
// a simple symmetry between open and close operations, and a direct
// correlation to adding and removing databases from the tracker's list,
// thus ensuring that we have a correct list for the interrupt and
// delete operations to work on.
//
// The only databases instances not tracked by the tracker's open database
// list are the ones that have not been added yet, or the ones that we
// attempted an open on but failed to. Such instances only exist in the
// DatabaseServer's factory methods for creating database backends.
//
// The factory methods will either call openAndVerifyVersion() or
// performOpenAndVerify(). These methods will add the newly instantiated
// database backend if they succeed in opening the requested database.
// In the case of failure to open the database, the factory methods will
// simply discard the newly instantiated database backend when they return.
// The ref counting mechanims will automatically destruct the un-added
// (and un-returned) databases instances.

namespace blink {

namespace {

// Stores a cached version of each database, keyed by a unique integer obtained
// by providing an origin-name pair.
class DatabaseVersionCache {
  USING_FAST_MALLOC(DatabaseVersionCache);

 public:
  Mutex& GetMutex() const LOCK_RETURNED(mutex_) { return mutex_; }

  // Registers a globally-unique integer using the string key (reusing it if it
  // already exists), and returns the integer. Currently, these IDs live for the
  // lifetime of the process.
  DatabaseGuid RegisterOriginAndName(const String& origin, const String& name)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_.AssertAcquired();
    String string_id = origin + "/" + name;
    DCHECK(string_id.IsSafeToSendToAnotherThread());
    DatabaseGuid guid = origin_name_to_guid_.at(string_id);
    if (!guid) {
      guid = next_guid_++;
      origin_name_to_guid_.Set(string_id, guid);
    }
    count_.insert(guid);
    return guid;
  }

  // Releases one use of this identifier (corresponding to a call to
  // RegisterOriginAndName). If all uses are released, the cached version will
  // be erased from memory.
  void ReleaseGuid(DatabaseGuid guid) EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_.AssertAcquired();
    DCHECK(count_.Contains(guid));
    if (count_.erase(guid))
      guid_to_version_.erase(guid);
  }

  // The null string is returned only if the cached version has not been set.
  String GetVersion(DatabaseGuid guid) const EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_.AssertAcquired();
    return guid_to_version_.at(guid).IsolatedCopy();
  }

  // Updates the cached version of a database.
  // The null string is treated as the empty string.
  void SetVersion(DatabaseGuid guid, const String& new_version)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_.AssertAcquired();
    guid_to_version_.Set(guid, new_version.IsNull()
                                   ? g_empty_string
                                   : new_version.IsolatedCopy());
  }

 private:
  mutable Mutex mutex_;
  HashMap<String, DatabaseGuid> origin_name_to_guid_ GUARDED_BY(mutex_);
  HashCountedSet<DatabaseGuid> count_ GUARDED_BY(mutex_);
  HashMap<DatabaseGuid, String> guid_to_version_ GUARDED_BY(mutex_);
  DatabaseGuid next_guid_ GUARDED_BY(mutex_) = 1;
};

DatabaseVersionCache& GetDatabaseVersionCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(DatabaseVersionCache, cache, ());
  return cache;
}

}  // namespace

static const char kVersionKey[] = "WebKitDatabaseVersionKey";
static const char kInfoTableName[] = "__WebKitDatabaseInfoTable__";

static String FormatErrorMessage(const char* message,
                                 int sqlite_error_code,
                                 const char* sqlite_error_message) {
  return String::Format("%s (%d %s)", message, sqlite_error_code,
                        sqlite_error_message);
}

static bool RetrieveTextResultFromDatabase(SQLiteDatabase& db,
                                           const String& query,
                                           String& result_string) {
  SQLiteStatement statement(db, query);
  int result = statement.Prepare();

  if (result != kSQLResultOk) {
    DLOG(ERROR) << "Error (" << result
                << ") preparing statement to read text result from database ("
                << query << ")";
    return false;
  }

  result = statement.Step();
  if (result == kSQLResultRow) {
    result_string = statement.GetColumnText(0);
    return true;
  }
  if (result == kSQLResultDone) {
    result_string = String();
    return true;
  }

  DLOG(ERROR) << "Error (" << result << ") reading text result from database ("
              << query << ")";
  return false;
}

static bool SetTextValueInDatabase(SQLiteDatabase& db,
                                   const String& query,
                                   const String& value) {
  SQLiteStatement statement(db, query);
  int result = statement.Prepare();

  if (result != kSQLResultOk) {
    DLOG(ERROR) << "Failed to prepare statement to set value in database ("
                << query << ")";
    return false;
  }

  statement.BindText(1, value);

  result = statement.Step();
  if (result != kSQLResultDone) {
    DLOG(ERROR) << "Failed to step statement to set value in database ("
                << query << ")";
    return false;
  }

  return true;
}

Database::Database(DatabaseContext* database_context,
                   const String& name,
                   const String& expected_version,
                   const String& display_name,
                   uint32_t estimated_size)
    : database_context_(database_context),
      name_(name.IsolatedCopy()),
      expected_version_(expected_version.IsolatedCopy()),
      display_name_(display_name.IsolatedCopy()),
      estimated_size_(estimated_size),
      guid_(0),
      opened_(false),
      new_(false),
      database_authorizer_(kInfoTableName),
      transaction_in_progress_(false),
      is_transaction_queue_enabled_(true) {
  DCHECK(IsMainThread());
  context_thread_security_origin_ =
      database_context_->GetSecurityOrigin()->IsolatedCopy();

  if (name_.IsNull())
    name_ = "";

  {
    auto& cache = GetDatabaseVersionCache();
    MutexLocker locker(cache.GetMutex());
    guid_ = cache.RegisterOriginAndName(GetSecurityOrigin()->ToString(), name);
  }

  filename_ = DatabaseManager::Manager().FullPathForDatabase(
      GetSecurityOrigin(), name_);

  database_thread_security_origin_ =
      context_thread_security_origin_->IsolatedCopy();
  DCHECK(database_context_->GetDatabaseThread());
  DCHECK(database_context_->IsContextThread());
  database_task_runner_ =
      GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess);
}

Database::~Database() {
  // SQLite is "multi-thread safe", but each database handle can only be used
  // on a single thread at a time.
  //
  // For Database, we open the SQLite database on the DatabaseThread, and
  // hence we should also close it on that same thread. This means that the
  // SQLite database need to be closed by another mechanism (see
  // DatabaseContext::stopDatabases()). By the time we get here, the SQLite
  // database should have already been closed.

  DCHECK(!Opened());
}

void Database::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_context_);
  ScriptWrappable::Trace(visitor);
}

bool Database::OpenAndVerifyVersion(bool set_version_in_new_database,
                                    DatabaseError& error,
                                    String& error_message,
                                    V8DatabaseCallback* creation_callback) {
  base::WaitableEvent event;
  if (!GetDatabaseContext()->DatabaseThreadAvailable())
    return false;

  DatabaseTracker::Tracker().PrepareToOpenDatabase(this);
  bool success = false;
  auto task = std::make_unique<DatabaseOpenTask>(
      this, set_version_in_new_database, &event, error, error_message, success);
  GetDatabaseContext()->GetDatabaseThread()->ScheduleTask(std::move(task));
  event.Wait();
  if (creation_callback) {
    if (success && IsNew()) {
      STORAGE_DVLOG(1)
          << "Scheduling DatabaseCreationCallbackTask for database " << this;
      auto task_id = std::make_unique<probe::AsyncTaskId>();
      probe::AsyncTaskScheduled(GetExecutionContext(), "openDatabase",
                                task_id.get());
      GetExecutionContext()
          ->GetTaskRunner(TaskType::kDatabaseAccess)
          ->PostTask(
              FROM_HERE,
              WTF::Bind(&Database::RunCreationCallback, WrapPersistent(this),
                        WrapPersistent(creation_callback), std::move(task_id)));
    }
  }

  return success;
}

void Database::RunCreationCallback(
    V8DatabaseCallback* creation_callback,
    std::unique_ptr<probe::AsyncTaskId> task_id) {
  probe::AsyncTask async_task(GetExecutionContext(), task_id.get());
  creation_callback->InvokeAndReportException(nullptr, this);
}

void Database::Close() {
  DCHECK(GetDatabaseContext()->GetDatabaseThread());
  DCHECK(GetDatabaseContext()->GetDatabaseThread()->IsDatabaseThread());

  {
    MutexLocker locker(transaction_in_progress_mutex_);

    // Clean up transactions that have not been scheduled yet:
    // Transaction phase 1 cleanup. See comment on "What happens if a
    // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
    SQLTransactionBackend* transaction = nullptr;
    while (!transaction_queue_.IsEmpty()) {
      transaction = transaction_queue_.TakeFirst();
      transaction->NotifyDatabaseThreadIsShuttingDown();
    }

    is_transaction_queue_enabled_ = false;
    transaction_in_progress_ = false;
  }

  CloseDatabase();
  GetDatabaseContext()->GetDatabaseThread()->RecordDatabaseClosed(this);
}

SQLTransactionBackend* Database::RunTransaction(SQLTransaction* transaction,
                                                bool read_only,
                                                const ChangeVersionData* data) {
  MutexLocker locker(transaction_in_progress_mutex_);
  if (!is_transaction_queue_enabled_)
    return nullptr;

  SQLTransactionWrapper* wrapper = nullptr;
  if (data) {
    wrapper = MakeGarbageCollected<ChangeVersionWrapper>(data->OldVersion(),
                                                         data->NewVersion());
  }

  auto* transaction_backend = MakeGarbageCollected<SQLTransactionBackend>(
      this, transaction, wrapper, read_only);
  transaction_queue_.push_back(transaction_backend);
  if (!transaction_in_progress_)
    ScheduleTransaction();

  return transaction_backend;
}

void Database::InProgressTransactionCompleted() {
  MutexLocker locker(transaction_in_progress_mutex_);
  transaction_in_progress_ = false;
  ScheduleTransaction();
}

void Database::ScheduleTransaction() {
  transaction_in_progress_mutex_.AssertAcquired();
  SQLTransactionBackend* transaction = nullptr;

  if (is_transaction_queue_enabled_ && !transaction_queue_.IsEmpty())
    transaction = transaction_queue_.TakeFirst();

  if (transaction && GetDatabaseContext()->DatabaseThreadAvailable()) {
    auto task = std::make_unique<DatabaseTransactionTask>(transaction);
    STORAGE_DVLOG(1) << "Scheduling DatabaseTransactionTask " << task.get()
                     << " for transaction " << task->Transaction();
    transaction_in_progress_ = true;
    GetDatabaseContext()->GetDatabaseThread()->ScheduleTask(std::move(task));
  } else {
    transaction_in_progress_ = false;
  }
}

void Database::ScheduleTransactionStep(SQLTransactionBackend* transaction) {
  if (!GetDatabaseContext()->DatabaseThreadAvailable())
    return;

  auto task = std::make_unique<DatabaseTransactionTask>(transaction);
  STORAGE_DVLOG(1) << "Scheduling DatabaseTransactionTask " << task.get()
                   << " for the transaction step";
  GetDatabaseContext()->GetDatabaseThread()->ScheduleTask(std::move(task));
}

SQLTransactionClient* Database::TransactionClient() const {
  return GetDatabaseContext()->GetDatabaseThread()->TransactionClient();
}

SQLTransactionCoordinator* Database::TransactionCoordinator() const {
  return GetDatabaseContext()->GetDatabaseThread()->TransactionCoordinator();
}

// static
const char* Database::DatabaseInfoTableName() {
  return kInfoTableName;
}

void Database::CloseDatabase() {
  if (!opened_.load(std::memory_order_relaxed))
    return;

  opened_.store(false, std::memory_order_release);
  sqlite_database_.Close();
  // See comment at the top this file regarding calling removeOpenDatabase().
  DatabaseTracker::Tracker().RemoveOpenDatabase(this);
  {
    auto& cache = GetDatabaseVersionCache();
    MutexLocker locker(cache.GetMutex());
    cache.ReleaseGuid(guid_);
  }
}

String Database::version() const {
  // Note: In multi-process browsers the cached value may be accurate, but we
  // cannot read the actual version from the database without potentially
  // inducing a deadlock.
  // FIXME: Add an async version getter to the DatabaseAPI.
  return GetCachedVersion();
}

class DoneCreatingDatabaseOnExitCaller {
  STACK_ALLOCATED();

 public:
  DoneCreatingDatabaseOnExitCaller(Database* database)
      : database_(database), open_succeeded_(false) {}
  ~DoneCreatingDatabaseOnExitCaller() {
    if (!open_succeeded_)
      DatabaseTracker::Tracker().FailedToOpenDatabase(database_);
  }

  void SetOpenSucceeded() { open_succeeded_ = true; }

 private:
  CrossThreadPersistent<Database> database_;
  bool open_succeeded_;
};

bool Database::PerformOpenAndVerify(bool should_set_version_in_new_database,
                                    DatabaseError& error,
                                    String& error_message) {
  DoneCreatingDatabaseOnExitCaller on_exit_caller(this);
  DCHECK(error_message.IsEmpty());
  DCHECK_EQ(error,
            DatabaseError::kNone);  // Better not have any errors already.
  // Presumed failure. We'll clear it if we succeed below.
  error = DatabaseError::kInvalidDatabaseState;

  const int kMaxSqliteBusyWaitTime = 30000;

  if (!sqlite_database_.Open(filename_)) {
    ReportSqliteError(sqlite_database_.LastError());
    error_message = FormatErrorMessage("unable to open database",
                                       sqlite_database_.LastError(),
                                       sqlite_database_.LastErrorMsg());
    return false;
  }
  if (!sqlite_database_.TurnOnIncrementalAutoVacuum())
    DLOG(ERROR) << "Unable to turn on incremental auto-vacuum ("
                << sqlite_database_.LastError() << " "
                << sqlite_database_.LastErrorMsg() << ")";

  sqlite_database_.SetBusyTimeout(kMaxSqliteBusyWaitTime);

  String current_version;
  {
    auto& cache = GetDatabaseVersionCache();
    MutexLocker locker(cache.GetMutex());

    current_version = cache.GetVersion(guid_);
    if (!current_version.IsNull()) {
      STORAGE_DVLOG(1) << "Current cached version for guid " << guid_ << " is "
                       << current_version;

      // Note: In multi-process browsers the cached value may be
      // inaccurate, but we cannot read the actual version from the
      // database without potentially inducing a form of deadlock, a
      // busytimeout error when trying to access the database. So we'll
      // use the cached value if we're unable to read the value from the
      // database file without waiting.
      // FIXME: Add an async openDatabase method to the DatabaseAPI.
      const int kNoSqliteBusyWaitTime = 0;
      sqlite_database_.SetBusyTimeout(kNoSqliteBusyWaitTime);
      String version_from_database;
      if (GetVersionFromDatabase(version_from_database, false)) {
        current_version = version_from_database;
        cache.SetVersion(guid_, current_version);
      }
      sqlite_database_.SetBusyTimeout(kMaxSqliteBusyWaitTime);
    } else {
      STORAGE_DVLOG(1) << "No cached version for guid " << guid_;

      SQLiteTransaction transaction(sqlite_database_);
      transaction.begin();
      if (!transaction.InProgress()) {
        ReportSqliteError(sqlite_database_.LastError());
        error_message = FormatErrorMessage(
            "unable to open database, failed to start transaction",
            sqlite_database_.LastError(), sqlite_database_.LastErrorMsg());
        sqlite_database_.Close();
        return false;
      }

      String table_name(kInfoTableName);
      if (!sqlite_database_.TableExists(table_name)) {
        new_ = true;

        if (!sqlite_database_.ExecuteCommand(
                "CREATE TABLE " + table_name +
                " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT "
                "REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
          ReportSqliteError(sqlite_database_.LastError());
          error_message = FormatErrorMessage(
              "unable to open database, failed to create 'info' table",
              sqlite_database_.LastError(), sqlite_database_.LastErrorMsg());
          transaction.Rollback();
          sqlite_database_.Close();
          return false;
        }
      } else if (!GetVersionFromDatabase(current_version, false)) {
        ReportSqliteError(sqlite_database_.LastError());
        error_message = FormatErrorMessage(
            "unable to open database, failed to read current version",
            sqlite_database_.LastError(), sqlite_database_.LastErrorMsg());
        transaction.Rollback();
        sqlite_database_.Close();
        return false;
      }

      if (current_version.length()) {
        STORAGE_DVLOG(1) << "Retrieved current version " << current_version
                         << " from database " << DatabaseDebugName();
      } else if (!new_ || should_set_version_in_new_database) {
        STORAGE_DVLOG(1) << "Setting version " << expected_version_
                         << " in database " << DatabaseDebugName()
                         << " that was just created";
        if (!SetVersionInDatabase(expected_version_, false)) {
          ReportSqliteError(sqlite_database_.LastError());
          error_message = FormatErrorMessage(
              "unable to open database, failed to write current version",
              sqlite_database_.LastError(), sqlite_database_.LastErrorMsg());
          transaction.Rollback();
          sqlite_database_.Close();
          return false;
        }
        current_version = expected_version_;
      }
      cache.SetVersion(guid_, current_version);
      transaction.Commit();
    }
  }

  if (current_version.IsNull()) {
    STORAGE_DVLOG(1) << "Database " << DatabaseDebugName()
                     << " does not have its version set";
    current_version = "";
  }

  // If the expected version isn't the empty string, ensure that the current
  // database version we have matches that version. Otherwise, set an
  // exception.
  // If the expected version is the empty string, then we always return with
  // whatever version of the database we have.
  if ((!new_ || should_set_version_in_new_database) &&
      expected_version_.length() && expected_version_ != current_version) {
    error_message =
        "unable to open database, version mismatch, '" + expected_version_ +
        "' does not match the currentVersion of '" + current_version + "'";
    sqlite_database_.Close();
    return false;
  }

  sqlite_database_.SetAuthorizer(&database_authorizer_);

  // See comment at the top this file regarding calling addOpenDatabase().
  DatabaseTracker::Tracker().AddOpenDatabase(this);
  opened_.store(true, std::memory_order_release);

  // Declare success:
  error = DatabaseError::kNone;  // Clear the presumed error from above.
  on_exit_caller.SetOpenSucceeded();

  if (new_ && !should_set_version_in_new_database) {
    // The caller provided a creationCallback which will set the expected
    // version.
    expected_version_ = "";
  }

  if (GetDatabaseContext()->GetDatabaseThread())
    GetDatabaseContext()->GetDatabaseThread()->RecordDatabaseOpen(this);
  return true;
}

String Database::StringIdentifier() const {
  // Return a deep copy for ref counting thread safety
  return name_.IsolatedCopy();
}

String Database::DisplayName() const {
  // Return a deep copy for ref counting thread safety
  return display_name_.IsolatedCopy();
}

uint32_t Database::EstimatedSize() const {
  return estimated_size_;
}

String Database::FileName() const {
  // Return a deep copy for ref counting thread safety
  return filename_.IsolatedCopy();
}

bool Database::GetVersionFromDatabase(String& version,
                                      bool should_cache_version) {
  String query(String("SELECT value FROM ") + kInfoTableName +
               " WHERE key = '" + kVersionKey + "';");

  database_authorizer_.Disable();

  bool result =
      RetrieveTextResultFromDatabase(sqlite_database_, query, version);
  if (result) {
    if (should_cache_version)
      SetCachedVersion(version);
  } else {
    DLOG(ERROR) << "Failed to retrieve version from database "
                << DatabaseDebugName();
  }

  database_authorizer_.Enable();

  return result;
}

bool Database::SetVersionInDatabase(const String& version,
                                    bool should_cache_version) {
  // The INSERT will replace an existing entry for the database with the new
  // version number, due to the UNIQUE ON CONFLICT REPLACE clause in the
  // CREATE statement (see Database::performOpenAndVerify()).
  String query(String("INSERT INTO ") + kInfoTableName +
               " (key, value) VALUES ('" + kVersionKey + "', ?);");

  database_authorizer_.Disable();

  bool result = SetTextValueInDatabase(sqlite_database_, query, version);
  if (result) {
    if (should_cache_version)
      SetCachedVersion(version);
  } else {
    DLOG(ERROR) << "Failed to set version " << version << " in database ("
                << query << ")";
  }

  database_authorizer_.Enable();

  return result;
}

void Database::SetExpectedVersion(const String& version) {
  expected_version_ = version.IsolatedCopy();
}

String Database::GetCachedVersion() const {
  auto& cache = GetDatabaseVersionCache();
  MutexLocker locker(cache.GetMutex());
  return cache.GetVersion(guid_);
}

void Database::SetCachedVersion(const String& actual_version) {
  auto& cache = GetDatabaseVersionCache();
  MutexLocker locker(cache.GetMutex());
  cache.SetVersion(guid_, actual_version);
}

bool Database::GetActualVersionForTransaction(String& actual_version) {
  DCHECK(sqlite_database_.TransactionInProgress());
  // Note: In multi-process browsers the cached value may be inaccurate. So we
  // retrieve the value from the database and update the cached value here.
  return GetVersionFromDatabase(actual_version, true);
}

void Database::DisableAuthorizer() {
  database_authorizer_.Disable();
}

void Database::EnableAuthorizer() {
  database_authorizer_.Enable();
}

void Database::SetAuthorizerPermissions(int permissions) {
  database_authorizer_.SetPermissions(permissions);
}

bool Database::LastActionChangedDatabase() {
  return database_authorizer_.LastActionChangedDatabase();
}

bool Database::LastActionWasInsert() {
  return database_authorizer_.LastActionWasInsert();
}

void Database::ResetDeletes() {
  database_authorizer_.ResetDeletes();
}

bool Database::HadDeletes() {
  return database_authorizer_.HadDeletes();
}

void Database::ResetAuthorizer() {
  database_authorizer_.Reset();
}

uint64_t Database::MaximumSize() const {
  return DatabaseTracker::Tracker().GetMaxSizeForDatabase(this);
}

void Database::IncrementalVacuumIfNeeded() {
  int64_t free_space_size = sqlite_database_.FreeSpaceSize();
  int64_t total_size = sqlite_database_.TotalSize();
  if (total_size <= 10 * free_space_size) {
    int result = sqlite_database_.RunIncrementalVacuumCommand();
    if (result != kSQLResultOk) {
      ReportSqliteError(result);
      LogErrorMessage(FormatErrorMessage("error vacuuming database", result,
                                         sqlite_database_.LastErrorMsg()));
    }
  }
}

void Database::ReportSqliteError(int sqlite_error_code) {
  WebDatabaseHost::GetInstance().ReportSqliteError(
      *GetSecurityOrigin(), StringIdentifier(), sqlite_error_code);
}

void Database::LogErrorMessage(const String& message) {
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kStorage,
                             mojom::ConsoleMessageLevel::kError, message));
}

ExecutionContext* Database::GetExecutionContext() const {
  return GetDatabaseContext()->GetExecutionContext();
}

void Database::CloseImmediately() {
  DCHECK(GetExecutionContext()->IsContextThread());
  if (GetDatabaseContext()->DatabaseThreadAvailable() && Opened()) {
    LogErrorMessage("forcibly closing database");
    GetDatabaseContext()->GetDatabaseThread()->ScheduleTask(
        std::make_unique<DatabaseCloseTask>(this, nullptr));
  }
}

void Database::changeVersion(const String& old_version,
                             const String& new_version,
                             V8SQLTransactionCallback* callback,
                             V8SQLTransactionErrorCallback* error_callback,
                             V8VoidCallback* success_callback) {
  ChangeVersionData data(old_version, new_version);
  RunTransaction(SQLTransaction::OnProcessV8Impl::Create(callback),
                 SQLTransaction::OnErrorV8Impl::Create(error_callback),
                 SQLTransaction::OnSuccessV8Impl::Create(success_callback),
                 false, &data);
}

void Database::transaction(V8SQLTransactionCallback* callback,
                           V8SQLTransactionErrorCallback* error_callback,
                           V8VoidCallback* success_callback) {
  RunTransaction(SQLTransaction::OnProcessV8Impl::Create(callback),
                 SQLTransaction::OnErrorV8Impl::Create(error_callback),
                 SQLTransaction::OnSuccessV8Impl::Create(success_callback),
                 false);
}

void Database::readTransaction(V8SQLTransactionCallback* callback,
                               V8SQLTransactionErrorCallback* error_callback,
                               V8VoidCallback* success_callback) {
  RunTransaction(SQLTransaction::OnProcessV8Impl::Create(callback),
                 SQLTransaction::OnErrorV8Impl::Create(error_callback),
                 SQLTransaction::OnSuccessV8Impl::Create(success_callback),
                 true);
}

void Database::PerformTransaction(
    SQLTransaction::OnProcessCallback* callback,
    SQLTransaction::OnErrorCallback* error_callback,
    SQLTransaction::OnSuccessCallback* success_callback) {
  RunTransaction(callback, error_callback, success_callback, false);
}

static void CallTransactionErrorCallback(
    SQLTransaction::OnErrorCallback* callback,
    std::unique_ptr<SQLErrorData> error_data) {
  callback->OnError(MakeGarbageCollected<SQLError>(*error_data));
}

void Database::RunTransaction(
    SQLTransaction::OnProcessCallback* callback,
    SQLTransaction::OnErrorCallback* error_callback,
    SQLTransaction::OnSuccessCallback* success_callback,
    bool read_only,
    const ChangeVersionData* change_version_data) {
  if (!GetExecutionContext())
    return;

  DCHECK(GetExecutionContext()->IsContextThread());
// FIXME: Rather than passing errorCallback to SQLTransaction and then
// sometimes firing it ourselves, this code should probably be pushed down
// into Database so that we only create the SQLTransaction if we're
// actually going to run it.
#if DCHECK_IS_ON()
  SQLTransaction::OnErrorCallback* original_error_callback = error_callback;
#endif
  SQLTransaction* transaction = SQLTransaction::Create(
      this, callback, success_callback, error_callback, read_only);
  SQLTransactionBackend* transaction_backend =
      RunTransaction(transaction, read_only, change_version_data);
  if (!transaction_backend) {
    SQLTransaction::OnErrorCallback* transaction_error_callback =
        transaction->ReleaseErrorCallback();
#if DCHECK_IS_ON()
    DCHECK_EQ(transaction_error_callback, original_error_callback);
#endif
    if (transaction_error_callback) {
      auto error = std::make_unique<SQLErrorData>(SQLError::kUnknownErr,
                                                  "database has been closed");
      GetDatabaseTaskRunner()->PostTask(
          FROM_HERE, WTF::Bind(&CallTransactionErrorCallback,
                               WrapPersistent(transaction_error_callback),
                               WTF::Passed(std::move(error))));
    }
  }
}

void Database::ScheduleTransactionCallback(SQLTransaction* transaction) {
  // The task is constructed in a database thread, and destructed in the
  // context thread.
  PostCrossThreadTask(
      *GetDatabaseTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&SQLTransaction::PerformPendingCallback,
                          WrapCrossThreadPersistent(transaction)));
}

Vector<String> Database::PerformGetTableNames() {
  DisableAuthorizer();

  SQLiteStatement statement(
      SqliteDatabase(), "SELECT name FROM sqlite_master WHERE type='table';");
  if (statement.Prepare() != kSQLResultOk) {
    DLOG(ERROR) << "Unable to retrieve list of tables for database "
                << DatabaseDebugName();
    EnableAuthorizer();
    return Vector<String>();
  }

  Vector<String> table_names;
  int result;
  while ((result = statement.Step()) == kSQLResultRow) {
    String name = statement.GetColumnText(0);
    if (name != DatabaseInfoTableName())
      table_names.push_back(name);
  }

  EnableAuthorizer();

  if (result != kSQLResultDone) {
    DLOG(ERROR) << "Error getting tables for database " << DatabaseDebugName();
    return Vector<String>();
  }

  return table_names;
}

Vector<String> Database::TableNames() {
  // FIXME: Not using isolatedCopy on these strings looks ok since threads
  // take strict turns in dealing with them. However, if the code changes,
  // this may not be true anymore.
  Vector<String> result;
  base::WaitableEvent event;
  if (!GetDatabaseContext()->DatabaseThreadAvailable())
    return result;

  auto task = std::make_unique<DatabaseTableNamesTask>(this, &event, result);
  GetDatabaseContext()->GetDatabaseThread()->ScheduleTask(std::move(task));
  event.Wait();

  return result;
}

const SecurityOrigin* Database::GetSecurityOrigin() const {
  if (!GetExecutionContext())
    return nullptr;
  if (GetExecutionContext()->IsContextThread())
    return context_thread_security_origin_.get();
  if (GetDatabaseContext()->GetDatabaseThread()->IsDatabaseThread())
    return database_thread_security_origin_.get();
  return nullptr;
}

base::SingleThreadTaskRunner* Database::GetDatabaseTaskRunner() const {
  return database_task_runner_.get();
}

}  // namespace blink
