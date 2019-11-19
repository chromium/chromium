/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"

#include <memory>

#include "base/location.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_client.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/quota_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

static void DatabaseClosed(Database* database) {
  WebDatabaseHost::GetInstance().DatabaseClosed(*database->GetSecurityOrigin(),
                                                database->StringIdentifier());
}

DatabaseTracker& DatabaseTracker::Tracker() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(DatabaseTracker, tracker, ());
  return tracker;
}

DatabaseTracker::DatabaseTracker() {
}

bool DatabaseTracker::CanEstablishDatabase(DatabaseContext* database_context,
                                           DatabaseError& error) {
  ExecutionContext* execution_context = database_context->GetExecutionContext();
  bool success =
      DatabaseClient::From(execution_context)->AllowDatabase(execution_context);
  if (!success)
    error = DatabaseError::kGenericSecurityError;
  return success;
}

String DatabaseTracker::FullPathForDatabase(const SecurityOrigin* origin,
                                            const String& name,
                                            bool) {
  return String(Platform::Current()->DatabaseCreateOriginIdentifier(
             WebSecurityOrigin(origin))) +
         "/" + name + "#";
}

void DatabaseTracker::AddOpenDatabase(Database* database) {
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    open_database_map_ = std::make_unique<DatabaseOriginMap>();

  String origin_string = database->GetSecurityOrigin()->ToRawString();
  DatabaseNameMap* name_map = open_database_map_->at(origin_string);
  if (!name_map) {
    name_map = new DatabaseNameMap();
    open_database_map_->Set(origin_string, name_map);
  }

  String name(database->StringIdentifier());
  DatabaseSet* database_set = name_map->at(name);
  if (!database_set) {
    database_set = new DatabaseSet();
    name_map->Set(name, database_set);
  }

  database_set->insert(database);
}

void DatabaseTracker::RemoveOpenDatabase(Database* database) {
  {
    MutexLocker open_database_map_lock(open_database_map_guard_);
    String origin_string = database->GetSecurityOrigin()->ToRawString();
    DCHECK(open_database_map_);
    DatabaseNameMap* name_map = open_database_map_->at(origin_string);
    if (!name_map)
      return;

    String name(database->StringIdentifier());
    DatabaseSet* database_set = name_map->at(name);
    if (!database_set)
      return;

    DatabaseSet::iterator found = database_set->find(database);
    if (found == database_set->end())
      return;

    database_set->erase(found);
    if (database_set->IsEmpty()) {
      name_map->erase(name);
      delete database_set;
      if (name_map->IsEmpty()) {
        open_database_map_->erase(origin_string);
        delete name_map;
      }
    }
  }
  DatabaseClosed(database);
}

void DatabaseTracker::PrepareToOpenDatabase(Database* database) {
  DCHECK(
      database->GetDatabaseContext()->GetExecutionContext()->IsContextThread());

  // This is an asynchronous call to the browser to open the database, however
  // we can't actually use the database until we revieve an RPC back that
  // advises is of the actual size of the database, so there is a race condition
  // where the database is in an unusable state. To assist, we will record the
  // size of the database straight away so we can use it immediately, and the
  // real size will eventually be updated by the RPC from the browser.
  WebDatabaseHost::GetInstance().DatabaseOpened(
      *database->GetSecurityOrigin(), database->StringIdentifier(),
      database->DisplayName(), database->EstimatedSize());
  // We write a temporary size of 0 to the QuotaTracker - we will be updated
  // with the correct size via RPC asynchronously.
  QuotaTracker::Instance().UpdateDatabaseSize(database->GetSecurityOrigin(),
                                              database->StringIdentifier(), 0);
}

void DatabaseTracker::FailedToOpenDatabase(Database* database) {
  DatabaseClosed(database);
}

uint64_t DatabaseTracker::GetMaxSizeForDatabase(const Database* database) {
  uint64_t space_available = 0;
  uint64_t database_size = 0;
  QuotaTracker::Instance().GetDatabaseSizeAndSpaceAvailableToOrigin(
      database->GetSecurityOrigin(), database->StringIdentifier(),
      &database_size, &space_available);
  return database_size + space_available;
}

void DatabaseTracker::CloseDatabasesImmediately(const SecurityOrigin* origin,
                                                const String& name) {
  String origin_string = origin->ToRawString();
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    return;

  DatabaseNameMap* name_map = open_database_map_->at(origin_string);
  if (!name_map)
    return;

  DatabaseSet* database_set = name_map->at(name);
  if (!database_set)
    return;

  // We have to call closeImmediately() on the context thread.
  for (DatabaseSet::iterator it = database_set->begin();
       it != database_set->end(); ++it) {
    PostCrossThreadTask(
        *(*it)->GetDatabaseTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&DatabaseTracker::CloseOneDatabaseImmediately,
                            CrossThreadUnretained(this), origin_string, name,
                            *it));
  }
}

void DatabaseTracker::ForEachOpenDatabaseInPage(Page* page,
                                                DatabaseCallback callback) {
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    return;
  for (auto& origin_map : *open_database_map_) {
    for (auto& name_database_set : *origin_map.value) {
      for (Database* database : *name_database_set.value) {
        ExecutionContext* context = database->GetExecutionContext();
        if (To<Document>(context)->GetPage() == page)
          callback.Run(database);
      }
    }
  }
}

void DatabaseTracker::CloseOneDatabaseImmediately(const String& origin_string,
                                                  const String& name,
                                                  Database* database) {
  // First we have to confirm the 'database' is still in our collection.
  {
    MutexLocker open_database_map_lock(open_database_map_guard_);
    if (!open_database_map_)
      return;

    DatabaseNameMap* name_map = open_database_map_->at(origin_string);
    if (!name_map)
      return;

    DatabaseSet* database_set = name_map->at(name);
    if (!database_set)
      return;

    if (!database_set->Contains(database))
      return;
  }

  // And we have to call closeImmediately() without our collection lock being
  // held.
  database->CloseImmediately();
}

}  // namespace blink
