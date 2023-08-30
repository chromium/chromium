/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webdatabase/database_task.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database_thread.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"

namespace blink {

DatabaseTask::DatabaseTask(Database* database,
                           base::WaitableEvent* complete_event)
    : database_(database),
      complete_event_(complete_event)
#if DCHECK_IS_ON()
      ,
      complete_(false)
#endif
{
}

DatabaseTask::~DatabaseTask() {
#if DCHECK_IS_ON()
  DCHECK(complete_ || !complete_event_);
#endif
}

void DatabaseTask::Run() {
// Database tasks are meant to be used only once, so make sure this one hasn't
// been performed before.
#if DCHECK_IS_ON()
  DCHECK(!complete_);
#endif

  if (!complete_event_ &&
      !database_->GetDatabaseContext()->GetDatabaseThread()->IsDatabaseOpen(
          database_.Get())) {
    TaskCancelled();
#if DCHECK_IS_ON()
    complete_ = true;
#endif
    return;
  }
#if DCHECK_IS_ON()
  STORAGE_DVLOG(1) << "Performing " << DebugTaskName() << " " << this;
#endif
  database_->ResetAuthorizer();
  DoPerformTask();

  if (complete_event_)
    complete_event_->Signal();

#if DCHECK_IS_ON()
  complete_ = true;
#endif
}

// *** DatabaseOpenTask ***
// Opens the database file and verifies the version matches the expected
// version.

Database::DatabaseOpenTask::DatabaseOpenTask(
    Database* database,
    bool set_version_in_new_database,
    base::WaitableEvent* complete_event,
    DatabaseError& error,
    String& error_message,
    bool& success)
    : DatabaseTask(database, complete_event),
      set_version_in_new_database_(set_version_in_new_database),
      error_(error),
      error_message_(error_message),
      success_(success) {
  DCHECK(complete_event);  // A task with output parameters is supposed to be
                           // synchronous.
}

void Database::DatabaseOpenTask::DoPerformTask() {
  String error_message;
  *success_ = GetDatabase()->PerformOpenAndVerify(set_version_in_new_database_,
                                                  *error_, error_message);
  if (!*success_) {
    (*error_message_) = error_message;
  }
}

#if DCHECK_IS_ON()
const char* Database::DatabaseOpenTask::DebugTaskName() const {
  return "DatabaseOpenTask";
}
#endif

// *** DatabaseCloseTask ***
// Closes the database.

Database::DatabaseCloseTask::DatabaseCloseTask(
    Database* database,
    base::WaitableEvent* complete_event)
    : DatabaseTask(database, complete_event) {}

void Database::DatabaseCloseTask::DoPerformTask() {
  GetDatabase()->Close();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseCloseTask::DebugTaskName() const {
  return "DatabaseCloseTask";
}
#endif

// *** DatabaseTransactionTask ***
// Starts a transaction that will report its results via a callback.

Database::DatabaseTransactionTask::DatabaseTransactionTask(
    SQLTransactionBackend* transaction)
    : DatabaseTask(transaction->GetDatabase(), nullptr),
      transaction_(transaction) {}

Database::DatabaseTransactionTask::~DatabaseTransactionTask() = default;

void Database::DatabaseTransactionTask::DoPerformTask() {
  transaction_->PerformNextStep();
}

void Database::DatabaseTransactionTask::TaskCancelled() {
  // If the task is being destructed without the transaction ever being run,
  // then we must either have an error or an interruption. Give the
  // transaction a chance to clean up since it may not have been able to
  // run to its clean up state.

  // Transaction phase 2 cleanup. See comment on "What happens if a
  // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.

  transaction_->NotifyDatabaseThreadIsShuttingDown();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseTransactionTask::DebugTaskName() const {
  return "DatabaseTransactionTask";
}
#endif

// *** DatabaseTableNamesTask ***
// Retrieves a list of all tables in the database - for WebInspector support.

Database::DatabaseTableNamesTask::DatabaseTableNamesTask(
    Database* database,
    base::WaitableEvent* complete_event,
    Vector<String>& names)
    : DatabaseTask(database, complete_event), table_names_(names) {
  DCHECK(complete_event);  // A task with output parameters is supposed to be
                           // synchronous.
}

void Database::DatabaseTableNamesTask::DoPerformTask() {
  (*table_names_) = GetDatabase()->PerformGetTableNames();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseTableNamesTask::DebugTaskName() const {
  return "DatabaseTableNamesTask";
}
#endif

}  // namespace blink
