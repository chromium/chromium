// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/shared_idb_database_connection.h"

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

SharedIDBDatabaseConnection::SharedIDBDatabaseConnection(
    ExecutionContext* context,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    const IDBDatabaseMetadata& metadata,
    int64_t open_request_transaction_id)
    : database_remote_(context),
      callbacks_receiver_(this, context),
      metadata_(metadata),
      open_request_transaction_id_(open_request_transaction_id) {
  if (base::FeatureList::IsEnabled(
          features::kIDBDatabaseExternalMemoryAccounting)) {
    if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent()) {
      external_memory_accounter_.Increase(
          isolate, IDBDatabase::kExternalMemorySize.InBytes());
    }
  }

  database_remote_.Bind(std::move(pending_database),
                        context->GetTaskRunner(TaskType::kDatabaseAccess));
  database_remote_.set_disconnect_handler(blink::BindOnce(
      &SharedIDBDatabaseConnection::OnDisconnect, WrapWeakPersistent(this)));
  callbacks_receiver_.Bind(std::move(callbacks_receiver),
                           context->GetTaskRunner(TaskType::kDatabaseAccess));
}

SharedIDBDatabaseConnection::~SharedIDBDatabaseConnection() {
  ClearExternalMemory();
}

void SharedIDBDatabaseConnection::Trace(Visitor* visitor) const {
  visitor->Trace(database_remote_);
  visitor->Trace(callbacks_receiver_);
  visitor->Trace(frontends_);
  visitor->Trace(transaction_map_);
}

void SharedIDBDatabaseConnection::ClearExternalMemory() {
  if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent()) {
    external_memory_accounter_.Clear(isolate);
  }
}

void SharedIDBDatabaseConnection::OnDisconnect() {
  if (disconnect_callback_) {
    std::move(disconnect_callback_).Run();
  }
}

void SharedIDBDatabaseConnection::RegisterFrontend(IDBDatabase* frontend) {
  DCHECK(!frontends_.Contains(frontend));
  frontends_.insert(frontend);
}

void SharedIDBDatabaseConnection::UnregisterFrontend(IDBDatabase* frontend) {
  // If this is called from IDBDatabase::Dispose() during GC, the weak pointer
  // in `frontends_` might have already been cleared and removed by cppgc
  // weak processing, so the frontend may not be present in the set.
  frontends_.erase(frontend);
  MaybeClose();
}

void SharedIDBDatabaseConnection::RegisterTransaction(int64_t transaction_id,
                                                      IDBDatabase* frontend) {
  DCHECK(frontend);
  DCHECK(!transaction_map_.Contains(transaction_id));
  transaction_map_.insert(transaction_id, frontend);
}

void SharedIDBDatabaseConnection::UnregisterTransaction(
    int64_t transaction_id) {
  DCHECK(transaction_map_.Contains(transaction_id));
  transaction_map_.erase(transaction_id);
}

void SharedIDBDatabaseConnection::DecrementPendingSharingCount() {
  CHECK_GT(pending_sharing_count_, 0);
  pending_sharing_count_--;
  MaybeClose();
}

void SharedIDBDatabaseConnection::MaybeClose() {
  if (frontends_.empty() && pending_sharing_count_ == 0) {
    if (database_remote_.is_bound()) {
      ClearExternalMemory();
      database_remote_.reset();
    }
    if (callbacks_receiver_.is_bound()) {
      callbacks_receiver_.reset();
    }
    if (disconnect_callback_) {
      std::move(disconnect_callback_).Run();
    }
  }
}

void SharedIDBDatabaseConnection::ForcedClose() {
  HeapVector<Member<IDBDatabase>> frontends(frontends_);
  for (IDBDatabase* frontend : frontends) {
    if (frontend) {
      frontend->ForcedClose();
    }
  }
}

void SharedIDBDatabaseConnection::VersionChange(int64_t old_version,
                                                int64_t new_version) {
  HeapVector<Member<IDBDatabase>> frontends(frontends_);
  for (IDBDatabase* frontend : frontends) {
    if (frontend) {
      frontend->VersionChange(old_version, new_version);
    }
  }
}

void SharedIDBDatabaseConnection::Abort(int64_t transaction_id,
                                        mojom::blink::IDBException code,
                                        const String& message) {
  auto it = transaction_map_.find(transaction_id);
  if (it != transaction_map_.end()) {
    IDBDatabase* frontend = it->value;
    frontend->Abort(transaction_id, code, message);
  }
}

void SharedIDBDatabaseConnection::Complete(int64_t transaction_id) {
  auto it = transaction_map_.find(transaction_id);
  if (it != transaction_map_.end()) {
    IDBDatabase* frontend = it->value;
    frontend->Complete(transaction_id);
  }
}

}  // namespace blink
