// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database_callbacks.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content::indexed_db {

DatabaseCallbacks::DatabaseCallbacks(
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        callbacks_remote) {
  if (!callbacks_remote.is_valid()) {
    return;
  }
  callbacks_.Bind(std::move(callbacks_remote));
}

DatabaseCallbacks::~DatabaseCallbacks() {}

void DatabaseCallbacks::OnForcedClose() {
  if (complete_) {
    return;
  }

  if (callbacks_) {
    callbacks_->ForcedClose();
  }
  complete_ = true;
}

void DatabaseCallbacks::OnVersionChange(int64_t old_version,
                                        int64_t new_version) {
  if (complete_) {
    return;
  }

  if (callbacks_) {
    callbacks_->VersionChange(old_version, new_version);
  }
}

void DatabaseCallbacks::OnAbort(const Transaction& transaction,
                                const DatabaseError& error) {
  if (complete_) {
    return;
  }

  if (callbacks_) {
    callbacks_->Abort(transaction.id(), error.code(), error.message());
  }
}

void DatabaseCallbacks::OnComplete(const Transaction& transaction) {
  if (complete_) {
    return;
  }

  if (callbacks_) {
    callbacks_->Complete(transaction.id());
  }
}

}  // namespace content::indexed_db
