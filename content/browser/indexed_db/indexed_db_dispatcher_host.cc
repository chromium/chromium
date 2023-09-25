// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_factory_client.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/transaction_impl.h"

namespace content {

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(indexed_db_context_);
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

IndexedDBDispatcherHost::ReceiverContext::ReceiverContext() = default;
IndexedDBDispatcherHost::ReceiverContext::ReceiverContext(
    absl::optional<storage::BucketInfo> bucket,
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote)
    : bucket(bucket),
      client_state_checker(
          base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
              std::move(client_state_checker_remote))) {}

IndexedDBDispatcherHost::ReceiverContext::ReceiverContext(
    IndexedDBDispatcherHost::ReceiverContext&&) noexcept = default;

IndexedDBDispatcherHost::ReceiverContext::~ReceiverContext() = default;

void IndexedDBDispatcherHost::AddReceiver(
    IndexedDBDispatcherHost::ReceiverContext context,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(pending_receiver), std::move(context));
}

void IndexedDBDispatcherHost::GetDatabaseInfo(
    GetDatabaseInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const absl::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(
                blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket->ToBucketLocator());
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      *bucket, indexed_db_path, std::move(callback));
}

void IndexedDBDispatcherHost::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const std::u16string& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const absl::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    IndexedDBFactoryClient(std::move(pending_factory_client), IDBTaskRunner())
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  auto callbacks = std::make_unique<IndexedDBFactoryClient>(
      std::move(pending_factory_client), IDBTaskRunner());
  auto database_callbacks = base::MakeRefCounted<IndexedDBDatabaseCallbacks>(
      indexed_db_context_, std::move(database_callbacks_remote),
      IDBTaskRunner());

  storage::BucketLocator bucket_locator = bucket->ToBucketLocator();
  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket_locator);

  auto create_transaction_callback = base::BindOnce(
      &TransactionImpl::CreateAndBind, std::move(transaction_receiver));
  std::unique_ptr<IndexedDBPendingConnection> connection =
      std::make_unique<IndexedDBPendingConnection>(
          std::move(callbacks), std::move(database_callbacks), transaction_id,
          version, std::move(create_transaction_callback));

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  indexed_db_context_->GetIDBFactory()->Open(
      name, std::move(connection), *bucket, indexed_db_path,
      receivers_.current_context().client_state_checker);
}

void IndexedDBDispatcherHost::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    const std::u16string& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const absl::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    IndexedDBFactoryClient(std::move(pending_factory_client), IDBTaskRunner())
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  auto factory_client = std::make_unique<IndexedDBFactoryClient>(
      std::move(pending_factory_client), IDBTaskRunner());
  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket->ToBucketLocator());
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, std::move(factory_client), *bucket, indexed_db_path, force_close);
}

base::SequencedTaskRunner* IndexedDBDispatcherHost::IDBTaskRunner() const {
  return indexed_db_context_->IDBTaskRunner().get();
}

}  // namespace content
