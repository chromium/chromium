// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sqlite_persistent_store_backend_base.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

namespace net {

SQLitePersistentStoreBackendBase::SQLitePersistentStoreBackendBase(
    const base::FilePath& path,
    const std::string& histogram_tag,
    const int current_version_number,
    const int compatible_version_number,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    bool enable_exclusive_access)
    : path_(path),
      histogram_tag_(histogram_tag),
      current_version_number_(current_version_number),
      compatible_version_number_(compatible_version_number),
      background_task_runner_(std::move(background_task_runner)),
      client_task_runner_(std::move(client_task_runner)),
      enable_exclusive_access_(enable_exclusive_access) {}

SQLitePersistentStoreBackendBase::~SQLitePersistentStoreBackendBase() {
  // If `db_` hasn't been reset by the time this destructor is called,
  // a use-after-free could occur if the `db_` error callback is ever
  // invoked. To guard against this, crash if `db_` hasn't been reset
  // so that this use-after-free doesn't happen and so that we'll be
  // alerted to the fact that a closer look at this code is needed.
  CHECK(!db_.get()) << "Close should already have been called.";
}

void SQLitePersistentStoreBackendBase::Flush(base::OnceClosure callback) {
  DCHECK(!background_task_runner_->RunsTasksInCurrentSequence());
  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(
          &SQLitePersistentStoreBackendBase::FlushAndNotifyInBackground, this,
          std::move(callback)));
}

void SQLitePersistentStoreBackendBase::Close() {
  if (background_task_runner_->RunsTasksInCurrentSequence()) {
    DoCloseInBackground();
  } else {
    // Must close the backend on the background runner.
    PostBackgroundTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentStoreBackendBase::DoCloseInBackground,
                       this));
  }
}

void SQLitePersistentStoreBackendBase::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {
  base::AutoLock locked(before_commit_callback_lock_);
  before_commit_callback_ = std::move(callback);
}

bool SQLitePersistentStoreBackendBase::InitializeDatabase() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (initialized_ || corruption_detected_) {
    // Return false if we were previously initialized but the DB has since been
    // closed, or if corruption caused a database reset during initialization.
    return db_ != nullptr;
  }

  base::ElapsedTimer timer;

  const base::FilePath dir = path_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    return false;
  }

  // TODO(crbug.com/40262972): Remove explicit_locking = false. This currently
  // needs to be set to false because of several failing MigrationTests.
  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = false,
      .exclusive_database_file_lock = enable_exclusive_access_});

  db_->set_histogram_tag(histogram_tag_);

  // base::Unretained is safe because |this| owns (and therefore outlives) the
  // sql::Database held by |db_|.
  db_->set_error_callback(base::BindRepeating(
      &SQLitePersistentStoreBackendBase::DatabaseErrorCallback,
      base::Unretained(this)));

  bool has_been_preloaded = false;
  // It is not possible to preload a database opened with exclusive access,
  // because the file cannot be opened again to preload it. In this case,
  // preload before opening the database.
  if (enable_exclusive_access_) {
    has_been_preloaded = true;

    // Can only attempt to preload before Open if the file exists.
    if (base::PathExists(path_)) {
      // See comments in Database::Preload for explanation of these values.
      constexpr int kPreReadSize = 128 * 1024 * 1024;  // 128 MB
      // TODO(crbug.com/40904059): Consider moving preload behind a database
      // option.
      base::PreReadFile(path_, /*is_executable=*/false, /*sequential=*/false,
                        kPreReadSize);
    }
  }

  if (!db_->Open(path_)) {
    DLOG(ERROR) << "Unable to open " << histogram_tag_ << " DB.";
    RecordOpenDBProblem();
    Reset();
    return false;
  }

  // Only attempt a preload if the database hasn't already been preloaded above.
  if (!has_been_preloaded) {
    db_->Preload();
  }

  if (!MigrateDatabaseSchema() || !CreateDatabaseSchema()) {
    DLOG(ERROR) << "Unable to update or initialize " << histogram_tag_
                << " DB tables.";
    RecordDBMigrationProblem();
    Reset();
    return false;
  }

  base::UmaHistogramCustomTimes(histogram_tag_ + ".TimeInitializeDB",
                                timer.Elapsed(), base::Milliseconds(1),
                                base::Minutes(1), 50);

  initialized_ = DoInitializeDatabase();

  if (!initialized_) {
    DLOG(ERROR) << "Unable to initialize " << histogram_tag_ << " DB.";
    RecordOpenDBProblem();
    Reset();
    return false;
  }

  return true;
}

