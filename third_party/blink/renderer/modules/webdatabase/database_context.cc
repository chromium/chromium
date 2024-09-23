/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/modules/webdatabase/database_context.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_task.h"
#include "third_party/blink/renderer/modules/webdatabase/database_thread.h"
#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// How the DatabaseContext Life-Cycle works?
// ========================================
// ... in other words, who's keeping the DatabaseContext alive and how long does
// it need to stay alive?
//
// The DatabaseContext is referenced from
// 1. Supplement<ExecutionContext>
// 2. Database
//
// At Birth:
// ========
// We create a DatabaseContext only when there is a need i.e. the script tries
// to open a Database via DatabaseContext::OpenDatabase().
//
// The DatabaseContext constructor will register itself to ExecutionContext as
// a supplement. This lets DatabaseContext keep itself alive until it is
// cleared after ContextDestroyed().
//
// Once a DatabaseContext is associated with a ExecutionContext, it will
// live until after the ExecutionContext destructs. This is true even if
// we don't succeed in opening any Databases for that context. When we do
// succeed in opening Databases for this ExecutionContext, the Database
// will re-use the same DatabaseContext.
//
// At Shutdown:
// ===========
// During shutdown, the DatabaseContext needs to:
// 1. "outlive" the ExecutionContext.
//    - This is needed because the DatabaseContext needs to remove itself from
//    the
//      ExecutionContext's ExecutionContextLifecycleObserver list and
//      ExecutionContextLifecycleObserver
//      list. This removal needs to be executed on the script's thread. Hence,
//      we
//      rely on the ExecutionContext's shutdown process to call
//      Stop() and ContextDestroyed() to give us a chance to clean these up from
//      the script thread.
//
// 2. "outlive" the Databases.
//    - This is because they may make use of the DatabaseContext to execute a
//      close task and shutdown in an orderly manner. When the Databases are
//      destructed, they will release the DatabaseContext reference from the
//      DatabaseThread.
//
// During shutdown, the ExecutionContext is shutting down on the script thread
// while the Databases are shutting down on the DatabaseThread. Hence, there can
// be a race condition as to whether the ExecutionContext or the Databases
// destruct first.
//
// The Members in the Databases and Supplement<ExecutionContext> will ensure
// that the DatabaseContext will outlive Database and ExecutionContext
// regardless of which of the 2 destructs first.

DatabaseContext* DatabaseContext::From(ExecutionContext& context) {
  auto* supplement =
      Supplement<ExecutionContext>::From<DatabaseContext>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<DatabaseContext>(
        context, base::PassKey<DatabaseContext>());
    ProvideTo(context, supplement);
  }
  return supplement;
}

const char DatabaseContext::kSupplementName[] = "DatabaseContext";

DatabaseContext::DatabaseContext(ExecutionContext& context,
                                 base::PassKey<DatabaseContext> passkey)
    : Supplement<ExecutionContext>(context),
      ExecutionContextLifecycleObserver(&context),
      has_open_databases_(false),
      has_requested_termination_(false) {
  DCHECK(IsMainThread());
}

DatabaseContext::~DatabaseContext() = default;

