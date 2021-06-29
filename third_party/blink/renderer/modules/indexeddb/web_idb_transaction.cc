// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBTransaction::WebIDBTransaction(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int64_t transaction_id)
    : task_runner_(task_runner), transaction_id_(transaction_id) {}

WebIDBTransaction::~WebIDBTransaction() = default;

mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
WebIDBTransaction::CreateReceiver() {
  return transaction_.BindNewEndpointAndPassReceiver(task_runner_);
}

void WebIDBTransaction::CreateObjectStore(int64_t object_store_id,
                                          const String& name,
                                          const IDBKeyPath& key_path,
                                          bool auto_increment) {
  transaction_->CreateObjectStore(object_store_id, name, key_path,
                                  auto_increment);
}

void WebIDBTransaction::DeleteObjectStore(int64_t object_store_id) {
  transaction_->DeleteObjectStore(object_store_id);
}

void WebIDBTransaction::Put(int64_t object_store_id,
                            std::unique_ptr<IDBValue> value,
                            std::unique_ptr<IDBKey> primary_key,
                            mojom::blink::IDBPutMode put_mode,
                            std::unique_ptr<WebIDBCallbacks> callbacks,
                            Vector<IDBIndexKeys> index_keys) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, nullptr);

  size_t index_keys_size = 0;
  for (const auto& index_key : index_keys) {
    index_keys_size++;  // Account for index_key.first (int64_t).
    for (const auto& key : index_key.keys) {
      // Because all size estimates are based on RAM usage, it is impossible to
      // overflow index_keys_size.
      index_keys_size += key->SizeEstimate();
    }
  }

  size_t arg_size =
      value->DataSize() + primary_key->SizeEstimate() + index_keys_size;
  if (arg_size >= max_put_value_size_) {
    callbacks->Error(
        mojom::blink::IDBException::kUnknownError,
        String::Format("The serialized keys and/or value are too large"
                       " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
                       arg_size, max_put_value_size_));
    return;
  }

  callbacks->SetState(nullptr, transaction_id_);
  transaction_->Put(object_store_id, std::move(value), std::move(primary_key),
                    put_mode, std::move(index_keys),
                    WTF::Bind(&WebIDBTransaction::PutCallback,
                              WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBTransaction::PutCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBTransactionPutResultPtr result) {
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
}

void WebIDBTransaction::PutAll(int64_t object_store_id,
                               Vector<mojom::blink::IDBPutParamsPtr> puts,
                               std::unique_ptr<WebIDBCallbacks> callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, nullptr);
  base::CheckedNumeric<size_t> index_keys_size = 0;
  for (auto& put : puts) {
    index_keys_size++;
    for (const auto& index_key : put->index_keys) {
      for (const auto& key : index_key.keys) {
        index_keys_size += key->SizeEstimate();
      }
    }
  }

  base::CheckedNumeric<size_t> arg_size;
  for (auto& put : puts) {
    arg_size += put->value->DataSize();
    arg_size += put->key->SizeEstimate();
  }
  arg_size += index_keys_size.ValueOrDie();
  if (arg_size.ValueOrDie() >= max_put_value_size_) {
    callbacks->Error(
        mojom::blink::IDBException::kUnknownError,
        String::Format("The serialized keys and/or values are too large"
                       " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
                       base::checked_cast<size_t>(arg_size.ValueOrDie()),
                       max_put_value_size_));
    return;
  }

  callbacks->SetState(nullptr, transaction_id_);
  transaction_->PutAll(object_store_id, std::move(puts),
                       WTF::Bind(&WebIDBTransaction::PutAllCallback,
                                 WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBTransaction::PutAllCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBTransactionPutAllResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_keys()) {
    callbacks->SuccessKey(IDBKey::CreateArray(std::move(result->get_keys())));
    callbacks.reset();
    return;
  }
}

void WebIDBTransaction::Commit(int64_t num_errors_handled) {
  transaction_->Commit(num_errors_handled);
}

void WebIDBTransaction::FlushForTesting() {
  transaction_.FlushForTesting();
}

}  // namespace blink
