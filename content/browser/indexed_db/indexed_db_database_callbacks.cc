// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

#include "base/task/post_task.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/public/browser/browser_task_traits.h"

using blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo;

namespace content {

class IndexedDBDatabaseCallbacks::IOThreadHelper {
 public:
  explicit IOThreadHelper(IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info);
  ~IOThreadHelper();

  void SendForcedClose();
  void SendVersionChange(int64_t old_version, int64_t new_version);
  void SendAbort(int64_t transaction_id, const IndexedDBDatabaseError& error);
  void SendComplete(int64_t transaction_id);
  void SendChanges(blink::mojom::IDBObserverChangesPtr changes);
  void OnConnectionError();

 private:
  blink::mojom::IDBDatabaseCallbacksAssociatedPtr callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    scoped_refptr<IndexedDBContextImpl> context,
    IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info)
    : indexed_db_context_(std::move(context)),
      io_helper_(new IOThreadHelper(std::move(callbacks_info))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBDatabaseCallbacks::OnForcedClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(io_helper_);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&IOThreadHelper::SendForcedClose,
                                          base::Unretained(io_helper_.get())));
  complete_ = true;
}

void IndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                 int64_t new_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(io_helper_);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&IOThreadHelper::SendVersionChange,
                                          base::Unretained(io_helper_.get()),
                                          old_version, new_version));
}

void IndexedDBDatabaseCallbacks::OnAbort(
    const IndexedDBTransaction& transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(io_helper_);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&IOThreadHelper::SendAbort,
                                          base::Unretained(io_helper_.get()),
                                          transaction.id(), error));
}

void IndexedDBDatabaseCallbacks::OnComplete(
    const IndexedDBTransaction& transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  indexed_db_context_->TransactionComplete(transaction.database()->origin());
  DCHECK(io_helper_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&IOThreadHelper::SendComplete,
                     base::Unretained(io_helper_.get()), transaction.id()));
}

void IndexedDBDatabaseCallbacks::OnDatabaseChange(
    blink::mojom::IDBObserverChangesPtr changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_helper_);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&IOThreadHelper::SendChanges,
                     base::Unretained(io_helper_.get()), std::move(changes)));
}

IndexedDBDatabaseCallbacks::IOThreadHelper::IOThreadHelper(
    IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info) {
  if (!callbacks_info.is_valid())
    return;
  callbacks_.Bind(std::move(callbacks_info));
  callbacks_.set_connection_error_handler(base::BindOnce(
      &IOThreadHelper::OnConnectionError, base::Unretained(this)));
}

IndexedDBDatabaseCallbacks::IOThreadHelper::~IOThreadHelper() {}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendForcedClose() {
  if (callbacks_)
    callbacks_->ForcedClose();
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendVersionChange(
    int64_t old_version,
    int64_t new_version) {
  if (callbacks_)
    callbacks_->VersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendAbort(
    int64_t transaction_id,
    const IndexedDBDatabaseError& error) {
  if (callbacks_)
    callbacks_->Abort(transaction_id, error.code(), error.message());
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendComplete(
    int64_t transaction_id) {
  if (callbacks_)
    callbacks_->Complete(transaction_id);
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::SendChanges(
    blink::mojom::IDBObserverChangesPtr changes) {
  if (callbacks_)
    callbacks_->Changes(std::move(changes));
}

void IndexedDBDatabaseCallbacks::IOThreadHelper::OnConnectionError() {
  callbacks_.reset();
}

}  // namespace content
