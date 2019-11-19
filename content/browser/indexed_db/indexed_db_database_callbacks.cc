// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/public/browser/browser_task_traits.h"

using blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo;

namespace content {

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    scoped_refptr<IndexedDBContextImpl> context,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        callbacks_remote,
    base::SequencedTaskRunner* idb_runner)
    : indexed_db_context_(std::move(context)) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callbacks_remote.is_valid())
    return;
  callbacks_.Bind(std::move(callbacks_remote));
  // |callbacks_| is owned by |this|, so if |this| is destroyed, then
  // |callbacks_| will also be destroyed.  While |callbacks_| is otherwise
  // alive, |this| will always be valid.
  callbacks_.set_disconnect_handler(base::BindOnce(
      &IndexedDBDatabaseCallbacks::OnConnectionError, base::Unretained(this)));
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Transfer |context_| ownership to a new task to prevent re-entrancy through
  // IndexedDBFactory::ContextDestroyed.
  base::SequencedTaskRunnerHandle::Get()->ReleaseSoon(
      FROM_HERE, std::move(indexed_db_context_));
}

void IndexedDBDatabaseCallbacks::OnForcedClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  if (callbacks_)
    callbacks_->ForcedClose();
  complete_ = true;
}

void IndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                 int64_t new_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  if (callbacks_)
    callbacks_->VersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacks::OnAbort(
    const IndexedDBTransaction& transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  if (callbacks_)
    callbacks_->Abort(transaction.id(), error.code(), error.message());
}

void IndexedDBDatabaseCallbacks::OnComplete(
    const IndexedDBTransaction& transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  indexed_db_context_->TransactionComplete(transaction.database()->origin());
  if (callbacks_)
    callbacks_->Complete(transaction.id());
}

void IndexedDBDatabaseCallbacks::OnDatabaseChange(
    blink::mojom::IDBObserverChangesPtr changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->Changes(std::move(changes));
}

void IndexedDBDatabaseCallbacks::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.reset();
}

}  // namespace content
