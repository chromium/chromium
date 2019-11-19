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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database_error.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class Database;
class DatabaseContext;
class ExceptionState;
class ExecutionContext;
class SecurityOrigin;
class V8DatabaseCallback;

class DatabaseManager {
  USING_FAST_MALLOC(DatabaseManager);

 public:
  static DatabaseManager& Manager();

  // These 2 methods are for DatabaseContext (un)registration, and should only
  // be called by the DatabaseContext constructor and destructor.
  void RegisterDatabaseContext(DatabaseContext*);
  void UnregisterDatabaseContext(DatabaseContext*);

#if DCHECK_IS_ON()
  void DidConstructDatabaseContext();
  void DidDestructDatabaseContext();
#else
  void DidConstructDatabaseContext() {}
  void DidDestructDatabaseContext() {}
#endif

  static void ThrowExceptionForDatabaseError(DatabaseError,
                                             const String& error_message,
                                             ExceptionState&);

  Database* OpenDatabase(ExecutionContext*,
                         const String& name,
                         const String& expected_version,
                         const String& display_name,
                         uint32_t estimated_size,
                         V8DatabaseCallback*,
                         DatabaseError&,
                         String& error_message);

  String FullPathForDatabase(const SecurityOrigin*,
                             const String& name,
                             bool create_if_does_not_exist = true);

 private:
  DatabaseManager();
  ~DatabaseManager();

  // This gets a DatabaseContext for the specified ExecutionContext.
  // If one doesn't already exist, it will create a new one.
  DatabaseContext* DatabaseContextFor(ExecutionContext*);
  // This gets a DatabaseContext for the specified ExecutionContext if
  // it already exist previously. Otherwise, it returns 0.
  DatabaseContext* ExistingDatabaseContextFor(ExecutionContext*);

  Database* OpenDatabaseInternal(ExecutionContext*,
                                 const String& name,
                                 const String& expected_version,
                                 const String& display_name,
                                 uint32_t estimated_size,
                                 V8DatabaseCallback*,
                                 bool set_version_in_new_database,
                                 DatabaseError&,
                                 String& error_message);

  static void LogErrorMessage(ExecutionContext*, const String& message);

  // context_map_ can have two or more entries even though we don't support
  // Web SQL on workers because single Blink process can have multiple main
  // contexts.
  typedef HeapHashMap<Member<ExecutionContext>, Member<DatabaseContext>>
      ContextMap;
  Persistent<ContextMap> context_map_;
#if DCHECK_IS_ON()
  int database_context_registered_count_ = 0;
  int database_context_instance_count_ = 0;
#endif

  DISALLOW_COPY_AND_ASSIGN(DatabaseManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_MANAGER_H_
