// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {
class WebIDBCursorImpl;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class MODULES_EXPORT IndexedDBDispatcher {
  DISALLOW_NEW();

 public:
  static void RegisterCursor(WebIDBCursorImpl* cursor);
  static void UnregisterCursor(WebIDBCursorImpl* cursor);

  // Reset cursor prefetch caches for all cursors except |except_cursor|.
  // In most callers, |except_cursor| is passed as nullptr, causing all cursors
  // to have their prefetch cache to be reset.  In 2 WebIDBCursorImpl callers,
  // specifically from |Advance| and |CursorContinue|, these want to reset all
  // cursor prefetch caches except the cursor the calls are running from.  They
  // get that behavior by passing |this| to |ResetCursorPrefetchCaches| which
  // skips calling |ResetPrefetchCache| on them.
  static void ResetCursorPrefetchCaches(int64_t transaction_id,
                                        WebIDBCursorImpl* except_cursor);

 private:
  friend class WTF::ThreadSpecific<IndexedDBDispatcher>;

  static IndexedDBDispatcher* GetInstanceForCurrentThread();

  IndexedDBDispatcher();

  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);

  WTF::HashSet<WebIDBCursorImpl*> cursors_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_