bool SQLitePersistentStoreBackendBase::DoInitializeDatabase() {
  return true;
}

void SQLitePersistentStoreBackendBase::Reset() {
  if (db_ && db_->is_open())
    db_->Raze();
  meta_table_.Reset();
  db_.reset();
}

void SQLitePersistentStoreBackendBase::Commit() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  {
    base::AutoLock locked(before_commit_callback_lock_);
    if (!before_commit_callback_.is_null())
      before_commit_callback_.Run();
  }

  DoCommit();
}

void SQLitePersistentStoreBackendBase::PostBackgroundTask(
    const base::Location& origin,
    base::OnceClosure task) {
  if (!background_task_runner_->PostTask(origin, std::move(task))) {
    LOG(WARNING) << "Failed to post task from " << origin.ToString()
                 << " to background_task_runner_.";
  }
}

void SQLitePersistentStoreBackendBase::PostClientTask(
    const base::Location& origin,
    base::OnceClosure task) {
  if (!client_task_runner_->PostTask(origin, std::move(task))) {
    LOG(WARNING) << "Failed to post task from " << origin.ToString()
                 << " to client_task_runner_.";
  }
}

bool SQLitePersistentStoreBackendBase::MigrateDatabaseSchema() {
  // Version check.
  if (!meta_table_.Init(db_.get(), current_version_number_,
                        compatible_version_number_)) {
    return false;
  }

  if (meta_table_.GetCompatibleVersionNumber() > current_version_number_) {
    LOG(WARNING) << histogram_tag_ << " database is too new.";
    return false;
  }

  // |cur_version| is the version that the database ends up at, after all the
  // database upgrade statements.
  std::optional<int> cur_version = DoMigrateDatabaseSchema();
  if (!cur_version.has_value())
    return false;

  // Metatable is corrupted. Try to recover.
  if (cur_version.value() < current_version_number_) {
    meta_table_.Reset();
    db_ = std::make_unique<sql::Database>();
    bool recovered = sql::Database::Delete(path_) && db()->Open(path_) &&
                     meta_table_.Init(db(), current_version_number_,
                                      compatible_version_number_);
    base::UmaHistogramBoolean(histogram_tag_ + ".CorruptMetaTableRecovered",
                              recovered);
    if (!recovered) {
      DUMP_WILL_BE_NOTREACHED()
          << "Unable to reset the " << histogram_tag_ << " DB.";
      meta_table_.Reset();
      db_.reset();
      return false;
    }
  }

  return true;
}

void SQLitePersistentStoreBackendBase::FlushAndNotifyInBackground(
    base::OnceClosure callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  Commit();
  if (callback)
    PostClientTask(FROM_HERE, std::move(callback));
}

void SQLitePersistentStoreBackendBase::DoCloseInBackground() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  // Commit any pending operations
  Commit();

  meta_table_.Reset();
  db_.reset();
}

void SQLitePersistentStoreBackendBase::DatabaseErrorCallback(
    int error,
    sql::Statement* stmt) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!sql::IsErrorCatastrophic(error))
    return;

  // TODO(shess): Running KillDatabase() multiple times should be
  // safe.
  if (corruption_detected_)
    return;

  corruption_detected_ = true;

  if (!initialized_) {
    sql::UmaHistogramSqliteResult(histogram_tag_ + ".ErrorInitializeDB", error);

#if BUILDFLAG(IS_WIN)
    base::UmaHistogramSparse(histogram_tag_ + ".WinGetLastErrorInitializeDB",
                             ::GetLastError());
#endif  // BUILDFLAG(IS_WIN)
  }

  // Don't just do the close/delete here, as we are being called by |db| and
  // that seems dangerous.
  // TODO(shess): Consider just calling RazeAndPoison() immediately.
  // db_ may not be safe to reset at this point, but RazeAndPoison()
  // would cause the stack to unwind safely with errors.
  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentStoreBackendBase::KillDatabase, this));
}

void SQLitePersistentStoreBackendBase::KillDatabase() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (db_) {
    // This Backend will now be in-memory only. In a future run we will recreate
    // the database. Hopefully things go better then!
    db_->RazeAndPoison();
    meta_table_.Reset();
    db_.reset();
  }
}

}  // namespace net
