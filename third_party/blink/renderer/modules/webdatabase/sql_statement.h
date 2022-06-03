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
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_statement_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_statement_error_callback.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_result_set.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Database;
class SQLError;
class SQLStatementBackend;
class SQLTransaction;

class SQLStatement final : public GarbageCollected<SQLStatement> {
 public:
  class OnSuccessCallback : public GarbageCollected<OnSuccessCallback> {
   public:
    virtual ~OnSuccessCallback() = default;
    virtual void Trace(Visitor*) const {}
    virtual bool OnSuccess(SQLTransaction*, SQLResultSet*) = 0;

   protected:
    OnSuccessCallback() = default;
  };

  class OnSuccessV8Impl : public OnSuccessCallback {
   public:
    static OnSuccessV8Impl* Create(V8SQLStatementCallback* callback) {
      return callback ? MakeGarbageCollected<OnSuccessV8Impl>(callback)
                      : nullptr;
    }

    explicit OnSuccessV8Impl(V8SQLStatementCallback* callback)
        : callback_(callback) {}

    void Trace(Visitor*) const override;
    bool OnSuccess(SQLTransaction*, SQLResultSet*) override;

   private:
    Member<V8SQLStatementCallback> callback_;
  };

  class OnErrorCallback : public GarbageCollected<OnErrorCallback> {
   public:
    virtual ~OnErrorCallback() = default;
    virtual void Trace(Visitor*) const {}
    virtual bool OnError(SQLTransaction*, SQLError*) = 0;

   protected:
    OnErrorCallback() = default;
  };

  class OnErrorV8Impl : public OnErrorCallback {
   public:
    static OnErrorV8Impl* Create(V8SQLStatementErrorCallback* callback) {
      return callback ? MakeGarbageCollected<OnErrorV8Impl>(callback) : nullptr;
    }

    explicit OnErrorV8Impl(V8SQLStatementErrorCallback* callback)
        : callback_(callback) {}

    void Trace(Visitor*) const override;
    bool OnError(SQLTransaction*, SQLError*) override;

   private:
    Member<V8SQLStatementErrorCallback> callback_;
  };

  static SQLStatement* Create(Database*, OnSuccessCallback*, OnErrorCallback*);

  SQLStatement(Database*, OnSuccessCallback*, OnErrorCallback*);

  void Trace(Visitor*) const;

  bool PerformCallback(SQLTransaction*);

  void SetBackend(SQLStatementBackend*);

  bool HasCallback();
  bool HasErrorCallback();

 private:
  // The SQLStatementBackend owns the SQLStatement. Hence, the backend is
  // guaranteed to be outlive the SQLStatement, and it is safe for us to refer
  // to the backend using a raw pointer here.
  Member<SQLStatementBackend> backend_;

  Member<OnSuccessCallback> success_callback_;
  Member<OnErrorCallback> error_callback_;

  probe::AsyncTaskId async_task_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_H_
