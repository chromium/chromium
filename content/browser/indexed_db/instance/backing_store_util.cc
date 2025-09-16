// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_util.h"

#include "base/containers/to_vector.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "crypto/hash.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

namespace {

base::DictValue DatabaseMetadataToDictValue(const BackingStore::Database& db) {
  base::DictValue result;
  const blink::IndexedDBDatabaseMetadata& metadata = db.GetMetadata();
  result.Set("name", metadata.name);
  result.Set("version", static_cast<int>(metadata.version));

  base::ListValue object_stores;
  for (const auto& [_, object_store] : metadata.object_stores) {
    base::DictValue object_store_dict;
    object_store_dict.Set("name", object_store.name);
    object_store_dict.Set("id", static_cast<int>(object_store.id));
    object_store_dict.Set("auto_increment", object_store.auto_increment);

    auto key_path_to_dict = [](const blink::IndexedDBKeyPath& key_path) {
      base::DictValue key_path_dict;
      key_path_dict.Set("type", static_cast<int>(key_path.type()));
      if (key_path.type() == blink::mojom::IDBKeyPathType::String) {
        key_path_dict.Set("string", key_path.string());
      } else if (key_path.type() == blink::mojom::IDBKeyPathType::Array) {
        base::ListValue key_path_list =
            base::ListValue::with_capacity(key_path.array().size());
        for (const std::u16string& component : key_path.array()) {
          key_path_list.Append(component);
        }
        key_path_dict.Set("path", std::move(key_path_list));
      } else {
        CHECK_EQ(key_path.type(), blink::mojom::IDBKeyPathType::Null);
      }
      return key_path_dict;
    };
    object_store_dict.Set("key_path", key_path_to_dict(object_store.key_path));

    base::ListValue indexes;
    for (const auto& [_, index] : object_store.indexes) {
      base::DictValue index_dict;
      index_dict.Set("name", index.name);
      index_dict.Set("id", static_cast<int>(index.id));
      index_dict.Set("key_path", key_path_to_dict(index.key_path));
      index_dict.Set("unique", index.unique);
      index_dict.Set("multi_entry", index.multi_entry);
      indexes.Append(std::move(index_dict));
    }
    object_store_dict.Set("indexes", std::move(indexes));

    object_stores.Append(std::move(object_store_dict));
  }
  result.Set("object_stores", std::move(object_stores));
  return result;
}

base::ListValue CursorToListValue(std::unique_ptr<BackingStore::Cursor>& cursor,
                                  bool include_primary_key,
                                  size_t key_count) {
  base::ListValue records = base::ListValue::with_capacity(key_count);
  while (cursor) {
    base::DictValue record;
    std::string key_string = cursor->GetKey().DebugString();
    if (key_string.size() > 256) {
      record.Set(
          "key_hash",
          base::ToVector(crypto::hash::Sha256(base::as_byte_span(key_string))));
    } else {
      record.Set("key", std::move(key_string));
    }
    if (include_primary_key) {
      key_string = cursor->GetPrimaryKey().DebugString();
      if (key_string.size() > 256) {
        record.Set("primary_key_hash", base::ToVector(crypto::hash::Sha256(
                                           base::as_byte_span(key_string))));
      } else {
        record.Set("primary_key", std::move(key_string));
      }
    }

    record.Set("value_hash",
               base::ToVector(crypto::hash::Sha256(cursor->GetValue().bits)));

    // Include some limited metadata for blobs (and other external objects).
    base::ListValue external_objects;
    for (const IndexedDBExternalObject& object :
         cursor->GetValue().external_objects) {
      base::DictValue object_dict;
      object_dict.Set("type", static_cast<int>(object.object_type()));
      object_dict.Set("size", static_cast<int>(object.size()));
      external_objects.Append(std::move(object_dict));
    }
    record.Set("external_objects", std::move(external_objects));

    records.Append(std::move(record));
    StatusOr<bool> continue_result = cursor->Continue();
    if (!continue_result.has_value()) {
      records.Append(continue_result.error().ToString());
      return records;
    }
    if (!*continue_result) {
      break;
    }
  }
  return records;
}

base::DictValue IndexToDictValue(
    const blink::IndexedDBObjectStoreMetadata& object_store,
    const blink::IndexedDBIndexMetadata& index,
    BackingStore::Transaction& txn) {
  base::DictValue contents;

  // This is technically unnecessary.
  StatusOr<uint32_t> key_count =
      txn.GetIndexKeyCount(object_store.id, index.id, /*key_range=*/{});
  if (!key_count.has_value()) {
    contents.Set("record_count_error", key_count.error().ToString());
    return contents;
  }
  contents.Set("record_count", static_cast<int>(*key_count));

  // Print all records via Cursor.
  StatusOr<std::unique_ptr<BackingStore::Cursor>> cursor =
      txn.OpenIndexCursor(object_store.id, index.id, /*key_range=*/{},
                          blink::mojom::IDBCursorDirection::Next);
  if (!cursor.has_value()) {
    contents.Set("cursor_error", cursor.error().ToString());
    return contents;
  }
  contents.Set(
      "records",
      CursorToListValue(*cursor, /*include_primary_key=*/true, *key_count));

  return contents;
}

base::DictValue ObjectStoreToDictValue(
    const blink::IndexedDBObjectStoreMetadata& object_store,
    BackingStore::Transaction& txn) {
  base::DictValue contents;

  // This is technically unnecessary.
  StatusOr<uint32_t> key_count =
      txn.GetObjectStoreKeyCount(object_store.id, /*key_range=*/{});
  if (!key_count.has_value()) {
    contents.Set("record_count_error", key_count.error().ToString());
    return contents;
  }
  contents.Set("record_count", static_cast<int>(*key_count));

  // Print all records via Cursor.
  StatusOr<std::unique_ptr<BackingStore::Cursor>> cursor =
      txn.OpenObjectStoreCursor(object_store.id, /*key_range=*/{},
                                blink::mojom::IDBCursorDirection::Next);
  if (!cursor.has_value()) {
    contents.Set("cursor_error", cursor.error().ToString());
    return contents;
  }
  contents.Set(
      "records",
      CursorToListValue(*cursor, /*include_primary_key=*/false, *key_count));

  base::ListValue indexes =
      base::ListValue::with_capacity(object_store.indexes.size());
  for (const auto& [_, index] : object_store.indexes) {
    indexes.Append(IndexToDictValue(object_store, index, txn));
  }
  contents.Set("indexes", std::move(indexes));
  return contents;
}

base::DictValue DatabaseContentsToDictValue(BackingStore::Database& db) {
  base::DictValue contents;

  auto txn =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Default,
                           blink::mojom::IDBTransactionMode::ReadOnly);
  if (!txn->Begin({}).ok()) {
    contents.Set("error", "Failed to begin read-only transaction");
    return contents;
  }

  base::ListValue object_stores =
      base::ListValue::with_capacity(db.GetMetadata().object_stores.size());
  for (const auto& [_, object_store] : db.GetMetadata().object_stores) {
    object_stores.Append(ObjectStoreToDictValue(object_store, *txn));
  }
  contents.Set("object_stores", std::move(object_stores));
  return contents;
}

}  // namespace

base::DictValue DumpDatabase(BackingStore::Database& db) {
  base::DictValue result;
  result.Set("metadata", DatabaseMetadataToDictValue(db));
  result.Set("contents", DatabaseContentsToDictValue(db));
  return result;
}

}  // namespace content::indexed_db
