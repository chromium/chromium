// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {
class WebIDBCursor;

// Handle the indexed db related communication for this context thread - the
// main thread and each worker thread have their own copies.
class MODULES_EXPORT IndexedDBDispatcher {
  DISALLOW_NEW();

 public:
  static void RegisterCursor(WebIDBCursor* cursor);
  static void UnregisterCursor(WebIDBCursor* cursor);

  // Reset cursor prefetch caches for all cursors except |except_cursor|.
  // In most callers, |except_cursor| is passed as nullptr, causing all cursors
  // to have their prefetch cache to be reset.  In 2 WebIDBCursor callers,
  // specifically from |Advance| and |CursorContinue|, these want to reset all
  // cursor prefetch caches except the cursor the calls are running from.  They
  // get that behavior by passing |this| to |ResetCursorPrefetchCaches| which
  // skips calling |ResetPrefetchCache| on them.
  static void ResetCursorPrefetchCaches(int64_t transaction_id,
                                        WebIDBCursor* except_cursor);

 private:
  friend class WTF::ThreadSpecific<IndexedDBDispatcher>;

  static IndexedDBDispatcher* GetInstanceForCurrentThread();

  IndexedDBDispatcher();

  WTF::HashSet<WebIDBCursor*> cursors_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_DISPATCHER_H_
