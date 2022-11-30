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

#include "third_party/blink/renderer/modules/webdatabase/database_thread.h"

#include <memory>
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_task.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_client.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_coordinator.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

DatabaseThread::DatabaseThread()
    : transaction_client_(std::make_unique<SQLTransactionClient>()),
      cleanup_sync_(nullptr),
      termination_requested_(false) {
  DCHECK(IsMainThread());
}

DatabaseThread::~DatabaseThread() {
  DCHECK(open_database_set_.empty());
  DCHECK(!thread_);
}

void DatabaseThread::Trace(Visitor* visitor) const {}

void DatabaseThread::Start() {
  DCHECK(IsMainThread());
  if (thread_)
    return;
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kDatabaseThread).SetSupportsGC(true));
  PostCrossThreadTask(*thread_->GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&DatabaseThread::SetupDatabaseThread,
                                          WrapCrossThreadPersistent(this)));
}

void DatabaseThread::SetupDatabaseThread() {
  DCHECK(thread_->IsCurrentThread());
  transaction_coordinator_ = MakeGarbageCollected<SQLTransactionCoordinator>();
}

void DatabaseThread::Terminate() {
  DCHECK(IsMainThread());
  base::WaitableEvent sync;
  {
    base::AutoLock lock(termination_requested_lock_);
    DCHECK(!termination_requested_);
    termination_requested_ = true;
    cleanup_sync_ = &sync;
    STORAGE_DVLOG(1) << "DatabaseThread " << this << " was asked to terminate";
    PostCrossThreadTask(
        *thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&DatabaseThread::CleanupDatabaseThread,
                            WrapCrossThreadPersistent(this)));
  }
  sync.Wait();
  // The Thread destructor blocks until all the tasks of the database
  // thread are processed. However, it shouldn't block at all because
  // the database thread has already finished processing the cleanup task.
  thread_.reset();
}

void DatabaseThread::CleanupDatabaseThread() {
  DCHECK(IsDatabaseThread());

  STORAGE_DVLOG(1) << "Cleaning up DatabaseThread " << this;

  // Clean up the list of all pending transactions on this database thread
  transaction_coordinator_->Shutdown();

  // Close the databases that we ran transactions on. This ensures that if any
  // transactions are still open, they are rolled back and we don't leave the
  // database in an inconsistent or locked state.
  if (open_database_set_.size() > 0) {
    // As the call to close will modify the original set, we must take a copy to
    // iterate over.
    HashSet<CrossThreadPersistent<Database>> open_set_copy;
    open_set_copy.swap(open_database_set_);
    HashSet<CrossThreadPersistent<Database>>::iterator end =
        open_set_copy.end();
    for (HashSet<CrossThreadPersistent<Database>>::iterator it =
             open_set_copy.begin();
         it != end; ++it)
      (*it)->Close();
  }
  open_database_set_.clear();

  thread_->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&DatabaseThread::CleanupDatabaseThreadCompleted,
                               WrapCrossThreadPersistent(this)));
}

void DatabaseThread::CleanupDatabaseThreadCompleted() {
  DCHECK(thread_->IsCurrentThread());
  if (cleanup_sync_)  // Someone wanted to know when we were done cleaning up.
    cleanup_sync_->Signal();
}

void DatabaseThread::RecordDatabaseOpen(Database* database) {
  DCHECK(IsDatabaseThread());
  DCHECK(database);
  DCHECK(!open_database_set_.Contains(database));
  base::AutoLock lock(termination_requested_lock_);
  if (!termination_requested_)
    open_database_set_.insert(database);
}

void DatabaseThread::RecordDatabaseClosed(Database* database) {
  DCHECK(IsDatabaseThread());
  DCHECK(database);
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(termination_requested_lock_);
    DCHECK(termination_requested_ || open_database_set_.Contains(database));
  }
#endif
  open_database_set_.erase(database);
}

bool DatabaseThread::IsDatabaseOpen(Database* database) {
  DCHECK(IsDatabaseThread());
  DCHECK(database);
  base::AutoLock lock(termination_requested_lock_);
  return !termination_requested_ && open_database_set_.Contains(database);
}

bool DatabaseThread::IsDatabaseThread() const {
  // This function is called only from the main thread or the database
  // thread. If we are not in the main thread, we are in the database thread.
  return !IsMainThread();
}

void DatabaseThread::ScheduleTask(std::unique_ptr<DatabaseTask> task) {
  DCHECK(thread_);
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(termination_requested_lock_);
    DCHECK(!termination_requested_);
  }
#endif
  // Thread takes ownership of the task.
  PostCrossThreadTask(*thread_->GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&DatabaseTask::Run, std::move(task)));
}

}  // namespace blink
