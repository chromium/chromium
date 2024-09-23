// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_STORE_BACKEND_BASE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_STORE_BACKEND_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/thread_annotations.h"
#include "sql/meta_table.h"

namespace base {
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace net {

// This class handles the initialization and closing of a SQLite database. It
// is designed to be shared between a client thread and a background task
// runner.
//
// Subclasses will want to have:
// - methods to load the data from the database, which should call
//   InitializeDatabase() from the background thread to ensure the database has
//   been initialized before attempting to load data,
// - overridden DoMigrateDatabaseSchema() and CreateDatabaseSchema(),
//   which will be called in the course of initializing the database,
// - optionally overridden DoInitializeDatabase() which performs any other
//   initialization tasks,
// - a way to keep track of pending operations in order to commit them
//   by invoking Commit() on the background thread, e.g. when a certain batch
//   size is reached or a certain amount of time has passed,
// - overridden DoCommit() to actually handle the logic of committing
//   pending operations to the database,
// - optionally overridden Record*() to record the appropriate metrics.
class SQLitePersistentStoreBackendBase
    : public base::RefCountedThreadSafe<SQLitePersistentStoreBackendBase> {
 public:
  SQLitePersistentStoreBackendBase(const SQLitePersistentStoreBackendBase&) =
      delete;
  SQLitePersistentStoreBackendBase& operator=(
      const SQLitePersistentStoreBackendBase&) = delete;

  // Posts a task to flush pending operations to the database in the background.
  // |callback| is run in the foreground when it is done.
  void Flush(base::OnceClosure callback);

  // Commit any pending operations and close the database. This must be called
  // before the object is destroyed.
  void Close();

  // Set the callback that will be run at the beginning of Commit.
  void SetBeforeCommitCallback(base::RepeatingClosure callback);

 protected:
  friend class base::RefCountedThreadSafe<SQLitePersistentStoreBackendBase>;

  // |current_version_number| and |compatible_version_number| must be greater
  // than 0, as per //sql/meta_table.h. |background_task_runner| should be
  // non-null. If |enable_exclusive_access| is true then the sqlite3 database
  // will be opened with exclusive flag.
  SQLitePersistentStoreBackendBase(
      const base::FilePath& path,
      const std::string& histogram_tag,
      const int current_version_number,
      const int compatible_version_number,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      bool enable_exclusive_access);

  virtual ~SQLitePersistentStoreBackendBase();

  // Initialize the database. Should be called on background thread. Call this
  // from a subclass' Load method(s) to ensure the database is initialized
  // before loading data from it.
  bool InitializeDatabase();

  // Record metrics on various errors/events that may occur during
  // initialization.
  virtual void RecordOpenDBProblem() {}
  virtual void RecordDBMigrationProblem() {}

  // Embedder-specific database upgrade statements. Returns the version number
  // that the database ends up at, or returns nullopt on error. This is called
  // during MigrateDatabaseSchema() which is called during InitializeDatabase(),
  // and returning |std::nullopt| will cause the initialization process to fail
  // and stop.
  virtual std::optional<int> DoMigrateDatabaseSchema() = 0;

  // Initializes the desired table(s) of the database, e.g. by creating them or
  // checking that they already exist. Returns whether the tables exist.
  // |db()| should not be null when this is called. This is called during
  // InitializeDatabase(), and returning false will cause the initialization
  // process to fail and stop.
  virtual bool CreateDatabaseSchema() = 0;

  // Embedder-specific database initialization tasks. Returns whether they were
  // successful. |db()| should not be null when this is called.
  // This is called during InitializeDatabase(), and returning false will cause
  // the initialization process to fail and stop. The default implementation
  // just returns true.
  virtual bool DoInitializeDatabase();

  // Raze and reset the metatable and database, e.g. if errors are encountered
  // in initialization.
  void Reset();

  // Commit pending operations to the database. First runs
  // |before_commit_callback_|. Should be called on the background thread.
  void Commit();

  // Embedder-specific logic to commit pending operations. (This base class has
  // no notion of pending operations or what to do with them.)
  virtual void DoCommit() = 0;

  // Post a task to the background task runner.
  void PostBackgroundTask(const base::Location& origin, base::OnceClosure task);

  // Post a task to the client task runner.
  void PostClientTask(const base::Location& origin, base::OnceClosure task);

  sql::Database* db() { return db_.get(); }
  sql::MetaTable* meta_table() { return &meta_table_; }

  base::SequencedTaskRunner* background_task_runner() {
    return background_task_runner_.get();
  }
  base::SequencedTaskRunner* client_task_runner() {
    return client_task_runner_.get();
  }

 private:
  // Ensures that the database is at the current version, upgrading if
  // necessary. Returns whether it was successful.
  bool MigrateDatabaseSchema();

  // Flushes (commits pending operations) on the background runner, and invokes
  // |callback| on the client thread when done.
  void FlushAndNotifyInBackground(base::OnceClosure callback);

  // Close the database on the background runner.
  void DoCloseInBackground();

  // Error-handling callback. On errors, the error number (and statement, if
  // available) will be passed to the callback.
  // Sets |corruption_detected_| and posts a task to the background runner to
  // kill the database.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Kills the database in the case of a catastropic error.
  void KillDatabase();

  // The file path where the database is stored.
  const base::FilePath path_;

  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;

  // The identifying prefix for metrics.
  const std::string histogram_tag_;

  // Whether the database has been initialized.
  bool initialized_ = false;

  // Whether the KillDatabase callback has been scheduled.
  bool corruption_detected_ = false;

  // Current version number of the database. Must be greater than 0.
  const int current_version_number_;

  // The lowest version of the code that the database can be read by. Must be
  // greater than 0.
  const int compatible_version_number_;

  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  // If true, then sqlite will be requested to open the file with exclusive
  // access.
  const bool enable_exclusive_access_;

  // Callback to be run before Commit.
  base::RepeatingClosure before_commit_callback_
      GUARDED_BY(before_commit_callback_lock_);
  // Guards |before_commit_callback_|.
  base::Lock before_commit_callback_lock_;
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_STORE_BACKEND_BASE_H_
