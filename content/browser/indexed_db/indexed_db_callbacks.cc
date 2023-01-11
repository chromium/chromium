// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/cursor_impl.h"
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
    if (cursor_)
      idb_runner_->DeleteSoon(FROM_HERE, cursor_.release());
  }
  SafeCursorWrapper(SafeCursorWrapper&& other) = default;

  std::unique_ptr<IndexedDBCursor> cursor_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;
};

}  // namespace

IndexedDBCallbacks::IndexedDBCallbacks(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    const absl::optional<storage::BucketInfo>& bucket,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : data_loss_(blink::mojom::IDBDataLoss::None),
      dispatcher_host_(std::move(dispatcher_host)),
      bucket_info_(bucket),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_callbacks.is_valid()) {
    callbacks_.Bind(std::move(pending_callbacks));
    // |callbacks_| is owned by |this|, so if |this| is destroyed, then
    // |callbacks_| will also be destroyed.  While |callbacks_| is otherwise
    // alive, |this| will always be valid.
    callbacks_.set_disconnect_handler(base::BindOnce(
        &IndexedDBCallbacks::OnConnectionError, base::Unretained(this)));
  }
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Error(error.code(), error.message());
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(
    std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessNamesAndVersionsList(std::move(names_and_versions));
  complete_ = true;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;

  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  if (callbacks_)
    callbacks_->Blocked(existing_version);
}

void IndexedDBCallbacks::OnUpgradeNeeded(
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
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  auto database = std::make_unique<DatabaseImpl>(
      std::move(wrapper.connection_), *bucket_info_, dispatcher_host_.get(),
      idb_runner_);

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  dispatcher_host_->AddDatabaseBinding(
      std::move(database), pending_remote.InitWithNewEndpointAndPassReceiver());
  callbacks_->UpgradeNeeded(std::move(pending_remote), old_version,
                            data_loss_info.status, data_loss_info.message,
                            metadata);
}

void IndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(connection_created_, !connection);

  scoped_refptr<IndexedDBCallbacks> self(this);

  // Only create a new connection if one was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> database_connection;
  if (!connection_created_)
    database_connection = std::move(connection);

  SafeConnectionWrapper wrapper(std::move(database_connection));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  if (wrapper.connection_) {
    auto database = std::make_unique<DatabaseImpl>(
        std::move(wrapper.connection_), *bucket_info_, dispatcher_host_.get(),
        idb_runner_);
    dispatcher_host_->AddDatabaseBinding(
        std::move(database),
        pending_remote.InitWithNewEndpointAndPassReceiver());
  }
  callbacks_->SuccessDatabase(std::move(pending_remote), metadata);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessInteger(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Success();
  complete_ = true;
}

void IndexedDBCallbacks::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.reset();
  dispatcher_host_ = nullptr;
}

}  // namespace content
