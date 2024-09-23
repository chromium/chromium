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

#include "third_party/blink/renderer/modules/webdatabase/sql_statement.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_statement_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sql_statement_error_callback.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement_backend.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_database.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_statement.h"

namespace blink {

void SQLStatement::OnSuccessV8Impl::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  OnSuccessCallback::Trace(visitor);
}

bool SQLStatement::OnSuccessV8Impl::OnSuccess(SQLTransaction* transaction,
                                              SQLResultSet* result_set) {
  v8::TryCatch try_catch(callback_->GetIsolate());
  try_catch.SetVerbose(true);

  // An exception if any is killed with the v8::TryCatch above and reported
  // to the global exception handler.
  return callback_->handleEvent(nullptr, transaction, result_set).IsJust();
}

void SQLStatement::OnErrorV8Impl::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  OnErrorCallback::Trace(visitor);
}

bool SQLStatement::OnErrorV8Impl::OnError(SQLTransaction* transaction,
                                          SQLError* error) {
  v8::TryCatch try_catch(callback_->GetIsolate());
  try_catch.SetVerbose(true);

  // 4.3.2 Processing model
  // https://www.w3.org/TR/webdatabase/#sqlstatementcallback
  // step 6.(In case of error).2. If the error callback returns false, then move
  // on to the next statement, if any, or onto the next overall step otherwise.
  // step 6.(In case of error).3. Otherwise, the error callback did not return
  // false, or there was no error callback. Jump to the last step in the overall
  // steps.
  bool return_value;
  // An exception if any is killed with the v8::TryCatch above and reported
  // to the global exception handler.
  if (!callback_->handleEvent(nullptr, transaction, error).To(&return_value)) {
    return true;
  }
  return return_value;
}

SQLStatement::SQLStatement(Database* database,
                           OnSuccessCallback* callback,
                           OnErrorCallback* error_callback)
    : success_callback_(callback), error_callback_(error_callback) {
  DCHECK(IsMainThread());

  if (HasCallback() || HasErrorCallback()) {
    async_task_context_.Schedule(database->GetExecutionContext(),
                                 "SQLStatement");
  }
}

void SQLStatement::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(success_callback_);
  visitor->Trace(error_callback_);
}

void SQLStatement::SetBackend(SQLStatementBackend* backend) {
  backend_ = backend;
}

bool SQLStatement::HasCallback() {
  return success_callback_ != nullptr;
}

bool SQLStatement::HasErrorCallback() {
  return error_callback_ != nullptr;
}

bool SQLStatement::PerformCallback(SQLTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(backend_);

  bool callback_error = false;

  OnSuccessCallback* callback = success_callback_.Release();
  OnErrorCallback* error_callback = error_callback_.Release();
  SQLErrorData* error = backend_->SqlError();

  probe::AsyncTask async_task(transaction->GetDatabase()->GetExecutionContext(),
                              &async_task_context_);

  // Call the appropriate statement callback and track if it resulted in an
  // error, because then we need to jump to the transaction error callback.
  if (error) {
    if (error_callback) {
      callback_error = error_callback->OnError(
          transaction, MakeGarbageCollected<SQLError>(*error));
    }
  } else if (callback) {
    callback_error =
        !callback->OnSuccess(transaction, backend_->SqlResultSet());
  }

  return callback_error;
}

}  // namespace blink
