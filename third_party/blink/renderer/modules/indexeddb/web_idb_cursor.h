// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_

#include <stdint.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory_client.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class IDBRequest;

class MODULES_EXPORT WebIDBCursor final {
 public:
  WebIDBCursor(mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor,
               int64_t transaction_id,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebIDBCursor();

  // Disallow copy and assign.
  WebIDBCursor(const WebIDBCursor&) = delete;
  WebIDBCursor& operator=(const WebIDBCursor&) = delete;

  // Used to implement IDBCursor.advance().
  void Advance(uint32_t count, IDBRequest* request);

  // Used to implement IDBCursor.continue() and IDBCursor.continuePrimaryKey().
  //
  // The key and primary key are null when they are not supplied by the
  // application. When both arguments are null, the cursor advances by one
  // entry.
  //
  // The keys pointed to by IDBKey* are only guaranteed to be alive for
  // the duration of the call.
  void CursorContinue(const IDBKey* key,
                      const IDBKey* primary_key,
                      IDBRequest* request);

  void PrefetchCallback(IDBRequest* request,
                        mojom::blink::IDBCursorResultPtr result);

  // Called after a cursor request's success handler is executed.
  //
  // This is only used by the cursor prefetching logic, and does not result in
  // an IPC.
  void PostSuccessHandlerCallback();

  void SetPrefetchData(Vector<std::unique_ptr<IDBKey>> keys,
                       Vector<std::unique_ptr<IDBKey>> primary_keys,
                       Vector<std::unique_ptr<IDBValue>> values);

  void CachedAdvance(uint32_t count, IDBRequest* request);
  void CachedContinue(IDBRequest* request);

  void ResetPrefetchCache();

  int64_t transaction_id() const { return transaction_id_; }

 private:
  void AdvanceCallback(IDBRequest* request,
                       mojom::blink::IDBCursorResultPtr result);

  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorTest, AdvancePrefetchTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorTest, PrefetchReset);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorTest, PrefetchTest);

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
  int continue_count_ = 0;

  // Number of items used from the last prefetch.
  int used_prefetches_ = 0;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_ = 0;

  // Number of items to request in next prefetch.
  int prefetch_amount_ = kMinPrefetchAmount;

  base::WeakPtrFactory<WebIDBCursor> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_
