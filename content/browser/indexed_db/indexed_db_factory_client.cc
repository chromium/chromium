// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory_client.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/database_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using std::swap;

namespace content {

namespace {

// The following two objects protect the given objects from being destructed
// while the current transaction task queue is being processed.
class SafeConnectionWrapper {
 public:
  explicit SafeConnectionWrapper(
      std::unique_ptr<IndexedDBConnection> connection)
      : connection_(std::move(connection)),
        idb_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  SafeConnectionWrapper(const SafeConnectionWrapper&) = delete;
  SafeConnectionWrapper& operator=(const SafeConnectionWrapper&) = delete;

  ~SafeConnectionWrapper() {
    if (connection_) {
      idb_runner_->PostTask(
          FROM_HERE, base::BindOnce(
                         [](std::unique_ptr<IndexedDBConnection> connection) {
                           connection->CloseAndReportForceClose();
                         },
                         std::move(connection_)));
    }
  }
  SafeConnectionWrapper(SafeConnectionWrapper&& other) = default;

  std::unique_ptr<IndexedDBConnection> connection_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
};

class SafeCursorWrapper {
 public:
  explicit SafeCursorWrapper(std::unique_ptr<IndexedDBCursor> cursor)
      : cursor_(std::move(cursor)),
        idb_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  SafeCursorWrapper(const SafeCursorWrapper&) = delete;
  SafeCursorWrapper& operator=(const SafeCursorWrapper&) = delete;

  ~SafeCursorWrapper() {
    if (cursor_) {
      idb_runner_->DeleteSoon(FROM_HERE, cursor_.release());
    }
  }
  SafeCursorWrapper(SafeCursorWrapper&& other) = default;

  std::unique_ptr<IndexedDBCursor> cursor_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
};

}  // namespace

IndexedDBFactoryClient::IndexedDBFactoryClient(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_client,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : data_loss_(blink::mojom::IDBDataLoss::None),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_client.is_valid()) {
    remote_.Bind(std::move(pending_client));
    // |remote_| is owned by |this|, so if |this| is destroyed, then
    // |remote_| will also be destroyed.  While |remote_| is
    // otherwise alive, |this| will always be valid.
    remote_.set_disconnect_handler(base::BindOnce(
        &IndexedDBFactoryClient::OnConnectionError, base::Unretained(this)));
  }
}

IndexedDBFactoryClient::~IndexedDBFactoryClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBFactoryClient::OnError(const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!remote_) {
    return;
  }
  remote_->Error(error.code(), error.message());
  complete_ = true;
}

void IndexedDBFactoryClient::OnBlocked(int64_t existing_version) {
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

void IndexedDBFactoryClient::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK(!connection_created_);

  data_loss_ = data_loss_info.status;
  connection_created_ = true;

  SafeConnectionWrapper wrapper(std::move(connection));
  if (!remote_) {
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending =
      DatabaseImpl::CreateAndBind(std::move(wrapper.connection_));
  remote_->UpgradeNeeded(std::move(pending), old_version, data_loss_info.status,
                         data_loss_info.message, metadata);
}

void IndexedDBFactoryClient::OnOpenSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(connection_created_, !connection);

  // Only create a new connection if one was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> database_connection;
  if (!connection_created_) {
    database_connection = std::move(connection);
  }

  SafeConnectionWrapper wrapper(std::move(database_connection));
  if (!remote_) {
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  if (wrapper.connection_) {
    pending_remote =
        DatabaseImpl::CreateAndBind(std::move(wrapper.connection_));
  }
  remote_->OpenSuccess(std::move(pending_remote), metadata);
  complete_ = true;
}

void IndexedDBFactoryClient::OnDeleteSuccess(int64_t old_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!remote_) {
    return;
  }
  remote_->DeleteSuccess(old_version);
  complete_ = true;
}

void IndexedDBFactoryClient::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_.reset();
}

}  // namespace content
