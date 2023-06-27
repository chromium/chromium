// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/webdata_export.h"

class WebDatabaseBackend;

namespace base {
class Location;
class SequencedTaskRunner;
}

class WDTypedResult;
class WebDataServiceConsumer;


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

  // WebDatabaseService lives on the UI sequence and posts tasks to the DB
  // sequence.  |path| points to the WebDatabase file.
  WebDatabaseService(const base::FilePath& path,
                     scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  WebDatabaseService(const WebDatabaseService&) = delete;
  WebDatabaseService& operator=(const WebDatabaseService&) = delete;

  // Adds |table| as a WebDatabaseTable that will participate in
  // managing the database, transferring ownership. All calls to this
  // method must be made before |LoadDatabase| is called.
  virtual void AddTable(std::unique_ptr<WebDatabaseTable> table);

  // Initializes the web database service.
  virtual void LoadDatabase();

  // Unloads the database and shuts down the service.
  virtual void ShutdownDatabase();

  // Gets a pointer to the WebDatabase (owned by WebDatabaseService).
  // TODO(caitkp): remove this method once SyncServices no longer depend on it.
  virtual WebDatabase* GetDatabaseOnDB() const;

  // Returns a pointer to the WebDatabaseBackend.
  scoped_refptr<WebDatabaseBackend> GetBackend() const;

  // Schedule an update/write task on the DB sequence.
  virtual void ScheduleDBTask(const base::Location& from_here, WriteTask task);

  // Schedule a read task on the DB sequence.
  // Retrieves a WeakPtr to the |consumer| so that |consumer| does not have to
  // outlive the WebDatabaseService.
  virtual WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const base::Location& from_here,
      ReadTask task,
      WebDataServiceConsumer* consumer);

  // Cancel an existing request for a task on the DB sequence.
  // TODO(caitkp): Think about moving the definition of the Handle type to
  // somewhere else.
  virtual void CancelRequest(WebDataServiceBase::Handle h);

  // Register a callback to be notified that the database has failed to load.
  // Multiple callbacks may be registered, and each will be called at most once
  // (following a database load failure), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  void RegisterDBErrorCallback(DBLoadErrorCallback callback);

 private:
  class BackendDelegate;
  friend class BackendDelegate;
  friend class base::RefCountedDeleteOnSequence<WebDatabaseService>;
  friend class base::DeleteHelper<WebDatabaseService>;

  using ErrorCallbacks = std::vector<DBLoadErrorCallback>;

  virtual ~WebDatabaseService();

  void OnDatabaseLoadDone(sql::InitStatus status,
                          const std::string& diagnostics);

  base::FilePath path_;

  // The primary owner is |WebDatabaseService| but is refcounted because
  // PostTask on DB sequence may outlive us.
  scoped_refptr<WebDatabaseBackend> web_db_backend_;

  // Callbacks to be called if the DB has failed to load.
  ErrorCallbacks error_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // All vended weak pointers are invalidated in ShutdownDatabase().
  base::WeakPtrFactory<WebDatabaseService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
