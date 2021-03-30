// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"

#include <utility>

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

}  // namespace blink
