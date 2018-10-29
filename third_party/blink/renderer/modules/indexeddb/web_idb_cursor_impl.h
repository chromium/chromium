// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class IndexedDBCallbacksImpl;

class MODULES_EXPORT WebIDBCursorImpl : public WebIDBCursor {
 public:
  WebIDBCursorImpl(mojom::blink::IDBCursorAssociatedPtrInfo cursor,
                   int64_t transaction_id);
  ~WebIDBCursorImpl() override;

  void Advance(unsigned long count, WebIDBCallbacks* callback) override;
  void CursorContinue(WebIDBKeyView key,
                      WebIDBKeyView primary_key,
                      WebIDBCallbacks* callback) override;
  void PostSuccessHandlerCallback() override;

  void SetPrefetchData(Vector<WebIDBKey> keys,
                       Vector<WebIDBKey> primary_keys,
                       Vector<WebIDBValue> values);

  void CachedAdvance(unsigned long count, WebIDBCallbacks* callbacks);
  void CachedContinue(WebIDBCallbacks* callbacks);

  // This method is virtual so it can be overridden in unit tests.
  virtual void ResetPrefetchCache();

  int64_t transaction_id() const { return transaction_id_; }

 private:
  mojom::blink::IDBCallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);

  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, AdvancePrefetchTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchReset);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchTest);

  static constexpr int kPrefetchContinueThreshold = 2;
  static constexpr int kMinPrefetchAmount = 5;
  static constexpr int kMaxPrefetchAmount = 100;

  int64_t transaction_id_;

  mojom::blink::IDBCursorAssociatedPtr cursor_;

  // Prefetch cache. Keys and values are stored in reverse order so that a
  // cache'd continue can pop a value off of the back and prevent new memory
  // allocations.
  Vector<WebIDBKey> prefetch_keys_;
  Vector<WebIDBKey> prefetch_primary_keys_;
  Vector<WebIDBValue> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  base::WeakPtrFactory<WebIDBCursorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_IMPL_H_