void DatabaseContext::Trace(Visitor* visitor) const {
  visitor->Trace(database_thread_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

// This is called if the associated ExecutionContext is destructing while
// we're still associated with it. That's our cue to disassociate and shutdown.
// To do this, we stop the database and let everything shutdown naturally
// because the database closing process may still make use of this context.
// It is not safe to just delete the context here.
void DatabaseContext::ContextDestroyed() {
  StopDatabases();
}

DatabaseThread* DatabaseContext::GetDatabaseThread() {
  if (!database_thread_ && !has_open_databases_) {
    // It's OK to ask for the m_databaseThread after we've requested
    // termination because we're still using it to execute the closing
    // of the database. However, it is NOT OK to create a new thread
    // after we've requested termination.
    DCHECK(!has_requested_termination_);

    // Create the database thread on first request - but not if at least one
    // database was already opened, because in that case we already had a
    // database thread and terminated it and should not create another.
    database_thread_ = MakeGarbageCollected<DatabaseThread>();
    database_thread_->Start();
  }

  return database_thread_.Get();
}

bool DatabaseContext::DatabaseThreadAvailable() {
  return GetDatabaseThread() && !has_requested_termination_;
}

void DatabaseContext::StopDatabases() {
  // Though we initiate termination of the DatabaseThread here in
  // stopDatabases(), we can't clear the m_databaseThread ref till we get to
  // the destructor. This is because the Databases that are managed by
  // DatabaseThread still rely on this ref between the context and the thread
  // to execute the task for closing the database. By the time we get to the
  // destructor, we're guaranteed that the databases are destructed (which is
  // why our ref count is 0 then and we're destructing). Then, the
  // m_databaseThread RefPtr destructor will deref and delete the
  // DatabaseThread.

  if (DatabaseThreadAvailable()) {
    has_requested_termination_ = true;
    // This blocks until the database thread finishes the cleanup task.
    database_thread_->Terminate();
  }
}

bool DatabaseContext::AllowDatabaseAccess() const {
  return To<LocalDOMWindow>(GetExecutionContext())->document()->IsActive();
}

const SecurityOrigin* DatabaseContext::GetSecurityOrigin() const {
  return GetExecutionContext()->GetSecurityOrigin();
}

bool DatabaseContext::IsContextThread() const {
  return GetExecutionContext()->IsContextThread();
}

static void LogOpenDatabaseError(ExecutionContext* context,
                                 const String& name) {
  STORAGE_DVLOG(1) << "Database " << name << " for origin "
                   << context->GetSecurityOrigin()->ToString()
                   << " not allowed to be established";
}

Database* DatabaseContext::OpenDatabaseInternal(
    const String& name,
    const String& expected_version,
    const String& display_name,
    V8DatabaseCallback* creation_callback,
    bool set_version_in_new_database,
    DatabaseError& error,
    String& error_message) {
  DCHECK_EQ(error, DatabaseError::kNone);

  if (DatabaseTracker::Tracker().CanEstablishDatabase(this, error)) {
    Database* backend = MakeGarbageCollected<Database>(
        this, name, expected_version, display_name);
    if (backend->OpenAndVerifyVersion(set_version_in_new_database, error,
                                      error_message, creation_callback)) {
      return backend;
    }
  }

  DCHECK_NE(error, DatabaseError::kNone);
  switch (error) {
    case DatabaseError::kGenericSecurityError:
      LogOpenDatabaseError(GetExecutionContext(), name);
      return nullptr;

    case DatabaseError::kInvalidDatabaseState:
      LogErrorMessage(GetExecutionContext(), error_message);
      return nullptr;

    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

Database* DatabaseContext::OpenDatabase(const String& name,
                                        const String& expected_version,
                                        const String& display_name,
                                        V8DatabaseCallback* creation_callback,
                                        DatabaseError& error,
                                        String& error_message) {
  DCHECK_EQ(error, DatabaseError::kNone);

  bool set_version_in_new_database = !creation_callback;
  Database* database = OpenDatabaseInternal(
      name, expected_version, display_name, creation_callback,
      set_version_in_new_database, error, error_message);
  if (!database) {
    return nullptr;
  }

  SetHasOpenDatabases();
  ExecutionContext* context = GetExecutionContext();
  DatabaseClient::From(context)->DidOpenDatabase(
      database, context->GetSecurityOrigin()->Host(), name, expected_version);
  DCHECK(database);
  return database;
}

void DatabaseContext::ThrowExceptionForDatabaseError(
    DatabaseError error,
    const String& error_message,
    ExceptionState& exception_state) {
  switch (error) {
    case DatabaseError::kNone:
      return;
    case DatabaseError::kGenericSecurityError:
      exception_state.ThrowSecurityError(error_message);
      return;
    case DatabaseError::kInvalidDatabaseState:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        error_message);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void DatabaseContext::LogErrorMessage(ExecutionContext* context,
                                      const String& message) {
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kStorage,
      mojom::blink::ConsoleMessageLevel::kError, message));
}

}  // namespace blink
