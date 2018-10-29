// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"

#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_name_and_version.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

using blink::IndexedDBDatabaseMetadata;
using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBMetadata;
using blink::WebIDBNameAndVersion;
using blink::WebIDBValue;
using blink::WebString;
using blink::WebVector;
using blink::mojom::blink::IDBDatabaseAssociatedPtrInfo;

namespace blink {

namespace {

WebIDBValue ConvertReturnValue(const mojom::blink::IDBReturnValuePtr& value) {
  if (!value)
    return WebIDBValue(WebData(), WebVector<WebBlobInfo>());

  WebIDBValue web_value = IndexedDBCallbacksImpl::ConvertValue(value->value);
  web_value.SetInjectedPrimaryKey(value->primary_key, value->key_path);
  return web_value;
}

WebIDBNameAndVersion ConvertNameVersion(
    const mojom::blink::IDBNameAndVersionPtr& name_and_version) {
  return WebIDBNameAndVersion(name_and_version->name,
                              name_and_version->version);
}

}  // namespace

// static
WebIDBValue IndexedDBCallbacksImpl::ConvertValue(
    const mojom::blink::IDBValuePtr& value) {
  if (!value || value->bits.length() == 0)
    return WebIDBValue(WebData(), WebVector<WebBlobInfo>());

  WebVector<WebBlobInfo> local_blob_info;
  local_blob_info.reserve(value->blob_or_file_info.size());
  for (const auto& info : value->blob_or_file_info) {
    if (info->file) {
      local_blob_info.emplace_back(
          WebString(info->uuid), FilePathToWebString(info->file->path),
          WebString(info->file->name), WebString(info->mime_type),
          info->file->last_modified.ToDoubleT(), info->size,
          info->blob.PassHandle());
    } else {
      local_blob_info.emplace_back(WebString(info->uuid),
                                   WebString(info->mime_type), info->size,
                                   info->blob.PassHandle());
    }
  }

  return WebIDBValue(WebData(value->bits.Latin1().data(), value->bits.length()),
                     std::move(local_blob_info));
}

IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    int64_t transaction_id,
    const base::WeakPtr<WebIDBCursorImpl>& cursor)
    : callbacks_(std::move(callbacks)),
      cursor_(cursor),
      transaction_id_(transaction_id) {}

IndexedDBCallbacksImpl::~IndexedDBCallbacksImpl() = default;

void IndexedDBCallbacksImpl::Error(int32_t code, const WTF::String& message) {
  callbacks_->OnError(WebIDBDatabaseError(code, WebString(message)));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessNamesAndVersionsList(
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) {
  WebVector<WebIDBNameAndVersion> web_names_and_versions;
  web_names_and_versions.reserve(names_and_versions.size());
  for (const mojom::blink::IDBNameAndVersionPtr& name_version :
       names_and_versions)
    web_names_and_versions.emplace_back(ConvertNameVersion(name_version));
  callbacks_->OnSuccess(web_names_and_versions);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessStringList(const Vector<String>& value) {
  WebVector<WebString> web_value(value.size());
  std::transform(value.begin(), value.end(), web_value.begin(),
                 [](const WTF::String& s) { return WebString(s); });
  callbacks_->OnSuccess(web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Blocked(int64_t existing_version) {
  callbacks_->OnBlocked(existing_version);
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set OnUpgradeNeeded() / OnSuccess() /
  // OnError().
}

void IndexedDBCallbacksImpl::UpgradeNeeded(
    IDBDatabaseAssociatedPtrInfo database_info,
    int64_t old_version,
    WebIDBDataLoss data_loss,
    const String& data_loss_message,
    const WebIDBMetadata& web_metadata) {
  WebIDBDatabase* database = new WebIDBDatabaseImpl(std::move(database_info));
  callbacks_->OnUpgradeNeeded(old_version, database, web_metadata, data_loss,
                              WebString(data_loss_message));
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set OnSuccess() / OnError().
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    IDBDatabaseAssociatedPtrInfo database_info,
    const WebIDBMetadata& web_metadata) {
  WebIDBDatabase* database = nullptr;
  if (database_info.is_valid())
    database = new WebIDBDatabaseImpl(std::move(database_info));

  callbacks_->OnSuccess(database, web_metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursor(
    mojom::blink::IDBCursorAssociatedPtrInfo cursor_info,
    WebIDBKey key,
    WebIDBKey primary_key,
    mojom::blink::IDBValuePtr value) {
  WebIDBCursorImpl* cursor =
      new WebIDBCursorImpl(std::move(cursor_info), transaction_id_);
  callbacks_->OnSuccess(cursor, std::move(key), std::move(primary_key),
                        ConvertValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessValue(
    mojom::blink::IDBReturnValuePtr value) {
  callbacks_->OnSuccess(ConvertReturnValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorContinue(
    WebIDBKey key,
    WebIDBKey primary_key,
    mojom::blink::IDBValuePtr value) {
  callbacks_->OnSuccess(std::move(key), std::move(primary_key),
                        ConvertValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorPrefetch(
    Vector<WebIDBKey> keys,
    Vector<WebIDBKey> primary_keys,
    Vector<mojom::blink::IDBValuePtr> values) {
  Vector<WebIDBValue> web_values;
  web_values.ReserveInitialCapacity(values.size());
  for (const mojom::blink::IDBValuePtr& value : values)
    web_values.emplace_back(ConvertValue(value));

  if (cursor_) {
    cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                             std::move(web_values));
    cursor_->CachedContinue(callbacks_.get());
  }
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  WebVector<WebIDBValue> web_values;
  web_values.reserve(values.size());
  for (const mojom::blink::IDBReturnValuePtr& value : values)
    web_values.emplace_back(ConvertReturnValue(value));
  callbacks_->OnSuccess(std::move(web_values));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessKey(WebIDBKey key) {
  callbacks_->OnSuccess(std::move(key));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callbacks_->OnSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Success() {
  callbacks_->OnSuccess();
  callbacks_.reset();
}

}  // namespace blink
