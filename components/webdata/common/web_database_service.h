// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/webdata_export.h"

class WebDatabaseBackend;

namespace base {
class Location;
class SequencedTaskRunner;
}

namespace os_crypt_async {
class OSCryptAsync;
}

class WDTypedResult;
class WebDataServiceConsumer;

namespace features {
WEBDATA_EXPORT BASE_DECLARE_FEATURE(kUseNewEncryptionKeyForWebData);
}  // namespace features

////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseService defines the interface to a generic data repository
// responsible for controlling access to the web database (metadata associated
// with web pages).
//
////////////////////////////////////////////////////////////////////////////////

class WEBDATA_EXPORT WebDatabaseService
    : public base::RefCountedDeleteOnSequence<WebDatabaseService> {
 public:
  using ReadTask =
      base::OnceCallback<std::unique_ptr<WDTypedResult>(WebDatabase*)>;
  using WriteTask = base::OnceCallback<WebDatabase::State(WebDatabase*)>;

  // Types for managing DB loading callbacks.
  using DBLoadErrorCallback =
      base::OnceCallback<void(sql::InitStatus, const std::string&)>;

  // `WebDatabaseService` lives on the UI sequence and posts tasks to the DB
  // sequence.  `path` points to the WebDatabase file. Do not run any database
  // tasks on DB sequence after passing to this constructor. Instead, call
  // `GetDbSequence` to obtain a valid sequenced task runner that ensures that
  // tasks run in the correct order i.e. after any internal initialization has
  // taken place.
  WebDatabaseService(const base::FilePath& path,
                     scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  WebDatabaseService(const WebDatabaseService&) = delete;
  WebDatabaseService& operator=(const WebDatabaseService&) = delete;

  // Adds |table| as a `WebDatabaseTable` that will participate in
  // managing the database, transferring ownership. All calls to this
  // method must be made before `LoadDatabase` is called.
  void AddTable(std::unique_ptr<WebDatabaseTable> table);

  // Initializes the web database service.
  void LoadDatabase(os_crypt_async::OSCryptAsync* os_crypt);

  // Unloads the database and shuts down the service.
  void ShutdownDatabase();

  // Gets a pointer to the `WebDatabase` (owned by `WebDatabaseService`).
  // TODO(caitkp): remove this method once SyncServices no longer depend on it.
  WebDatabase* GetDatabaseOnDB() const;

  // Returns a pointer to the `WebDatabaseBackend`.
  scoped_refptr<WebDatabaseBackend> GetBackend() const;

  // Obtain the sequence to execute any database tasks on. This should be called
  // rather than using the `db_task_runner` passed into the constructor, because
  // it might differ from the original `db_task_runner` passed into this class.
  // Prefer simply calling one of the Schedule* methods to schedule database
  // tasks to the DB sequence.
  scoped_refptr<base::SequencedTaskRunner> GetDbSequence();

  // Schedule an update/write task on the DB sequence.
  void ScheduleDBTask(const base::Location& from_here, WriteTask task);

  // Schedule a read task on the DB sequence.
  // Retrieves a WeakPtr to the |consumer| so that |consumer| does not have to
  // outlive the `WebDatabaseService`.
  WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const base::Location& from_here,
      ReadTask task,
      WebDataServiceConsumer* consumer);

  // Cancel an existing request for a task on the DB sequence.
  // TODO(caitkp): Think about moving the definition of the Handle type to
  // somewhere else.
  void CancelRequest(WebDataServiceBase::Handle h);

  // Register a callback to be notified that the database has failed to load.
  // Multiple callbacks may be registered, and each will be called at most once
  // (following a database load failure), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  void RegisterDBErrorCallback(DBLoadErrorCallback callback);

  // Test-only API to verify if the database is stored in-memory only, as
  // opposed to on-disk storage.
  bool UsesInMemoryDatabaseForTest() const;

 private:
  class BackendDelegate;
  friend class BackendDelegate;
  friend class base::RefCountedDeleteOnSequence<WebDatabaseService>;
  friend class base::DeleteHelper<WebDatabaseService>;

  using ErrorCallbacks = std::vector<DBLoadErrorCallback>;

  ~WebDatabaseService();

  void OnDatabaseLoadDone(sql::InitStatus status,
                          const std::string& diagnostics);

  void CompleteLoadDatabase(os_crypt_async::Encryptor encryptor, bool success);

  base::FilePath path_;

  base::CallbackListSubscription subscription_;

  // The primary owner is |WebDatabaseService| but is refcounted because
  // PostTask on DB sequence may outlive us.
  scoped_refptr<WebDatabaseBackend> web_db_backend_;

  // Callbacks to be called if the DB has failed to load.
  ErrorCallbacks error_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Deferred task runner on which any tasks externally posted are queued until
  // the initialization callback has been run.
  scoped_refptr<base::DeferredSequencedTaskRunner> pending_task_queue_;

  // All vended weak pointers are invalidated in ShutdownDatabase().
  base::WeakPtrFactory<WebDatabaseService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
