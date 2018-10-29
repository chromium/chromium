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
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "url/origin.h"

namespace content {

namespace {

const char kInvalidOrigin[] = "Origin is invalid";

bool IsValidOrigin(const url::Origin& origin) {
  return !origin.opaque();
}

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

void DoCallCompactionStatusCallback(
    IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabaseCallback
        callback,
    leveldb::Status status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(GetIndexedDBStatus(status));
}

void CallCompactionStatusCallbackOnIOThread(
    scoped_refptr<base::SequencedTaskRunner> io_runner,
    IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabaseCallback
        mojo_callback,
    leveldb::Status status) {
  io_runner->PostTask(FROM_HERE,
                      base::BindOnce(&DoCallCompactionStatusCallback,
                                     std::move(mojo_callback), status));
}

void DoCallAbortStatusCallback(
    IndexedDBDispatcherHost::AbortTransactionsForDatabaseCallback callback,
    leveldb::Status status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(GetIndexedDBStatus(status));
}

void CallAbortStatusCallbackOnIOThread(
    scoped_refptr<base::SequencedTaskRunner> io_runner,
    IndexedDBDispatcherHost::AbortTransactionsForDatabaseCallback mojo_callback,
    leveldb::Status status) {
  io_runner->PostTask(FROM_HERE,
                      base::BindOnce(&DoCallAbortStatusCallback,
                                     std::move(mojo_callback), status));
}

}  // namespace

class IndexedDBDispatcherHost::IDBSequenceHelper {
 public:
  IDBSequenceHelper(
      int ipc_process_id,
      scoped_refptr<IndexedDBContextImpl> indexed_db_context)
      : ipc_process_id_(ipc_process_id),
        indexed_db_context_(std::move(indexed_db_context)) {}
  ~IDBSequenceHelper() {}

  void GetDatabaseInfoOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                  const url::Origin& origin);

  void GetDatabaseNamesOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                   const url::Origin& origin);
  void OpenOnIDBThread(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      const url::Origin& origin,
      const base::string16& name,
      int64_t version,
      int64_t transaction_id);
  void DeleteDatabaseOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                 const url::Origin& origin,
                                 const base::string16& name,
                                 bool force_close);
  void AbortTransactionsAndCompactDatabaseOnIDBThread(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin);
  void AbortTransactionsForDatabaseOnIDBThread(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin);

 private:
  const int ipc_process_id_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IDBSequenceHelper);
};

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id,
    scoped_refptr<IndexedDBContextImpl> indexed_db_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : indexed_db_context_(std::move(indexed_db_context)),
      blob_storage_context_(std::move(blob_storage_context)),
      ipc_process_id_(ipc_process_id),
      idb_helper_(new IDBSequenceHelper(ipc_process_id_,
                                        indexed_db_context_)),
      weak_factory_(this) {
  DCHECK(indexed_db_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  IDBTaskRunner()->DeleteSoon(FROM_HERE, idb_helper_);
}

void IndexedDBDispatcherHost::AddBinding(
    blink::mojom::IDBFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void IndexedDBDispatcherHost::AddDatabaseBinding(
    std::unique_ptr<blink::mojom::IDBDatabase> database,
    blink::mojom::IDBDatabaseAssociatedRequest request) {
  database_bindings_.AddBinding(std::move(database), std::move(request));
}

void IndexedDBDispatcherHost::AddCursorBinding(
    std::unique_ptr<blink::mojom::IDBCursor> cursor,
    blink::mojom::IDBCursorAssociatedRequest request) {
  cursor_bindings_.AddBinding(std::move(cursor), std::move(request));
}

void IndexedDBDispatcherHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings,
          base::Unretained(this)));
}

void IndexedDBDispatcherHost::GetDatabaseInfo(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), IDBTaskRunner()));
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IDBSequenceHelper::GetDatabaseInfoOnIDBThread,
                                base::Unretained(idb_helper_),
                                std::move(callbacks), origin));
}

void IndexedDBDispatcherHost::GetDatabaseNames(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), IDBTaskRunner()));
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IDBSequenceHelper::GetDatabaseNamesOnIDBThread,
                                base::Unretained(idb_helper_),
                                std::move(callbacks), origin));
}

void IndexedDBDispatcherHost::Open(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo database_callbacks_info,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), IDBTaskRunner()));
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks(
      new IndexedDBDatabaseCallbacks(indexed_db_context_,
                                     std::move(database_callbacks_info)));
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IDBSequenceHelper::OpenOnIDBThread,
                     base::Unretained(idb_helper_), std::move(callbacks),
                     std::move(database_callbacks), origin, name, version,
                     transaction_id));
}

void IndexedDBDispatcherHost::DeleteDatabase(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), IDBTaskRunner()));
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IDBSequenceHelper::DeleteDatabaseOnIDBThread,
                     base::Unretained(idb_helper_), std::move(callbacks),
                     origin, name, force_close));
}

void IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabase(
    const url::Origin& origin,
    AbortTransactionsAndCompactDatabaseCallback mojo_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallCompactionStatusCallbackOnIOThread,
      base::ThreadTaskRunnerHandle::Get(), std::move(mojo_callback));
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IDBSequenceHelper::AbortTransactionsAndCompactDatabaseOnIDBThread,
          base::Unretained(idb_helper_), std::move(callback_on_io), origin));
}

void IndexedDBDispatcherHost::AbortTransactionsForDatabase(
    const url::Origin& origin,
    AbortTransactionsForDatabaseCallback mojo_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallAbortStatusCallbackOnIOThread, base::ThreadTaskRunnerHandle::Get(),
      std::move(mojo_callback));
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IDBSequenceHelper::AbortTransactionsForDatabaseOnIDBThread,
          base::Unretained(idb_helper_), std::move(callback_on_io), origin));
}

void IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings() {
  weak_factory_.InvalidateWeakPtrs();
  cursor_bindings_.CloseAllBindings();
  database_bindings_.CloseAllBindings();
}

base::SequencedTaskRunner* IndexedDBDispatcherHost::IDBTaskRunner() const {
  return indexed_db_context_->TaskRunner();
}

void IndexedDBDispatcherHost::IDBSequenceHelper::GetDatabaseInfoOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(callbacks, origin,
                                                        indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::GetDatabaseNamesOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseNames(callbacks, origin,
                                                         indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::OpenOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::TimeTicks begin_time = base::TimeTicks::Now();
  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  callbacks->SetConnectionOpenStartTime(begin_time);
  std::unique_ptr<IndexedDBPendingConnection> connection =
      std::make_unique<IndexedDBPendingConnection>(
          callbacks, database_callbacks, ipc_process_id_, transaction_id,
          version);
  indexed_db_context_->GetIDBFactory()->Open(name, std::move(connection),
                                             origin, indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::DeleteDatabaseOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, callbacks, origin, indexed_db_path, force_close);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::
    AbortTransactionsAndCompactDatabaseOnIDBThread(
        base::OnceCallback<void(leveldb::Status)> callback,
        const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  indexed_db_context_->GetIDBFactory()->AbortTransactionsAndCompactDatabase(
      std::move(callback), origin);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::
    AbortTransactionsForDatabaseOnIDBThread(
        base::OnceCallback<void(leveldb::Status)> callback,
        const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  indexed_db_context_->GetIDBFactory()->AbortTransactionsForDatabase(
      std::move(callback), origin);
}

}  // namespace content
