/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webdatabase/database_manager.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database_task.h"
#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

static DatabaseManager* g_database_manager;

DatabaseManager& DatabaseManager::Manager() {
  DCHECK(IsMainThread());
  if (!g_database_manager)
    g_database_manager = new DatabaseManager();
  return *g_database_manager;
}

DatabaseManager::DatabaseManager()
    : context_map_(MakeGarbageCollected<ContextMap>()) {}

DatabaseManager::~DatabaseManager() = default;

DatabaseContext* DatabaseManager::ExistingDatabaseContextFor(
    ExecutionContext* context) {
#if DCHECK_IS_ON()
  DCHECK_GE(database_context_registered_count_, 0);
  DCHECK_GE(database_context_instance_count_, 0);
  DCHECK_LE(database_context_registered_count_,
            database_context_instance_count_);
#endif
  return context_map_->at(context);
}

DatabaseContext* DatabaseManager::DatabaseContextFor(
    ExecutionContext* context) {
  if (DatabaseContext* database_context = ExistingDatabaseContextFor(context))
    return database_context;
  return DatabaseContext::Create(context);
}

void DatabaseManager::RegisterDatabaseContext(
    DatabaseContext* database_context) {
  ExecutionContext* context = database_context->GetExecutionContext();
  context_map_->Set(context, database_context);
#if DCHECK_IS_ON()
  database_context_registered_count_++;
#endif
}

void DatabaseManager::UnregisterDatabaseContext(
    DatabaseContext* database_context) {
  ExecutionContext* context = database_context->GetExecutionContext();
  DCHECK(context_map_->at(context));
#if DCHECK_IS_ON()
  database_context_registered_count_--;
#endif
  context_map_->erase(context);
}

#if DCHECK_IS_ON()
void DatabaseManager::DidConstructDatabaseContext() {
  database_context_instance_count_++;
}

void DatabaseManager::DidDestructDatabaseContext() {
  database_context_instance_count_--;
  DCHECK_LE(database_context_registered_count_,
            database_context_instance_count_);
}
#endif

void DatabaseManager::ThrowExceptionForDatabaseError(
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
      NOTREACHED();
  }
}

static void LogOpenDatabaseError(ExecutionContext* context,
                                 const String& name) {
  STORAGE_DVLOG(1) << "Database " << name << " for origin "
                   << context->GetSecurityOrigin()->ToString()
                   << " not allowed to be established";
}

Database* DatabaseManager::OpenDatabaseInternal(
    ExecutionContext* context,
    const String& name,
    const String& expected_version,
    const String& display_name,
    uint32_t estimated_size,
    V8DatabaseCallback* creation_callback,
    bool set_version_in_new_database,
    DatabaseError& error,
    String& error_message) {
  DCHECK_EQ(error, DatabaseError::kNone);

  DatabaseContext* backend_context = DatabaseContextFor(context)->Backend();
  if (DatabaseTracker::Tracker().CanEstablishDatabase(backend_context, error)) {
    Database* backend = MakeGarbageCollected<Database>(
        backend_context, name, expected_version, display_name, estimated_size);
    if (backend->OpenAndVerifyVersion(set_version_in_new_database, error,
                                      error_message, creation_callback))
      return backend;
  }

  DCHECK_NE(error, DatabaseError::kNone);
  switch (error) {
    case DatabaseError::kGenericSecurityError:
      LogOpenDatabaseError(context, name);
      return nullptr;

    case DatabaseError::kInvalidDatabaseState:
      LogErrorMessage(context, error_message);
      return nullptr;

    default:
      NOTREACHED();
  }
  return nullptr;
}

Database* DatabaseManager::OpenDatabase(ExecutionContext* context,
                                        const String& name,
                                        const String& expected_version,
                                        const String& display_name,
                                        uint32_t estimated_size,
                                        V8DatabaseCallback* creation_callback,
                                        DatabaseError& error,
                                        String& error_message) {
  DCHECK_EQ(error, DatabaseError::kNone);

  bool set_version_in_new_database = !creation_callback;
  Database* database = OpenDatabaseInternal(
      context, name, expected_version, display_name, estimated_size,
      creation_callback, set_version_in_new_database, error, error_message);
  if (!database)
    return nullptr;

  DatabaseContextFor(context)->SetHasOpenDatabases();
  DatabaseClient::From(context)->DidOpenDatabase(
      database, context->GetSecurityOrigin()->Host(), name, expected_version);
  DCHECK(database);
  return database;
}

String DatabaseManager::FullPathForDatabase(const SecurityOrigin* origin,
                                            const String& name,
                                            bool create_if_does_not_exist) {
  return DatabaseTracker::Tracker().FullPathForDatabase(
      origin, name, create_if_does_not_exist);
}

void DatabaseManager::LogErrorMessage(ExecutionContext* context,
                                      const String& message) {
  context->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kStorage,
                             mojom::ConsoleMessageLevel::kError, message));
}

}  // namespace blink
