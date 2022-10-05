/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_BACKEND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_BACKEND_H_

#include <memory>
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/webdatabase/database_basic_types.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement_backend.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_state_machine.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Database;
class SQLErrorData;
class SQLiteTransaction;
class SQLTransaction;
class SQLTransactionBackend;
class SQLValue;

class SQLTransactionWrapper : public GarbageCollected<SQLTransactionWrapper> {
 public:
  virtual ~SQLTransactionWrapper() = default;
  virtual void Trace(Visitor* visitor) const {}
  virtual bool PerformPreflight(SQLTransactionBackend*) = 0;
  virtual bool PerformPostflight(SQLTransactionBackend*) = 0;
  virtual SQLErrorData* SqlError() const = 0;
  virtual void HandleCommitFailedAfterPostflight(SQLTransactionBackend*) = 0;
};

class SQLTransactionBackend final
    : public GarbageCollected<SQLTransactionBackend>,
      public SQLTransactionStateMachine<SQLTransactionBackend> {
 public:
  SQLTransactionBackend(Database*,
                        SQLTransaction*,
                        SQLTransactionWrapper*,
                        bool read_only);
  ~SQLTransactionBackend() override;
  void Trace(Visitor*) const;

  void LockAcquired();
  void PerformNextStep();

  Database* GetDatabase() { return database_.Get(); }
  bool IsReadOnly() { return read_only_; }
  void NotifyDatabaseThreadIsShuttingDown();

  // APIs called from the frontend published:
  void RequestTransitToState(SQLTransactionState);
  SQLErrorData* TransactionError();
  SQLStatement* CurrentStatement();
  void SetShouldRetryCurrentStatement(bool);
  void ExecuteSQL(SQLStatement*,
                  const String& statement,
                  const Vector<SQLValue>& arguments,
                  int permissions);

 private:
  void DoCleanup();

  void EnqueueStatementBackend(SQLStatementBackend*);

  // State Machine functions:
  StateFunction StateFunctionFor(SQLTransactionState) override;
  void ComputeNextStateAndCleanupIfNeeded();

  // State functions:
  SQLTransactionState AcquireLock();
  SQLTransactionState OpenTransactionAndPreflight();
  SQLTransactionState RunStatements();
  SQLTransactionState PostflightAndCommit();
  SQLTransactionState CleanupAndTerminate();
  SQLTransactionState CleanupAfterTransactionErrorCallback();

  SQLTransactionState UnreachableState();
  SQLTransactionState SendToFrontendState();

  SQLTransactionState NextStateForCurrentStatementError();
  SQLTransactionState NextStateForTransactionError();
  SQLTransactionState RunCurrentStatementAndGetNextState();

  void GetNextStatement();

  CrossThreadPersistent<SQLTransaction> frontend_;
  CrossThreadPersistent<SQLStatementBackend> current_statement_backend_;

  CrossThreadPersistent<Database> database_;
  Member<SQLTransactionWrapper> wrapper_;
  std::unique_ptr<SQLErrorData> transaction_error_;

  bool has_callback_;
  bool has_success_callback_;
  bool has_error_callback_;
  bool should_retry_current_statement_;
  bool modified_database_;
  bool lock_acquired_;
  bool read_only_;
  bool has_version_mismatch_;

  base::Lock statement_lock_;
  Deque<CrossThreadPersistent<SQLStatementBackend>> statement_queue_
      GUARDED_BY(statement_lock_);

  std::unique_ptr<SQLiteTransaction> sqlite_transaction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_BACKEND_H_
