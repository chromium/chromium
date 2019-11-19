// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"

#include <utility>

#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_observation.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

IndexedDBDatabaseCallbacksImpl::IndexedDBDatabaseCallbacksImpl(
    std::unique_ptr<WebIDBDatabaseCallbacks> callbacks)
    : callbacks_(std::move(callbacks)) {}

IndexedDBDatabaseCallbacksImpl::~IndexedDBDatabaseCallbacksImpl() = default;

void IndexedDBDatabaseCallbacksImpl::ForcedClose() {
  callbacks_->OnForcedClose();
}

void IndexedDBDatabaseCallbacksImpl::VersionChange(int64_t old_version,
                                                   int64_t new_version) {
  callbacks_->OnVersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacksImpl::Abort(int64_t transaction_id,
                                           mojom::blink::IDBException code,
                                           const String& message) {
  callbacks_->OnAbort(
      transaction_id,
      IDBDatabaseError(static_cast<DOMExceptionCode>(code), message));
}

void IndexedDBDatabaseCallbacksImpl::Complete(int64_t transaction_id) {
  callbacks_->OnComplete(transaction_id);
}

void IndexedDBDatabaseCallbacksImpl::Changes(
    mojom::blink::IDBObserverChangesPtr changes) {
  Vector<Persistent<IDBObservation>> observations;
  observations.ReserveInitialCapacity(changes->observations.size());
  for (const auto& observation : changes->observations) {
    IDBKeyRange* key_range = observation->key_range.To<IDBKeyRange*>();
    std::unique_ptr<IDBValue> value;
    if (observation->value.has_value())
      value = std::move(observation->value.value());
    if (!value || value->Data()->IsEmpty()) {
      value = std::make_unique<IDBValue>(scoped_refptr<SharedBuffer>(),
                                         Vector<WebBlobInfo>());
    }
    observations.emplace_back(MakeGarbageCollected<IDBObservation>(
        observation->object_store_id, observation->type, key_range,
        std::move(value)));
  }

  HashMap<int32_t, Vector<int32_t>> observation_index_map;
  for (const auto& observation_pair : changes->observation_index_map) {
    observation_index_map.insert(observation_pair.key,
                                 Vector<int32_t>(observation_pair.value));
  }

  HashMap<int32_t, std::pair<int64_t, Vector<int64_t>>> observer_transactions;
  for (const auto& transaction_pair : changes->transaction_map) {
    // Moving an int64_t is rather silly. Sadly, std::make_pair's overloads
    // accept either two rvalue arguments, or none.
    observer_transactions.insert(transaction_pair.key,
                                 std::make_pair<int64_t, Vector<int64_t>>(
                                     std::move(transaction_pair.value->id),
                                     std::move(transaction_pair.value->scope)));
  }

  callbacks_->OnChanges(observation_index_map, std::move(observations),
                        observer_transactions);
}

}  // namespace blink
