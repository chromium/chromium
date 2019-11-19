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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_H_

#include <atomic>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_database_callback.h"
#include "third_party/blink/renderer/modules/webdatabase/database_authorizer.h"
#include "third_party/blink/renderer/modules/webdatabase/database_basic_types.h"
#include "third_party/blink/renderer/modules/webdatabase/database_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_backend.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_database.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ChangeVersionData;
class DatabaseAuthorizer;
class DatabaseContext;
class ExecutionContext;
class SQLTransactionClient;
class SQLTransactionCoordinator;

class Database final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Database(DatabaseContext*,
           const String& name,
           const String& expected_version,
           const String& display_name,
           uint32_t estimated_size);
  ~Database() override;
  void Trace(blink::Visitor*) override;

  bool OpenAndVerifyVersion(bool set_version_in_new_database,
                            DatabaseError&,
                            String& error_message,
                            V8DatabaseCallback* creation_callback);
  void Close();

  SQLTransactionBackend* RunTransaction(SQLTransaction*,
                                        bool read_only,
                                        const ChangeVersionData*);
  void ScheduleTransactionStep(SQLTransactionBackend*);
  void InProgressTransactionCompleted();

  SQLTransactionClient* TransactionClient() const;
  SQLTransactionCoordinator* TransactionCoordinator() const;

  // Direct support for the DOM API
  String version() const;
  void changeVersion(const String& old_version,
                     const String& new_version,
                     V8SQLTransactionCallback*,
                     V8SQLTransactionErrorCallback*,
                     V8VoidCallback* success_callback);
  void transaction(V8SQLTransactionCallback*,
                   V8SQLTransactionErrorCallback*,
                   V8VoidCallback* success_callback);
  void readTransaction(V8SQLTransactionCallback*,
                       V8SQLTransactionErrorCallback*,
                       V8VoidCallback* success_callback);

  void PerformTransaction(SQLTransaction::OnProcessCallback*,
                          SQLTransaction::OnErrorCallback*,
                          SQLTransaction::OnSuccessCallback*);

  bool Opened() { return opened_.load(std::memory_order_acquire); }
  bool IsNew() const { return new_; }

  const SecurityOrigin* GetSecurityOrigin() const;
  String StringIdentifier() const;
  String DisplayName() const;
  uint32_t EstimatedSize() const;
  String FileName() const;
  SQLiteDatabase& SqliteDatabase() { return sqlite_database_; }

  uint64_t MaximumSize() const;
  void IncrementalVacuumIfNeeded();

  void DisableAuthorizer();
  void EnableAuthorizer();
  void SetAuthorizerPermissions(int);
  bool LastActionChangedDatabase();
  bool LastActionWasInsert();
  void ResetDeletes();
  bool HadDeletes();
  void ResetAuthorizer();

  Vector<String> TableNames();
  void ScheduleTransactionCallback(SQLTransaction*);
  void CloseImmediately();
  void CloseDatabase();

  DatabaseContext* GetDatabaseContext() const {
    return database_context_.Get();
  }
  ExecutionContext* GetExecutionContext() const;
  base::SingleThreadTaskRunner* GetDatabaseTaskRunner() const;

 private:
  class DatabaseOpenTask;
  class DatabaseCloseTask;
  class DatabaseTransactionTask;
  class DatabaseTableNamesTask;

  bool PerformOpenAndVerify(bool set_version_in_new_database,
                            DatabaseError&,
                            String& error_message);
  void RunCreationCallback(V8DatabaseCallback* creation_callback,
                           std::unique_ptr<probe::AsyncTaskId> task_id);

  void ScheduleTransaction();

  bool GetVersionFromDatabase(String& version,
                              bool should_cache_version = true);
  bool SetVersionInDatabase(const String& version,
                            bool should_cache_version = true);
  void SetExpectedVersion(const String&);
  const String& ExpectedVersion() const { return expected_version_; }
  String GetCachedVersion() const;
  void SetCachedVersion(const String&);
  bool GetActualVersionForTransaction(String& version);

  void RunTransaction(SQLTransaction::OnProcessCallback*,
                      SQLTransaction::OnErrorCallback*,
                      SQLTransaction::OnSuccessCallback*,
                      bool read_only,
                      const ChangeVersionData* = nullptr);
  Vector<String> PerformGetTableNames();

  void ReportSqliteError(int sqlite_error_code);
  void LogErrorMessage(const String&);
  static const char* DatabaseInfoTableName();
  String DatabaseDebugName() const {
    return context_thread_security_origin_->ToString() + "::" + name_;
  }

  scoped_refptr<const SecurityOrigin> context_thread_security_origin_;
  scoped_refptr<const SecurityOrigin> database_thread_security_origin_;
  Member<DatabaseContext>
      database_context_;  // Associated with m_executionContext.
  // ExecutionContext::GetTaskRunner() is not thread-safe, so we save
  // SingleThreadTaskRunner for TaskType::DatabaseAccess for later use as the
  // constructor runs in the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> database_task_runner_;

  String name_;
  String expected_version_;
  String display_name_;
  uint32_t estimated_size_;
  String filename_;

  DatabaseGuid guid_;

  // Atomically written from the database thread only, but read from multiple
  // threads.
  std::atomic_bool opened_;

  bool new_;

  DatabaseAuthorizer database_authorizer_;
  SQLiteDatabase sqlite_database_;

  Deque<CrossThreadPersistent<SQLTransactionBackend>> transaction_queue_;
  Mutex transaction_in_progress_mutex_;
  bool transaction_in_progress_;
  bool is_transaction_queue_enabled_;

  friend class ChangeVersionWrapper;
  friend class DatabaseManager;
  friend class SQLStatementBackend;
  friend class SQLTransaction;
  friend class SQLTransactionBackend;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_H_
