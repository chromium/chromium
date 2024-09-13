// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/factory_client.h"

#include <forward_list>
#include <memory>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using std::swap;

namespace content::indexed_db {

FactoryClient::FactoryClient(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_client)
    : data_loss_(blink::mojom::IDBDataLoss::None) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_client.is_valid()) {
    remote_.Bind(std::move(pending_client));
    remote_.set_disconnect_handler(base::BindOnce(
        &FactoryClient::OnConnectionError, base::Unretained(this)));
  }
}

FactoryClient::~FactoryClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FactoryClient::OnError(const DatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!remote_) {
    return;
  }
  remote_->Error(error.code(), error.message());
  complete_ = true;
}

void FactoryClient::OnBlocked(int64_t existing_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (sent_blocked_) {
    return;
  }

  sent_blocked_ = true;

  if (remote_) {
    remote_->Blocked(existing_version);
  }
}

void FactoryClient::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<Connection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK(!connection_created_);

  data_loss_ = data_loss_info.status;
  connection_created_ = true;

  if (!remote_) {
    // Don't destroy the connection while the current transaction task queue is
    // being processed.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(connection));
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending =
      Connection::MakeSelfOwnedReceiverAndBindRemote(std::move(connection));
  remote_->UpgradeNeeded(std::move(pending), old_version, data_loss_info.status,
                         data_loss_info.message, metadata);
}

void FactoryClient::OnOpenSuccess(std::unique_ptr<Connection> connection,
                                  const IndexedDBDatabaseMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(connection_created_, !connection);

  // Only create a new connection if one was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<Connection> database_connection;
  if (!connection_created_) {
    database_connection = std::move(connection);
  }

  if (!remote_) {
    if (database_connection) {
      // Don't destroy the connection while the current transaction task queue
      // is being processed.
      base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
          FROM_HERE, std::move(database_connection));
    }
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  if (database_connection) {
    pending_remote = Connection::MakeSelfOwnedReceiverAndBindRemote(
        std::move(database_connection));
  }
  remote_->OpenSuccess(std::move(pending_remote), metadata);
  complete_ = true;
}

void FactoryClient::OnDeleteSuccess(int64_t old_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!remote_) {
    return;
  }
  remote_->DeleteSuccess(old_version);
  complete_ = true;
}

void FactoryClient::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since the renderer-process `IDBFactory` is a self-owned receiver, a
  // disconnection should only occur if the renderer process is gone.
  remote_.reset();
}

}  // namespace content::indexed_db
