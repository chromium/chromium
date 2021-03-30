// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DATABASE_CALLBACKS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DATABASE_CALLBACKS_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"

namespace blink {
class WebIDBDatabaseCallbacks;

class IndexedDBDatabaseCallbacksImpl
    : public mojom::blink::IDBDatabaseCallbacks {
 public:
  explicit IndexedDBDatabaseCallbacksImpl(
      std::unique_ptr<WebIDBDatabaseCallbacks> callbacks);
  ~IndexedDBDatabaseCallbacksImpl() override;

  // Disallow copy and assign.
  IndexedDBDatabaseCallbacksImpl(const IndexedDBDatabaseCallbacksImpl&) =
      delete;
  IndexedDBDatabaseCallbacksImpl& operator=(
      const IndexedDBDatabaseCallbacksImpl&) = delete;

  // mojom::blink::IDBDatabaseCallbacks implementation
  void ForcedClose() override;
  void VersionChange(int64_t old_version, int64_t new_version) override;
  void Abort(int64_t transaction_id,
             mojom::blink::IDBException code,
             const WTF::String& message) override;
  void Complete(int64_t transaction_id) override;

 private:
  std::unique_ptr<WebIDBDatabaseCallbacks> callbacks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DATABASE_CALLBACKS_IMPL_H_
