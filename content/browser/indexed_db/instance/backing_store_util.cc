// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_util.h"

#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "crypto/hash.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

namespace {

// Converts an integer that may have different length or signedness to a
// base::Value, since base::Value only natively supports int. If it doesn't fit
// in an int, the output will be a string.
template <typename T>
base::Value ToIntValue(T value) {
  T small_value = static_cast<int>(value);
  if (value == small_value) {
    return base::Value(static_cast<int>(small_value));
  }
  return base::Value(base::NumberToString(value));
}

// When keys or values are longer than this number, or an object store has more
// records than this number, the contents will be hashed instead of stored
// verbatim. This limits total memory usage.
static const int kHashingThreshold = 256;

base::DictValue DatabaseMetadataToDictValue(const BackingStore::Database& db) {
  base::DictValue result;
  const blink::IndexedDBDatabaseMetadata& metadata = db.GetMetadata();
  result.Set("name", metadata.name);
  result.Set("version", ToIntValue(metadata.version));

  base::ListValue object_stores;
  for (const auto& [_, object_store] : metadata.object_stores) {
    base::DictValue object_store_dict;
    object_store_dict.Set("name", object_store.name);
    object_store_dict.Set("id", ToIntValue(object_store.id));
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
      index_dict.Set("id", ToIntValue(index.id));
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

// Fully hashes keys and values from the cursor. For use when there are a lot of
// rows (high `key_count`).
StatusOr<base::BlobStorage> CursorToSummaryValue(
    std::unique_ptr<BackingStore::Cursor>& cursor,
    bool include_primary_key,
    size_t key_count) {
  crypto::hash::Hasher streaming_hasher(crypto::hash::HashKind::kSha256);

  while (cursor) {
    streaming_hasher.Update(cursor->GetKey().DebugString());
    if (include_primary_key) {
      streaming_hasher.Update(cursor->GetPrimaryKey().DebugString());
    }
    streaming_hasher.Update(cursor->GetValue().bits);

    std::vector<int64_t> object_data;
    object_data.reserve(2 * cursor->GetValue().external_objects.size());
    for (const IndexedDBExternalObject& object :
         cursor->GetValue().external_objects) {
      object_data.push_back(static_cast<int64_t>(object.object_type()));
      object_data.push_back(object.size());
    }
    streaming_hasher.Update(base::as_byte_span(object_data));

    StatusOr<bool> continue_result = cursor->Continue();
    if (!continue_result.has_value()) {
      return base::unexpected(continue_result.error());
    }
    if (!*continue_result) {
      break;
    }
  }

  base::BlobStorage digest(crypto::hash::kSha256Size);
  streaming_hasher.Finish(digest);
  return digest;
}

// Turns the record pointed to by `Cursor` into a dictionary, and applies some
// "light" hashing to keys and values.
base::DictValue RecordToDictValue(BackingStore::Cursor& cursor,
                                  bool include_primary_key) {
  base::DictValue record;
  std::string key_string = cursor.GetKey().DebugString();
  if (key_string.size() > kHashingThreshold) {
    record.Set(
        "key_digest",
        base::ToVector(crypto::hash::Sha256(base::as_byte_span(key_string))));
  } else {
    record.Set("key", std::move(key_string));
  }
  if (include_primary_key) {
    key_string = cursor.GetPrimaryKey().DebugString();
    if (key_string.size() > kHashingThreshold) {
      record.Set(
          "primary_key_digest",
          base::ToVector(crypto::hash::Sha256(base::as_byte_span(key_string))));
    } else {
      record.Set("primary_key", std::move(key_string));
    }
  }

  IndexedDBValue value = std::move(cursor.GetValue());
  if (value.bits.size() > kHashingThreshold) {
    record.Set("value_digest",
               base::ToVector(crypto::hash::Sha256(value.bits)));
  } else {
    record.Set("value", base::ToVector(value.bits));
  }

  // Include some limited metadata for blobs (and other external objects).
  base::ListValue external_objects;
  for (const IndexedDBExternalObject& object : value.external_objects) {
    base::DictValue object_dict;
    object_dict.Set("type", static_cast<int>(object.object_type()));
    object_dict.Set("size", ToIntValue(object.size()));
    external_objects.Append(std::move(object_dict));
  }
  record.Set("external_objects", std::move(external_objects));

  return record;
}

StatusOr<base::ListValue> CursorToListValue(
    std::unique_ptr<BackingStore::Cursor>& cursor,
    bool include_primary_key,
    size_t key_count) {
  base::ListValue records = base::ListValue::with_capacity(key_count);
  while (cursor) {
    records.Append(RecordToDictValue(*cursor, include_primary_key));
    StatusOr<bool> continue_result = cursor->Continue();
    if (!continue_result.has_value()) {
      return base::unexpected(continue_result.error());
    }
    if (!*continue_result) {
      break;
    }
  }
  return records;
}

StatusOr<base::DictValue> IndexToDictValue(
    const blink::IndexedDBObjectStoreMetadata& object_store,
    const blink::IndexedDBIndexMetadata& index,
    BackingStore::Transaction& txn) {
  base::DictValue contents;

  // This is technically unnecessary.
  StatusOr<uint32_t> key_count =
      txn.GetIndexKeyCount(object_store.id, index.id, /*key_range=*/{});
  if (!key_count.has_value()) {
    return base::unexpected(key_count.error());
  }
  contents.Set("record_count", ToIntValue(*key_count));

  // Print all records via Cursor.
  StatusOr<std::unique_ptr<BackingStore::Cursor>> cursor =
      txn.OpenIndexCursor(object_store.id, index.id, /*key_range=*/{},
                          blink::mojom::IDBCursorDirection::Next);
  if (!cursor.has_value()) {
    return base::unexpected(cursor.error());
  }
  if (*key_count > kHashingThreshold) {
    StatusOr<base::BlobStorage> records_digest =
        CursorToSummaryValue(*cursor, /*include_primary_key=*/true, *key_count);
    if (!records_digest.has_value()) {
      return base::unexpected(records_digest.error());
    }
    contents.Set("records_digest", *std::move(records_digest));
  } else {
    StatusOr<base::ListValue> records =
        CursorToListValue(*cursor, /*include_primary_key=*/true, *key_count);
    if (!records.has_value()) {
      return base::unexpected(records.error());
    }
    contents.Set("records", std::move(*records));
  }

  return contents;
}

StatusOr<base::DictValue> ObjectStoreToDictValue(
    const blink::IndexedDBObjectStoreMetadata& object_store,
    BackingStore::Transaction& txn) {
  base::DictValue contents;

  // This is technically unnecessary.
  StatusOr<uint32_t> key_count =
      txn.GetObjectStoreKeyCount(object_store.id, /*key_range=*/{});
  if (!key_count.has_value()) {
    return base::unexpected(key_count.error());
  }
  contents.Set("record_count", ToIntValue(*key_count));

  // Print all records via Cursor.
  StatusOr<std::unique_ptr<BackingStore::Cursor>> cursor =
      txn.OpenObjectStoreCursor(object_store.id, /*key_range=*/{},
                                blink::mojom::IDBCursorDirection::Next);
  if (!cursor.has_value()) {
    return base::unexpected(cursor.error());
  }

  if (*key_count > kHashingThreshold) {
    StatusOr<base::BlobStorage> records_digest = CursorToSummaryValue(
        *cursor, /*include_primary_key=*/false, *key_count);
    if (!records_digest.has_value()) {
      return base::unexpected(records_digest.error());
    }
    contents.Set("records_digest", *std::move(records_digest));
  } else {
    StatusOr<base::ListValue> records =
        CursorToListValue(*cursor, /*include_primary_key=*/false, *key_count);
    if (!records.has_value()) {
      return base::unexpected(records.error());
    }
    contents.Set("records", *std::move(records));
  }

  base::DictValue indexes;
  indexes.reserve(object_store.indexes.size());
  for (const auto& [id, index] : object_store.indexes) {
    StatusOr<base::DictValue> index_dict =
        IndexToDictValue(object_store, index, txn);
    if (!index_dict.has_value()) {
      return index_dict;
    }
    indexes.Set(base::NumberToString(id), std::move(*index_dict));
  }
  contents.Set("indexes", std::move(indexes));
  return contents;
}

StatusOr<base::DictValue> DatabaseContentsToDictValue(
    BackingStore::Database& db) {
  base::DictValue contents;

  auto txn =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadOnly);
  // Locks are assumed to be unnecessary as this is a synchronous operation
  // that won't be interrupted.
  std::vector<PartitionedLock> locks;
  locks.emplace_back(PartitionedLockId{.partition = 0, .key = "unused"},
                     base::DoNothing());
  Status status = txn->Begin(std::move(locks));
  if (!status.ok()) {
    return base::unexpected(status);
  }

  base::DictValue object_stores;
  object_stores.reserve(db.GetMetadata().object_stores.size());
  for (const auto& [id, object_store] : db.GetMetadata().object_stores) {
    StatusOr<base::DictValue> obj_store =
        ObjectStoreToDictValue(object_store, *txn);
    if (!obj_store.has_value()) {
      return obj_store;
    }

    object_stores.Set(base::NumberToString(id), std::move(*obj_store));
  }
  contents.Set("object_stores", std::move(object_stores));
  return contents;
}

}  // namespace

StatusOr<base::DictValue> SnapshotDatabase(BackingStore::Database& db) {
  StatusOr<base::DictValue> metadata = DatabaseMetadataToDictValue(db);
  if (!metadata.has_value()) {
    return metadata;
  }

  StatusOr<base::DictValue> contents = DatabaseContentsToDictValue(db);
  if (!contents.has_value()) {
    return contents;
  }

  base::DictValue result;
  result.Set("metadata", *std::move(metadata));
  result.Set("contents", *std::move(contents));
  return result;
}

}  // namespace content::indexed_db
