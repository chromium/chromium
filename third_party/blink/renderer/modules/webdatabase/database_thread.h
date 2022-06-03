/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_THREAD_H_

#include <memory>
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class Database;
class DatabaseTask;
class SQLTransactionClient;
class SQLTransactionCoordinator;

class DatabaseThread final : public GarbageCollected<DatabaseThread> {
 public:
  DatabaseThread();
  ~DatabaseThread();
  void Trace(Visitor*) const;

  // Callable only from the main thread.
  void Start();
  void Terminate();

  // Callable from the main thread or the database thread.
  void ScheduleTask(std::unique_ptr<DatabaseTask>);
  bool IsDatabaseThread() const;

  // Callable only from the database thread.
  void RecordDatabaseOpen(Database*);
  void RecordDatabaseClosed(Database*);
  bool IsDatabaseOpen(Database*);

  SQLTransactionClient* TransactionClient() {
    return transaction_client_.get();
  }
  SQLTransactionCoordinator* TransactionCoordinator() {
    return transaction_coordinator_.Get();
  }

 private:
  void SetupDatabaseThread();
  void CleanupDatabaseThread();
  void CleanupDatabaseThreadCompleted();

  std::unique_ptr<blink::Thread> thread_;

  // This set keeps track of the open databases that have been used on this
  // thread.  This must be updated in the database thread though it is
  // constructed and destructed in the context thread.
  HashSet<CrossThreadPersistent<Database>> open_database_set_;

  std::unique_ptr<SQLTransactionClient> transaction_client_;
  CrossThreadPersistent<SQLTransactionCoordinator> transaction_coordinator_;
  base::WaitableEvent* cleanup_sync_;

  Mutex termination_requested_mutex_;
  bool termination_requested_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_THREAD_H_
