// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/transaction_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "url/origin.h"

namespace content {

namespace {

blink::mojom::IDBStatus GetIndexedDBStatus(leveldb::Status status) {
  if (status.ok())
    return blink::mojom::IDBStatus::OK;
  else if (status.IsNotFound())
    return blink::mojom::IDBStatus::NotFound;
  else if (status.IsCorruption())
    return blink::mojom::IDBStatus::Corruption;
  else if (status.IsNotSupportedError())
    return blink::mojom::IDBStatus::NotSupported;
  else if (status.IsInvalidArgument())
    return blink::mojom::IDBStatus::InvalidArgument;
  else
    return blink::mojom::IDBStatus::IOError;
}

void CallCompactionStatusCallbackOnIDBThread(
    IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabaseCallback
        mojo_callback,
    leveldb::Status status) {
  std::move(mojo_callback).Run(GetIndexedDBStatus(status));
}

void CallAbortStatusCallbackOnIDBThread(
    IndexedDBDispatcherHost::AbortTransactionsForDatabaseCallback mojo_callback,
    leveldb::Status status) {
  std::move(mojo_callback).Run(GetIndexedDBStatus(status));
}

}  // namespace

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id,
    scoped_refptr<IndexedDBContextImpl> indexed_db_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : indexed_db_context_(std::move(indexed_db_context)),
      blob_storage_context_(std::move(blob_storage_context)),
      ipc_process_id_(ipc_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(indexed_db_context_);
  DCHECK(blob_storage_context_);
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBDispatcherHost::AddReceiver(
    int render_process_id,
    int render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(render_process_id, ipc_process_id_);
  receivers_.Add(this, std::move(pending_receiver),
                 {origin, IndexedDBExecutionContextConnectionTracker(
                              render_process_id, render_frame_id)});
}

void IndexedDBDispatcherHost::AddDatabaseBinding(
    std::unique_ptr<blink::mojom::IDBDatabase> database,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabase>
        pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_receivers_.Add(std::move(database), std::move(pending_receiver));
}

mojo::PendingAssociatedRemote<blink::mojom::IDBCursor>
IndexedDBDispatcherHost::CreateCursorBinding(
    const url::Origin& origin,
    std::unique_ptr<IndexedDBCursor> cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto cursor_impl = std::make_unique<CursorImpl>(std::move(cursor), origin,
                                                  this, IDBTaskRunner());
  auto* cursor_impl_ptr = cursor_impl.get();
  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> remote;
  mojo::ReceiverId receiver_id = cursor_receivers_.Add(
      std::move(cursor_impl), remote.InitWithNewEndpointAndPassReceiver());
  cursor_impl_ptr->OnRemoveBinding(
      base::BindOnce(&IndexedDBDispatcherHost::RemoveCursorBinding,
                     weak_factory_.GetWeakPtr(), receiver_id));
  return remote;
}

void IndexedDBDispatcherHost::RemoveCursorBinding(
    mojo::ReceiverId receiver_id) {
  cursor_receivers_.Remove(receiver_id);
}

void IndexedDBDispatcherHost::AddTransactionBinding(
    std::unique_ptr<blink::mojom::IDBTransaction> transaction,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transaction_receivers_.Add(std::move(transaction), std::move(receiver));
}

void IndexedDBDispatcherHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Since |this| is destructed on the IDB task runner, the next call would be
  // issued and run before any destruction event.  This guarantees that the
  // base::Unretained(this) usage is safe below.
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings,
          base::Unretained(this)));
}

void IndexedDBDispatcherHost::GetDatabaseInfo(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(pending_callbacks), IDBTaskRunner()));
  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      std::move(callbacks), context.origin, indexed_db_path);
}

void IndexedDBDispatcherHost::GetDatabaseNames(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(pending_callbacks), IDBTaskRunner()));
  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseNames(
      std::move(callbacks), context.origin, indexed_db_path);
}

void IndexedDBDispatcherHost::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const base::string16& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(pending_callbacks), IDBTaskRunner()));
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks(
      new IndexedDBDatabaseCallbacks(indexed_db_context_,
                                     std::move(database_callbacks_remote),
                                     IDBTaskRunner()));
  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  auto create_transaction_callback = base::BindOnce(
      &IndexedDBDispatcherHost::CreateAndBindTransactionImpl, AsWeakPtr(),
      std::move(transaction_receiver), context.origin);
  std::unique_ptr<IndexedDBPendingConnection> connection =
      std::make_unique<IndexedDBPendingConnection>(
          std::move(callbacks), std::move(database_callbacks),
          context.connection_tracker.CreateHandle(), transaction_id, version,
          std::move(create_transaction_callback));
  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  indexed_db_context_->GetIDBFactory()->Open(name, std::move(connection),
                                             context.origin, indexed_db_path);
}

void IndexedDBDispatcherHost::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    const base::string16& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(pending_callbacks), IDBTaskRunner()));
  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, std::move(callbacks), context.origin, indexed_db_path, force_close);
}

void IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabase(
    AbortTransactionsAndCompactDatabaseCallback mojo_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallCompactionStatusCallbackOnIDBThread, std::move(mojo_callback));
  indexed_db_context_->GetIDBFactory()->AbortTransactionsAndCompactDatabase(
      std::move(callback_on_io), context.origin);
}

void IndexedDBDispatcherHost::AbortTransactionsForDatabase(
    AbortTransactionsForDatabaseCallback mojo_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = receivers_.current_context();
  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallAbortStatusCallbackOnIDBThread, std::move(mojo_callback));
  indexed_db_context_->GetIDBFactory()->AbortTransactionsForDatabase(
      std::move(callback_on_io), context.origin);
}

void IndexedDBDispatcherHost::CreateAndBindTransactionImpl(
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    const url::Origin& origin,
    base::WeakPtr<IndexedDBTransaction> transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto transaction_impl = std::make_unique<TransactionImpl>(
      transaction, origin, this->AsWeakPtr(), IDBTaskRunner());
  AddTransactionBinding(std::move(transaction_impl),
                        std::move(transaction_receiver));
}

void IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  cursor_receivers_.Clear();
  database_receivers_.Clear();
  transaction_receivers_.Clear();
}

base::SequencedTaskRunner* IndexedDBDispatcherHost::IDBTaskRunner() const {
  return indexed_db_context_->TaskRunner();
}

}  // namespace content
