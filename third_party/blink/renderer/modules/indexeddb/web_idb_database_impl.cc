// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"

namespace blink {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(
    mojom::blink::IDBDatabaseAssociatedPtrInfo database_info)
    : database_(std::move(database_info)) {}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() = default;

void WebIDBDatabaseImpl::CreateObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const String& name,
                                           const WebIDBKeyPath& key_path,
                                           bool auto_increment) {
  database_->CreateObjectStore(transaction_id, object_store_id, name, key_path,
                               auto_increment);
}

void WebIDBDatabaseImpl::DeleteObjectStore(long long transaction_id,
                                           long long object_store_id) {
  database_->DeleteObjectStore(transaction_id, object_store_id);
}

void WebIDBDatabaseImpl::RenameObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const String& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id, new_name);
}

void WebIDBDatabaseImpl::CreateTransaction(
    long long transaction_id,
    const Vector<int64_t>& object_store_ids,
    WebIDBTransactionMode mode) {
  database_->CreateTransaction(transaction_id, object_store_ids, mode);
}

void WebIDBDatabaseImpl::Close() {
  database_->Close();
}

void WebIDBDatabaseImpl::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabaseImpl::AddObserver(
    long long transaction_id,
    int32_t observer_id,
    bool include_transaction,
    bool no_records,
    bool values,
    const std::bitset<kWebIDBOperationTypeCount>& operation_types) {
  static_assert(kWebIDBOperationTypeCount < sizeof(uint16_t) * CHAR_BIT,
                "WebIDBOperationType Count exceeds size of uint16_t");
  database_->AddObserver(transaction_id, observer_id, include_transaction,
                         no_records, values, operation_types.to_ulong());
}

void WebIDBDatabaseImpl::RemoveObservers(const Vector<int32_t>& observer_ids) {
  database_->RemoveObservers(observer_ids);
}

void WebIDBDatabaseImpl::Get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const WebIDBKeyRange& key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Get(transaction_id, object_store_id, index_id, key_range, key_only,
                 GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::GetAll(long long transaction_id,
                                long long object_store_id,
                                long long index_id,
                                const WebIDBKeyRange& key_range,
                                long long max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->GetAll(transaction_id, object_store_id, index_id, key_range,
                    key_only, max_count,
                    GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Put(long long transaction_id,
                             long long object_store_id,
                             const WebData& value,
                             const Vector<WebBlobInfo>& web_blob_info,
                             WebIDBKeyView web_primary_key,
                             WebIDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const Vector<WebIDBIndexKeys>& index_keys) {
  WebIDBKey key = WebIDBKeyBuilder::Build(web_primary_key);

  if (value.size() + key.SizeEstimate() > max_put_value_size_) {
    callbacks->OnError(WebIDBDatabaseError(
        kWebIDBDatabaseExceptionUnknownError,
        WebString(String::Format("The serialized value is too large"
                                 " (size=%" PRIuS " bytes, max=%" PRIuS
                                 " bytes).",
                                 value.size(), max_put_value_size_))));
    return;
  }

  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto mojo_value = mojom::blink::IDBValue::New();
  DCHECK(mojo_value->bits.IsEmpty());
  value.ForEachSegment([&mojo_value](const char* segment, size_t segment_size,
                                     size_t segment_offset) {
    mojo_value->bits.append(String(segment, segment_size));
    return true;
  });
  mojo_value->blob_or_file_info.ReserveInitialCapacity(web_blob_info.size());
  for (const WebBlobInfo& info : web_blob_info) {
    auto blob_info = mojom::blink::IDBBlobInfo::New();
    if (info.IsFile()) {
      blob_info->file = mojom::blink::IDBFileInfo::New();
      blob_info->file->path = WebStringToFilePath(info.FilePath());
      String name = info.FileName();
      if (name.IsNull())
        name = g_empty_string;
      blob_info->file->name = name;
      blob_info->file->last_modified =
          base::Time::FromDoubleT(info.LastModified());
    }
    blob_info->size = info.size();
    blob_info->uuid = info.Uuid();
    DCHECK(!blob_info->uuid.IsEmpty());
    String mime_type = info.GetType();
    if (mime_type.IsNull())
      mime_type = g_empty_string;
    blob_info->mime_type = mime_type;
    blob_info->blob = mojom::blink::BlobPtrInfo(info.CloneBlobHandle(),
                                                mojom::blink::Blob::Version_);
    mojo_value->blob_or_file_info.push_back(std::move(blob_info));
  }

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Put(transaction_id, object_store_id, std::move(mojo_value),
                 std::move(key), put_mode, std::move(index_keys),
                 GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::SetIndexKeys(
    long long transaction_id,
    long long object_store_id,
    WebIDBKeyView primary_key,
    const Vector<WebIDBIndexKeys>& index_keys) {
  IndexedDBKey temp(IndexedDBKeyBuilder::Build(primary_key));
  database_->SetIndexKeys(transaction_id, object_store_id,
                          WebIDBKeyBuilder::Build(temp), std::move(index_keys));
}

void WebIDBDatabaseImpl::SetIndexesReady(long long transaction_id,
                                         long long object_store_id,
                                         const Vector<int64_t>& index_ids) {
  database_->SetIndexesReady(transaction_id, object_store_id,
                             std::move(index_ids));
}

void WebIDBDatabaseImpl::OpenCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const WebIDBKeyRange& key_range,
                                    WebIDBCursorDirection direction,
                                    bool key_only,
                                    WebIDBTaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->OpenCursor(transaction_id, object_store_id, index_id, key_range,
                        direction, key_only, task_type,
                        GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Count(long long transaction_id,
                               long long object_store_id,
                               long long index_id,
                               const WebIDBKeyRange& key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Count(transaction_id, object_store_id, index_id, key_range,
                   GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Delete(long long transaction_id,
                                long long object_store_id,
                                WebIDBKeyView primary_key,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->DeleteRange(transaction_id, object_store_id,
                         WebIDBKeyRangeBuilder::Build(primary_key),
                         GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::DeleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const WebIDBKeyRange& key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->DeleteRange(transaction_id, object_store_id, key_range,
                         GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Clear(long long transaction_id,
                               long long object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Clear(transaction_id, object_store_id,
                   GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::CreateIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const String& name,
                                     const WebIDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id, name,
                         key_path, unique, multi_entry);
}

void WebIDBDatabaseImpl::DeleteIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabaseImpl::RenameIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const String& new_name) {
  DCHECK(!new_name.IsNull());
  database_->RenameIndex(transaction_id, object_store_id, index_id, new_name);
}

void WebIDBDatabaseImpl::Abort(long long transaction_id) {
  database_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::Commit(long long transaction_id) {
  database_->Commit(transaction_id);
}

mojom::blink::IDBCallbacksAssociatedPtrInfo
WebIDBDatabaseImpl::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  mojom::blink::IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace blink
