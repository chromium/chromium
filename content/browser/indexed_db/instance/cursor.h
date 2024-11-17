// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_CURSOR_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/database.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace storage {
struct BucketLocator;
}  // namespace storage

namespace content::indexed_db {

enum class CursorType { kKeyAndValue = 0, kKeyOnly = 1 };

class Cursor : public blink::mojom::IDBCursor {
 public:
  // Creates a new self-owned instance and binds to `pending_remote`.
  static Cursor* CreateAndBind(
      std::unique_ptr<BackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      blink::mojom::IDBTaskType task_type,
      base::WeakPtr<Transaction> transaction,
      mojo::PendingAssociatedRemote<blink::mojom::IDBCursor>& pending_remote);

  ~Cursor() override;

  Cursor(const Cursor&) = delete;
  Cursor& operator=(const Cursor&) = delete;

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
    return (cursor_type_ == indexed_db::CursorType::kKeyOnly)
               ? nullptr
               : cursor_->value();
  }

  void Close();

 private:
  Cursor(std::unique_ptr<BackingStore::Cursor> cursor,
         indexed_db::CursorType cursor_type,
         blink::mojom::IDBTaskType task_type,
         base::WeakPtr<Transaction> transaction);

  Status ContinueOperation(std::unique_ptr<blink::IndexedDBKey> key,
                           std::unique_ptr<blink::IndexedDBKey> primary_key,
                           blink::mojom::IDBCursor::ContinueCallback callback,
                           Transaction* transaction);
  Status AdvanceOperation(uint32_t count,
                          blink::mojom::IDBCursor::AdvanceCallback callback,
                          Transaction* transaction);
  Status PrefetchIterationOperation(
      int number_to_fetch,
      blink::mojom::IDBCursor::PrefetchCallback callback,
      Transaction* transaction);

  const storage::BucketLocator bucket_locator_;
  blink::mojom::IDBTaskType task_type_;
  indexed_db::CursorType cursor_type_;

  // We rely on the transaction calling Close() to clear this.
  base::WeakPtr<Transaction> transaction_;

  // Must be destroyed before transaction_.
  std::unique_ptr<BackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  std::unique_ptr<BackingStore::Cursor> saved_cursor_;

  bool closed_ = false;

  mojo::AssociatedReceiver<blink::mojom::IDBCursor> receiver_{this};

  base::WeakPtrFactory<Cursor> ptr_factory_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_CURSOR_H_
