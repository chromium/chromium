// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sqlite_persistent_store_backend_base.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"

namespace net {

SQLitePersistentStoreBackendBase::SQLitePersistentStoreBackendBase(
    const base::FilePath& path,
    std::string histogram_tag,
    const int current_version_number,
    const int compatible_version_number,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : path_(path),
      histogram_tag_(std::move(histogram_tag)),
      initialized_(false),
      corruption_detected_(false),
      current_version_number_(current_version_number),
      compatible_version_number_(compatible_version_number),
      background_task_runner_(std::move(background_task_runner)),
      client_task_runner_(std::move(client_task_runner)) {}

SQLitePersistentStoreBackendBase::~SQLitePersistentStoreBackendBase() {
  DCHECK(!db_.get()) << "Close should already have been called.";
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

  base::Time start = base::Time::Now();

  const base::FilePath dir = path_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    RecordPathDoesNotExistProblem();
    return false;
  }

  int64_t db_size = 0;
  if (base::GetFileSize(path_, &db_size))
    base::UmaHistogramCounts1M(histogram_tag_ + ".DBSizeInKB", db_size / 1024);

  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag(histogram_tag_);

  // base::Unretained is safe because |this| owns (and therefore outlives) the
  // sql::Database held by |db_|.
  db_->set_error_callback(base::BindRepeating(
      &SQLitePersistentStoreBackendBase::DatabaseErrorCallback,
      base::Unretained(this)));

  bool new_db = !base::PathExists(path_);

  if (!db_->Open(path_)) {
    DLOG(ERROR) << "Unable to open " << histogram_tag_ << " DB.";
    RecordOpenDBProblem();
    Reset();
    return false;
  }
  db_->Preload();

  if (!MigrateDatabaseSchema() || !CreateDatabaseSchema()) {
    DLOG(ERROR) << "Unable to update or initialize " << histogram_tag_
                << " DB tables.";
    RecordDBMigrationProblem();
    Reset();
    return false;
  }

  base::UmaHistogramCustomTimes(histogram_tag_ + ".TimeInitializeDB",
                                base::Time::Now() - start,
                                base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(1), 50);

  initialized_ = DoInitializeDatabase();

  if (!initialized_) {
    DLOG(ERROR) << "Unable to initialize " << histogram_tag_ << " DB.";
    RecordOpenDBProblem();
    Reset();
    return false;
  }

  if (new_db) {
    RecordNewDBFile();
  } else {
    RecordDBLoaded();
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
  base::Optional<int> cur_version = DoMigrateDatabaseSchema();
  if (!cur_version.has_value())
    return false;

  if (cur_version.value() < current_version_number_) {
    base::UmaHistogramCounts100(histogram_tag_ + ".CorruptMetaTable", 1);

    meta_table_.Reset();
    db_ = std::make_unique<sql::Database>();
    if (!sql::Database::Delete(path_) || !db()->Open(path_) ||
        !meta_table_.Init(db(), current_version_number_,
                          compatible_version_number_)) {
      base::UmaHistogramCounts100(
          histogram_tag_ + ".CorruptMetaTableRecoveryFailed", 1);
      NOTREACHED() << "Unable to reset the " << histogram_tag_ << " DB.";
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

  // Don't just do the close/delete here, as we are being called by |db| and
  // that seems dangerous.
  // TODO(shess): Consider just calling RazeAndClose() immediately.
  // db_ may not be safe to reset at this point, but RazeAndClose()
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
    bool success = db_->RazeAndClose();
    base::UmaHistogramBoolean(histogram_tag_ + ".KillDatabaseResult", success);
    meta_table_.Reset();
    db_.reset();
  }
}

}  // namespace net
