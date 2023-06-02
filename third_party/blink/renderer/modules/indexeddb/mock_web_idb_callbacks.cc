// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_callbacks.h"

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

namespace blink {

MockWebIDBCallbacks::MockWebIDBCallbacks() {}

MockWebIDBCallbacks::~MockWebIDBCallbacks() {}

void MockWebIDBCallbacks::SetState(base::WeakPtr<WebIDBCursor> cursor,
                                   int64_t transaction_id) {}

void MockWebIDBCallbacks::SuccessCursorContinue(
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    absl::optional<std::unique_ptr<IDBValue>> value) {
  DoSuccessCursorContinue(key, primary_key, value);
}

void MockWebIDBCallbacks::SuccessKey(std::unique_ptr<IDBKey> key) {
  DoSuccessKey(key);
}

void MockWebIDBCallbacks::SuccessValue(mojom::blink::IDBReturnValuePtr value) {
  DoSuccessValue(value);
}

void MockWebIDBCallbacks::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  DoSuccessArray(values);
}

void MockWebIDBCallbacks::SuccessArrayArray(
    Vector<Vector<mojom::blink::IDBReturnValuePtr>> all_values) {
  DoSuccessArrayArray(all_values);
}

}  // namespace blink
