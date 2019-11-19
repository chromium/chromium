// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

#include "base/format_macros.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  database_.Bind(std::move(pending_database), task_runner_);
}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() = default;

void WebIDBDatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                           int64_t object_store_id,
                                           const String& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id, new_name);
}

void WebIDBDatabaseImpl::CreateTransaction(
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    const Vector<int64_t>& object_store_ids,
    mojom::IDBTransactionMode mode,
    mojom::IDBTransactionDurability durability) {
  database_->CreateTransaction(std::move(transaction_receiver), transaction_id,
                               object_store_ids, mode, durability);
}

void WebIDBDatabaseImpl::Close() {
  database_->Close();
}

void WebIDBDatabaseImpl::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabaseImpl::AddObserver(
    int64_t transaction_id,
    int32_t observer_id,
    bool include_transaction,
    bool no_records,
    bool values,
    std::bitset<blink::kIDBOperationTypeCount> operation_types) {
  static_assert(kIDBOperationTypeCount < sizeof(uint32_t) * CHAR_BIT,
                "IDBOperationTypeCount exceeds size of uint32_t");
  database_->AddObserver(transaction_id, observer_id, include_transaction,
                         no_records, values,
                         static_cast<uint32_t>(operation_types.to_ulong()));
}

void WebIDBDatabaseImpl::RemoveObservers(const Vector<int32_t>& observer_ids) {
  database_->RemoveObservers(observer_ids);
}

void WebIDBDatabaseImpl::Get(int64_t transaction_id,
                             int64_t object_store_id,
                             int64_t index_id,
                             const IDBKeyRange* key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks_ptr) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->Get(transaction_id, object_store_id, index_id,
                 std::move(key_range_ptr), key_only,
                 WTF::Bind(&WebIDBDatabaseImpl::GetCallback,
                           WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBDatabaseImpl::GetCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBDatabaseGetResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_empty()) {
    callbacks->Success();
    callbacks.reset();
    return;
  }

  if (result->is_key()) {
    callbacks->SuccessKey(std::move(result->get_key()));
    callbacks.reset();
    return;
  }

  if (result->is_value()) {
    callbacks->SuccessValue(std::move(result->get_value()));
    callbacks.reset();
    return;
  }
}

void WebIDBDatabaseImpl::GetAll(int64_t transaction_id,
                                int64_t object_store_id,
                                int64_t index_id,
                                const IDBKeyRange* key_range,
                                int64_t max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks_ptr) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->GetAll(transaction_id, object_store_id, index_id,
                    std::move(key_range_ptr), key_only, max_count,
                    WTF::Bind(&WebIDBDatabaseImpl::GetAllCallback,
                              WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBDatabaseImpl::GetAllCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBDatabaseGetAllResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_key()) {
    callbacks->SuccessKey(std::move(result->get_key()));
    callbacks.reset();
    return;
  }

  if (result->is_values()) {
    callbacks->SuccessArray(std::move(result->get_values()));
    callbacks.reset();
    return;
  }
}

void WebIDBDatabaseImpl::SetIndexKeys(int64_t transaction_id,
                                      int64_t object_store_id,
                                      std::unique_ptr<IDBKey> primary_key,
                                      Vector<IDBIndexKeys> index_keys) {
  database_->SetIndexKeys(transaction_id, object_store_id,
                          std::move(primary_key), std::move(index_keys));
}

void WebIDBDatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                         int64_t object_store_id,
                                         const Vector<int64_t>& index_ids) {
  database_->SetIndexesReady(transaction_id, object_store_id,
                             std::move(index_ids));
}

void WebIDBDatabaseImpl::OpenCursor(int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const IDBKeyRange* key_range,
                                    mojom::IDBCursorDirection direction,
                                    bool key_only,
                                    mojom::IDBTaskType task_type,
                                    WebIDBCallbacks* callbacks_ptr) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->OpenCursor(transaction_id, object_store_id, index_id,
                        std::move(key_range_ptr), direction, key_only,
                        task_type,
                        WTF::Bind(&WebIDBDatabaseImpl::OpenCursorCallback,
                                  WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBDatabaseImpl::OpenCursorCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBDatabaseOpenCursorResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_empty()) {
    CHECK(result->get_empty());  // Only true values are allowed.
    callbacks->SuccessValue(nullptr);
    callbacks.reset();
    return;
  }

  CHECK(result->is_value());
  callbacks->SuccessCursor(std::move(result->get_value()->cursor),
                           std::move(result->get_value()->key),
                           std::move(result->get_value()->primary_key),
                           std::move(result->get_value()->value));
  callbacks.reset();
}

void WebIDBDatabaseImpl::Count(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const IDBKeyRange* key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->Count(transaction_id, object_store_id, index_id,
                   std::move(key_range_ptr),
                   GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Delete(int64_t transaction_id,
                                int64_t object_store_id,
                                const IDBKey* primary_key,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(IDBKeyRange::Create(primary_key));
  callbacks->SetState(nullptr, transaction_id);
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr),
                         GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::DeleteRange(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const IDBKeyRange* key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr),
                         GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    WebIDBCallbacks* callbacks) {
  callbacks->SetState(nullptr, transaction_id);
  database_->GetKeyGeneratorCurrentNumber(
      transaction_id, object_store_id,
      GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Clear(int64_t transaction_id,
                               int64_t object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  callbacks->SetState(nullptr, transaction_id);
  database_->Clear(transaction_id, object_store_id,
                   GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::CreateIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id,
                                     const String& name,
                                     const IDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id, name,
                         key_path, unique, multi_entry);
}

void WebIDBDatabaseImpl::DeleteIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabaseImpl::RenameIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id,
                                     const String& new_name) {
  DCHECK(!new_name.IsNull());
  database_->RenameIndex(transaction_id, object_store_id, index_id, new_name);
}

void WebIDBDatabaseImpl::Abort(int64_t transaction_id) {
  database_->Abort(transaction_id);
}

mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
WebIDBDatabaseImpl::GetCallbacksProxy(
    std::unique_ptr<WebIDBCallbacks> callbacks_impl) {
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> pending_callbacks;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(callbacks_impl),
      pending_callbacks.InitWithNewEndpointAndPassReceiver(), task_runner_);
  return pending_callbacks;
}

}  // namespace blink
