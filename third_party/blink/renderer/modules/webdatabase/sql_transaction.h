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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_H_

#include <memory>
#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_void_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_transaction_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_transaction_error_callback.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_state_machine.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Database;
class ExceptionState;
class SQLErrorData;
class SQLTransactionBackend;
class SQLValue;
class ScriptValue;

class SQLTransaction final : public ScriptWrappable,
                             public SQLTransactionStateMachine<SQLTransaction> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class OnProcessCallback : public GarbageCollected<OnProcessCallback> {
   public:
    virtual ~OnProcessCallback() = default;
    virtual void Trace(Visitor*) const {}
    virtual bool OnProcess(SQLTransaction*) = 0;

   protected:
    OnProcessCallback() = default;
  };

  class OnProcessV8Impl : public OnProcessCallback {
   public:
    static OnProcessV8Impl* Create(V8SQLTransactionCallback* callback) {
      return callback ? MakeGarbageCollected<OnProcessV8Impl>(callback)
                      : nullptr;
    }

    explicit OnProcessV8Impl(V8SQLTransactionCallback* callback)
        : callback_(callback) {}

    void Trace(Visitor*) const override;
    bool OnProcess(SQLTransaction*) override;

   private:
    Member<V8SQLTransactionCallback> callback_;
  };

  class OnSuccessCallback : public GarbageCollected<OnSuccessCallback> {
   public:
    virtual ~OnSuccessCallback() = default;
    virtual void Trace(Visitor*) const {}
    virtual void OnSuccess() = 0;

   protected:
    OnSuccessCallback() = default;
  };

  class OnSuccessV8Impl : public OnSuccessCallback {
   public:
    static OnSuccessV8Impl* Create(V8VoidCallback* callback) {
      return callback ? MakeGarbageCollected<OnSuccessV8Impl>(callback)
                      : nullptr;
    }

    explicit OnSuccessV8Impl(V8VoidCallback* callback) : callback_(callback) {}

    void Trace(Visitor*) const override;
    void OnSuccess() override;

   private:
    Member<V8VoidCallback> callback_;
  };

  class OnErrorCallback : public GarbageCollected<OnErrorCallback> {
   public:
    virtual ~OnErrorCallback() = default;
    virtual void Trace(Visitor*) const {}
    virtual bool OnError(SQLError*) = 0;

   protected:
    OnErrorCallback() = default;
  };

  class OnErrorV8Impl : public OnErrorCallback {
   public:
    static OnErrorV8Impl* Create(V8SQLTransactionErrorCallback* callback) {
      return callback ? MakeGarbageCollected<OnErrorV8Impl>(callback) : nullptr;
    }

    explicit OnErrorV8Impl(V8SQLTransactionErrorCallback* callback)
        : callback_(callback) {}

    void Trace(Visitor*) const override;
    bool OnError(SQLError*) override;

   private:
    Member<V8SQLTransactionErrorCallback> callback_;
  };

  static SQLTransaction* Create(Database*,
                                OnProcessCallback*,
                                OnSuccessCallback*,
                                OnErrorCallback*,
                                bool read_only);

  SQLTransaction(Database*,
                 OnProcessCallback*,
                 OnSuccessCallback*,
                 OnErrorCallback*,
                 bool read_only);
  ~SQLTransaction() override;
  void Trace(Visitor*) const override;

  void PerformPendingCallback();

  void ExecuteSQL(const String& sql_statement,
                  const Vector<SQLValue>& arguments,
                  SQLStatement::OnSuccessCallback*,
                  SQLStatement::OnErrorCallback*,
                  ExceptionState&);
  void executeSql(ScriptState*, const String& sql_statement, ExceptionState&);
  void executeSql(ScriptState*,
                  const String& sql_statement,
                  const std::optional<HeapVector<ScriptValue>>& arguments,
                  V8SQLStatementCallback*,
                  V8SQLStatementErrorCallback*,
                  ExceptionState&);

  Database* GetDatabase() { return database_.Get(); }

  OnErrorCallback* ReleaseErrorCallback();

  // APIs called from the backend published:
  void RequestTransitToState(SQLTransactionState);
  bool HasCallback() const;
  bool HasSuccessCallback() const;
  bool HasErrorCallback() const;
  void SetBackend(SQLTransactionBackend*);

 private:
  void ClearCallbacks();

  // State Machine functions:
  StateFunction StateFunctionFor(SQLTransactionState) override;
  bool ComputeNextStateAndCleanupIfNeeded();

  // State functions:
  SQLTransactionState DeliverTransactionCallback();
  SQLTransactionState DeliverTransactionErrorCallback();
  SQLTransactionState DeliverStatementCallback();
  SQLTransactionState DeliverQuotaIncreaseCallback();
  SQLTransactionState DeliverSuccessCallback();

  SQLTransactionState UnreachableState();
  SQLTransactionState SendToBackendState();

  SQLTransactionState NextStateForTransactionError();

  Member<Database> database_;
  Member<SQLTransactionBackend> backend_;
  Member<OnProcessCallback> callback_;
  Member<OnSuccessCallback> success_callback_;
  Member<OnErrorCallback> error_callback_;

  bool execute_sql_allowed_;
  std::unique_ptr<SQLErrorData> transaction_error_;
  probe::AsyncTaskContext async_task_context_;

  bool read_only_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_H_
