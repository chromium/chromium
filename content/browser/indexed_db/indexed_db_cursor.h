// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {

class CONTENT_EXPORT IndexedDBCursor {
 public:
  IndexedDBCursor(std::unique_ptr<IndexedDBBackingStore::Cursor> cursor,
                  indexed_db::CursorType cursor_type,
                  blink::mojom::IDBTaskType task_type,
                  base::WeakPtr<IndexedDBTransaction> transaction);
  ~IndexedDBCursor();

  void Advance(uint32_t count,
               base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
               blink::mojom::IDBCursor::AdvanceCallback callback);
  void Continue(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                std::unique_ptr<blink::IndexedDBKey> key,
                std::unique_ptr<blink::IndexedDBKey> primary_key,
                blink::mojom::IDBCursor::CursorContinueCallback callback);
  void PrefetchContinue(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                        int number_to_fetch,
                        blink::mojom::IDBCursor::PrefetchCallback callback);
  leveldb::Status PrefetchReset(int used_prefetches, int unused_prefetches);

  void OnRemoveBinding(base::OnceClosure remove_binding_cb);

  const blink::IndexedDBKey& key() const { return cursor_->key(); }
  const blink::IndexedDBKey& primary_key() const {
    return cursor_->primary_key();
  }
  IndexedDBValue* Value() const {
    return (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) ? NULL
                                                         : cursor_->value();
  }

  // RemoveBinding() removes the mojo cursor binding, which owns
  // |IndexedDBCursor|, so calls to this function will delete |this|.
  void RemoveBinding();
  void Close();

  leveldb::Status CursorContinueOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      std::unique_ptr<blink::IndexedDBKey> key,
      std::unique_ptr<blink::IndexedDBKey> primary_key,
      blink::mojom::IDBCursor::CursorContinueCallback callback,
      IndexedDBTransaction* transaction);
  leveldb::Status CursorAdvanceOperation(
      uint32_t count,
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      blink::mojom::IDBCursor::AdvanceCallback callback,
      IndexedDBTransaction* transaction);
  leveldb::Status CursorPrefetchIterationOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      int number_to_fetch,
      blink::mojom::IDBCursor::PrefetchCallback callback,
      IndexedDBTransaction* transaction);

 private:
  blink::mojom::IDBTaskType task_type_;
  indexed_db::CursorType cursor_type_;

  // We rely on the transaction calling Close() to clear this.
  base::WeakPtr<IndexedDBTransaction> transaction_;

  // Must be destroyed before transaction_.
  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  std::unique_ptr<IndexedDBBackingStore::Cursor> saved_cursor_;

  base::OnceClosure remove_binding_cb_;

  bool closed_;

  base::WeakPtrFactory<IndexedDBCursor> ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCursor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
