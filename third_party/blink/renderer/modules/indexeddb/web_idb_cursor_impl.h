// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT WebIDBCursorImpl : public WebIDBCursor {
 public:
  WebIDBCursorImpl(
      mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor,
      int64_t transaction_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebIDBCursorImpl() override;

  void Advance(uint32_t count, WebIDBCallbacks* callback) override;

  void CursorContinue(const IDBKey* key,
                      const IDBKey* primary_key,
                      WebIDBCallbacks* callback) override;
  void CursorContinueCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                              mojom::blink::IDBCursorResultPtr result);
  void PrefetchCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                        mojom::blink::IDBCursorResultPtr result);

  void PostSuccessHandlerCallback() override;

  void SetPrefetchData(Vector<std::unique_ptr<IDBKey>> keys,
                       Vector<std::unique_ptr<IDBKey>> primary_keys,
                       Vector<std::unique_ptr<IDBValue>> values);

  void CachedAdvance(uint32_t count, WebIDBCallbacks* callbacks);
  void CachedContinue(WebIDBCallbacks* callbacks);

  // This method is virtual so it can be overridden in unit tests.
  virtual void ResetPrefetchCache();

  int64_t transaction_id() const { return transaction_id_; }

 private:
  void AdvanceCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                       mojom::blink::IDBCursorResultPtr result);
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> GetCallbacksProxy(
      std::unique_ptr<WebIDBCallbacks> callbacks);

  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, AdvancePrefetchTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchReset);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchTest);

  static constexpr int kPrefetchContinueThreshold = 2;
  static constexpr int kMinPrefetchAmount = 5;
  static constexpr int kMaxPrefetchAmount = 100;

  int64_t transaction_id_;

  mojo::AssociatedRemote<mojom::blink::IDBCursor> cursor_;

  // Prefetch cache. Keys and values are stored in reverse order so that a
  // cache'd continue can pop a value off of the back and prevent new memory
  // allocations.
  Vector<std::unique_ptr<IDBKey>> prefetch_keys_;
  Vector<std::unique_ptr<IDBKey>> prefetch_primary_keys_;
  Vector<std::unique_ptr<IDBValue>> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<WebIDBCursorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_
