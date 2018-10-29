// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"

#include <unordered_map>
#include <utility>

#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_observation.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"

using blink::WebVector;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBObservation;

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
                                           int32_t code,
                                           const String& message) {
  callbacks_->OnAbort(transaction_id,
                      blink::WebIDBDatabaseError(code, message));
}

void IndexedDBDatabaseCallbacksImpl::Complete(int64_t transaction_id) {
  callbacks_->OnComplete(transaction_id);
}

void IndexedDBDatabaseCallbacksImpl::Changes(
    mojom::blink::IDBObserverChangesPtr changes) {
  WebVector<WebIDBObservation> web_observations;
  web_observations.reserve(changes->observations.size());
  for (const auto& observation : changes->observations) {
    web_observations.emplace_back(
        observation->object_store_id, observation->type, observation->key_range,
        IndexedDBCallbacksImpl::ConvertValue(observation->value));
  }

  std::unordered_map<int32_t, WebVector<int32_t>> observation_index_map;
  for (const auto& observation_pair : changes->observation_index_map) {
    observation_index_map[observation_pair.key] =
        WebVector<int32_t>(observation_pair.value);
  }

  std::unordered_map<int32_t, std::pair<int64_t, WebVector<int64_t>>>
      observer_transactions;
  for (const auto& transaction_pair : changes->transaction_map) {
    // Moving an int64_t is rather silly. Sadly, std::make_pair's overloads
    // accept either two rvalue arguments, or none.
    observer_transactions[transaction_pair.key] =
        std::make_pair<int64_t, Vector<int64_t>>(
            std::move(transaction_pair.value->id),
            std::move(transaction_pair.value->scope));
  }

  callbacks_->OnChanges(observation_index_map, std::move(web_observations),
                        observer_transactions);
}

}  // namespace blink
