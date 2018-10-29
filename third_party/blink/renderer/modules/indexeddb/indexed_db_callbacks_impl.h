// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"

using blink::IndexedDBKey;

namespace blink {

class WebIDBCallbacks;
class WebIDBCursorImpl;
class WebIDBValue;

// Implements the child-process end of the pipe used to deliver callbacks.
// |callback_runner_| is used to post tasks back to the thread which owns the
// blink::WebIDBCallbacks.
class IndexedDBCallbacksImpl : public mojom::blink::IDBCallbacks {
 public:
  // |kNoTransaction| is used as the default transaction ID when instantiating
  // an IndexedDBCallbacksImpl instance.  See web_idb_factory_impl.cc for those
  // cases.
  enum : int64_t { kNoTransaction = -1 };

  static WebIDBValue ConvertValue(const mojom::blink::IDBValuePtr& value);

  IndexedDBCallbacksImpl(std::unique_ptr<WebIDBCallbacks> callbacks,
                         int64_t transaction_id,
                         const base::WeakPtr<WebIDBCursorImpl>& cursor);
  ~IndexedDBCallbacksImpl() override;

  // mojom::blink::IDBCallbacks implementation:
  void Error(int32_t code, const String& message) override;
  void SuccessNamesAndVersionsList(
      Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) override;
  void SuccessStringList(const Vector<String>& value) override;
  void Blocked(int64_t existing_version) override;
  void UpgradeNeeded(mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
                     int64_t old_version,
                     WebIDBDataLoss data_loss,
                     const String& data_loss_message,
                     const WebIDBMetadata& metadata) override;
  void SuccessDatabase(mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
                       const WebIDBMetadata& metadata) override;
  void SuccessCursor(mojom::blink::IDBCursorAssociatedPtrInfo cursor,
                     WebIDBKey key,
                     WebIDBKey primary_key,
                     mojom::blink::IDBValuePtr value) override;
  void SuccessValue(mojom::blink::IDBReturnValuePtr value) override;
  void SuccessCursorContinue(WebIDBKey key,
                             WebIDBKey primary_key,
                             mojom::blink::IDBValuePtr value) override;
  void SuccessCursorPrefetch(Vector<WebIDBKey> keys,
                             Vector<WebIDBKey> primary_keys,
                             Vector<mojom::blink::IDBValuePtr> values) override;
  void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr> values) override;
  void SuccessKey(WebIDBKey key) override;
  void SuccessInteger(int64_t value) override;
  void Success() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
  std::unique_ptr<WebIDBCallbacks> callbacks_;
  base::WeakPtr<WebIDBCursorImpl> cursor_;
  int64_t transaction_id_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacksImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_
