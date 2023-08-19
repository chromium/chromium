// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace storage {
struct BucketLocator;
}  // namespace storage

namespace content {

class IndexedDBCursor : public blink::mojom::IDBCursor {
 public:
  // Creates a new self-owned instance and binds to `pending_remote`.
  static IndexedDBCursor* CreateAndBind(
      std::unique_ptr<IndexedDBBackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      blink::mojom::IDBTaskType task_type,
      IndexedDBDispatcherHost& dispatcher_host,
      base::WeakPtr<IndexedDBTransaction> transaction,
      mojo::PendingAssociatedRemote<blink::mojom::IDBCursor>& pending_remote);

  ~IndexedDBCursor() override;

  IndexedDBCursor(const IndexedDBCursor&) = delete;
  IndexedDBCursor& operator=(const IndexedDBCursor&) = delete;

  // blink::mojom::IDBCursor implementation
  void Advance(uint32_t count,
               blink::mojom::IDBCursor::AdvanceCallback callback) override;
  void Continue(const blink::IndexedDBKey& key,
                const blink::IndexedDBKey& primary_key,
                blink::mojom::IDBCursor::ContinueCallback callback) override;
  void Prefetch(int32_t count,
                blink::mojom::IDBCursor::PrefetchCallback callback) override;
  void PrefetchReset(int32_t used_prefetches) override;

  const blink::IndexedDBKey& key() const { return cursor_->key(); }
  const blink::IndexedDBKey& primary_key() const {
    return cursor_->primary_key();
  }
  IndexedDBValue* Value() const {
    return (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) ? nullptr
                                                         : cursor_->value();
  }

  void Close();

 private:
  IndexedDBCursor(std::unique_ptr<IndexedDBBackingStore::Cursor> cursor,
                  indexed_db::CursorType cursor_type,
                  blink::mojom::IDBTaskType task_type,
                  IndexedDBDispatcherHost& dispatcher_host,
                  base::WeakPtr<IndexedDBTransaction> transaction);

  leveldb::Status ContinueOperation(
      std::unique_ptr<blink::IndexedDBKey> key,
      std::unique_ptr<blink::IndexedDBKey> primary_key,
      blink::mojom::IDBCursor::ContinueCallback callback,
      IndexedDBTransaction* transaction);
  leveldb::Status AdvanceOperation(
      uint32_t count,
      blink::mojom::IDBCursor::AdvanceCallback callback,
      IndexedDBTransaction* transaction);
  leveldb::Status PrefetchIterationOperation(
      int number_to_fetch,
      blink::mojom::IDBCursor::PrefetchCallback callback,
      IndexedDBTransaction* transaction);

  const storage::BucketLocator bucket_locator_;
  blink::mojom::IDBTaskType task_type_;
  indexed_db::CursorType cursor_type_;

  // We rely on the transaction calling Close() to clear this.
  base::WeakPtr<IndexedDBTransaction> transaction_;

  raw_ptr<IndexedDBDispatcherHost> dispatcher_host_;

  // Must be destroyed before transaction_.
  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  std::unique_ptr<IndexedDBBackingStore::Cursor> saved_cursor_;

  bool closed_ = false;

  mojo::AssociatedReceiver<blink::mojom::IDBCursor> receiver_{this};

  base::WeakPtrFactory<IndexedDBCursor> ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
