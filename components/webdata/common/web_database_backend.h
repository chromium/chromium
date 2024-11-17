// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_BACKEND_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_BACKEND_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_export.h"

class WebDatabase;
class WebDatabaseTable;
class WebDataRequest;
class WebDataRequestManager;
class WebDataServiceBase;
class WebDatabaseService;

namespace base {
class SequencedTaskRunner;
}

// `WebDatabaseBackend` handles all database tasks posted by
// `WebDatabaseService`. It is refcounted to allow asynchronous destruction on
// the DB thread.

class WEBDATA_EXPORT WebDatabaseBackend
    : public base::RefCountedDeleteOnSequence<WebDatabaseBackend> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when the backend has finished loading the db.
    // `status` is the result of initializing the db.
    // `diagnostics` contains diagnostic information about the db, and it will
    // only be populated when an error occurs.
    virtual void DBLoaded(sql::InitStatus status,
                          const std::string& diagnostics) = 0;
  };

  WebDatabaseBackend(const base::FilePath& path,
                     std::unique_ptr<Delegate> delegate,
                     const scoped_refptr<base::SequencedTaskRunner>& db_thread);

  WebDatabaseBackend(const WebDatabaseBackend&) = delete;
  WebDatabaseBackend& operator=(const WebDatabaseBackend&) = delete;

  // Obtains the database. Call on the DB sequence. If you must run tasks on
  // this database, make sure you run them on the sequenced task runner returned
  // from a call to `GetDbSequence` on the owning `WebDatabaseService`.
  WebDatabase* database() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_.get();
  }

 protected:
  friend class base::RefCountedDeleteOnSequence<WebDatabaseBackend>;
  friend class base::DeleteHelper<WebDatabaseBackend>;

  virtual ~WebDatabaseBackend();

 private:
  friend class WebDatabaseService;
  friend class WebDataServiceBase;

  // Must call only before `InitDatabase`. `AddTable` is called on the client
  // sequence.
  void AddTable(std::unique_ptr<WebDatabaseTable> table);

  // Initializes the database.
  void InitDatabase();

  // Shuts down the database.
  void ShutdownDatabase();

  // Task wrappers to update requests and and notify `request_manager_`. These
  // are used in cases where the request is being made from the UI thread and an
  // asynchronous callback is required to notify the client of `request`'s
  // completion.
  void DBWriteTaskWrapper(WebDatabaseService::WriteTask task,
                          std::unique_ptr<WebDataRequest> request);
  void DBReadTaskWrapper(WebDatabaseService::ReadTask task,
                         std::unique_ptr<WebDataRequest> request);

  // Called on UI sequence to initialize the encryptors in the tables.
  void MaybeInitEncryptorOnUiSequence(os_crypt_async::Encryptor encryptor);

  // Task runners to run database tasks.
  void ExecuteWriteTask(WebDatabaseService::WriteTask task);
  std::unique_ptr<WDTypedResult> ExecuteReadTask(
      WebDatabaseService::ReadTask task);

  const scoped_refptr<WebDataRequestManager>& request_manager() {
    return request_manager_;
  }

  // Opens the database file from the profile path if an init has not yet been
  // attempted. Separated from the constructor to ease construction/destruction
  // of this object on one thread but database access on the DB thread.
  void LoadDatabaseIfNecessary();

  // Invoked on a db error.
  void DatabaseErrorCallback(int error, sql::Statement* statement);

  // Commit the current transaction.
  void Commit();

  // Path to database file.
  base::FilePath db_path_;

  // This Encryptor is held on the db sequence and passed to each table during
  // initialization. Must outlive `db_`.
  std::optional<const os_crypt_async::Encryptor> encryptor_;

  // The tables that participate in managing the database. These are
  // owned here but other than that this class does nothing with
  // them. Their initialization is in whatever factory creates
  // `WebDatabaseService`, and lookup by type is provided by the
  // `WebDatabase` class. The tables need to be owned by this refcounted
  // object, or they themselves would need to be refcounted. Owning
  // them here rather than having `WebDatabase` own them makes for
  // easier unit testing of `WebDatabase`.
  std::vector<std::unique_ptr<WebDatabaseTable>> tables_;

  std::unique_ptr<WebDatabase> db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of all pending requests made to the db.
  scoped_refptr<WebDataRequestManager> request_manager_ =
      base::MakeRefCounted<WebDataRequestManager>();

  // State of database initialization. Used to prevent the executing of tasks
  // before the db is ready.
  sql::InitStatus init_status_ = sql::INIT_FAILURE;

  // True if an attempt has been made to load the database (even if the attempt
  // fails), used to avoid continually trying to reinit if the db init fails.
  bool init_complete_ = false;

  // True if a catastrophic database error occurs and further error callbacks
  // from the database should be ignored.
  bool catastrophic_error_occurred_ = false;

  // If a catastrophic database error has occurred, this contains any available
  // diagnostic information.
  std::string diagnostics_;

  // Delegate. See the class definition above for more information.
  std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_BACKEND_H_
