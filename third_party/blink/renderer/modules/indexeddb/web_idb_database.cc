// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"

#include <utility>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBDatabase::WebIDBDatabase(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  database_.Bind(std::move(pending_database), task_runner_);
}

WebIDBDatabase::~WebIDBDatabase() = default;

void WebIDBDatabase::RenameObjectStore(int64_t transaction_id,
                                       int64_t object_store_id,
                                       const String& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id, new_name);
}

void WebIDBDatabase::CreateTransaction(
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    const Vector<int64_t>& object_store_ids,
    mojom::blink::IDBTransactionMode mode,
    mojom::blink::IDBTransactionDurability durability) {
  database_->CreateTransaction(std::move(transaction_receiver), transaction_id,
                               object_store_ids, mode, durability);
}

void WebIDBDatabase::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabase::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IDBKeyRange* key_range,
    bool key_only,
    base::OnceCallback<void(mojom::blink::IDBDatabaseGetResultPtr)>
        result_callback) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_->Get(transaction_id, object_store_id, index_id,
                 std::move(key_range_ptr), key_only,
                 std::move(result_callback));
}

void WebIDBDatabase::GetAll(int64_t transaction_id,
                            int64_t object_store_id,
                            int64_t index_id,
                            const IDBKeyRange* key_range,
                            int64_t max_count,
                            bool key_only,
                            IDBRequest* request) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_->GetAll(transaction_id, object_store_id, index_id,
                    std::move(key_range_ptr), key_only, max_count,
                    WTF::BindOnce(&IDBRequest::OnGetAll,
                                  WrapWeakPersistent(request), key_only));
}

void WebIDBDatabase::SetIndexKeys(int64_t transaction_id,
                                  int64_t object_store_id,
                                  std::unique_ptr<IDBKey> primary_key,
                                  Vector<IDBIndexKeys> index_keys) {
  database_->SetIndexKeys(transaction_id, object_store_id,
                          std::move(primary_key), std::move(index_keys));
}

void WebIDBDatabase::SetIndexesReady(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const Vector<int64_t>& index_ids) {
  database_->SetIndexesReady(transaction_id, object_store_id,
                             std::move(index_ids));
}

void WebIDBDatabase::OpenCursor(int64_t object_store_id,
                                int64_t index_id,
                                const IDBKeyRange* key_range,
                                mojom::blink::IDBCursorDirection direction,
                                bool key_only,
                                mojom::blink::IDBTaskType task_type,
                                IDBRequest* request) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(request->transaction()->Id(),
                                                 nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_->OpenCursor(
      request->transaction()->Id(), object_store_id, index_id,
      std::move(key_range_ptr), direction, key_only, task_type,
      WTF::BindOnce(&IDBRequest::OnOpenCursor, WrapWeakPersistent(request)));
}

void WebIDBDatabase::Count(int64_t transaction_id,
                           int64_t object_store_id,
                           int64_t index_id,
                           const IDBKeyRange* key_range,
                           mojom::blink::IDBDatabase::CountCallback callback) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  database_->Count(transaction_id, object_store_id, index_id,
                   mojom::blink::IDBKeyRange::From(key_range),
                   std::move(callback));
}

void WebIDBDatabase::Delete(int64_t transaction_id,
                            int64_t object_store_id,
                            const IDBKey* primary_key,
                            base::OnceCallback<void(bool)> success_callback) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(IDBKeyRange::Create(primary_key));
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr), std::move(success_callback));
}

void WebIDBDatabase::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IDBKeyRange* key_range,
    base::OnceCallback<void(bool)> success_callback) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr), std::move(success_callback));
}

void WebIDBDatabase::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    mojom::blink::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback) {
  database_->GetKeyGeneratorCurrentNumber(transaction_id, object_store_id,
                                          std::move(callback));
}

void WebIDBDatabase::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    mojom::blink::IDBDatabase::ClearCallback success_callback) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);
  database_->Clear(transaction_id, object_store_id,
                   std::move(success_callback));
}

void WebIDBDatabase::CreateIndex(int64_t transaction_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 const String& name,
                                 const IDBKeyPath& key_path,
                                 bool unique,
                                 bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id, name,
                         key_path, unique, multi_entry);
}

void WebIDBDatabase::DeleteIndex(int64_t transaction_id,
                                 int64_t object_store_id,
                                 int64_t index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabase::RenameIndex(int64_t transaction_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 const String& new_name) {
  DCHECK(!new_name.IsNull());
  database_->RenameIndex(transaction_id, object_store_id, index_id, new_name);
}

void WebIDBDatabase::Abort(int64_t transaction_id) {
  database_->Abort(transaction_id);
}

void WebIDBDatabase::DidBecomeInactive() {
  database_->DidBecomeInactive();
}

}  // namespace blink
