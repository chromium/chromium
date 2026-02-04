// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_util.h"

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/container/node_hash_map.h"
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
    if (object.object_type() ==
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle) {
      object_dict.Set(
          "fsa_handle",
          base::BlobStorage(object.serialized_file_system_access_handle()));
    } else {
      object_dict.Set("size", ToIntValue(object.size()));
    }
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

Status MigrateDatabase(BackingStore::Database& source,
                       BackingStore::Database& target) {
  const blink::IndexedDBDatabaseMetadata& metadata = source.GetMetadata();

  // All the writes into `target` occur in a single VersionChange transaction.
  std::unique_ptr<BackingStore::Transaction> target_txn =
      target.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::VersionChange);
  std::vector<PartitionedLock> target_locks;
  target_locks.emplace_back(PartitionedLockId{.partition = 0, .key = "unused"},
                            base::DoNothing());
  // TODO(crbug.com/419264073): handle errors.
  CHECK(target_txn->Begin(std::move(target_locks)).ok());

  CHECK(target_txn->SetDatabaseVersion(metadata.version).ok());

  // Create all the object stores and indexes.
  for (const auto& [_, object_store] : metadata.object_stores) {
    CHECK(target_txn
              ->CreateObjectStore(object_store.id, object_store.name,
                                  object_store.key_path,
                                  object_store.auto_increment)
              .ok());

    for (const auto& [_, index] : object_store.indexes) {
      CHECK(target_txn->CreateIndex(object_store.id, index).ok());
    }
  }

  // Read and duplicate all the records in all the object stores of `source`.
  std::unique_ptr<BackingStore::Transaction> source_txn =
      source.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadOnly);
  std::vector<PartitionedLock> source_locks;
  source_locks.emplace_back(PartitionedLockId{.partition = 0, .key = "unused"},
                            base::DoNothing());
  CHECK(source_txn->Begin(std::move(source_locks)).ok());

  for (const auto& [_, object_store] : metadata.object_stores) {
    // Maintain a mapping from primary key to record identifier in the target
    // database. This is necessary because record identifiers are used to
    // rebuild the indexes in `target`.
    // Unlike `flat_hash_map`, `node_hash_map` doesn't require keys be
    // `CopyConstructible` (i.e. doesn't copy on rehash).
    absl::node_hash_map<blink::IndexedDBKey, BackingStore::RecordIdentifier>
        primary_key_to_record_id;

    // Copy all records in the object store and build the map.
    std::unique_ptr<BackingStore::Cursor> cursor =
        source_txn
            ->OpenObjectStoreCursor(object_store.id, /*key_range=*/{},
                                    blink::mojom::IDBCursorDirection::Next)
            .value();

    for (bool has_values = !!cursor; has_values;
         has_values = cursor->Continue().value()) {
      blink::IndexedDBKey key = cursor->GetKey().Clone();
      IndexedDBValue value = std::move(cursor->GetValue());

      // Put the record into `target`.
      ASSIGN_OR_RETURN(
          BackingStore::RecordIdentifier record_result,
          target_txn->PutRecord(object_store.id, key, std::move(value)));

      // Store the record identifier in the map.
      primary_key_to_record_id.emplace(std::move(key),
                                       std::move(record_result));
    }

    // Rebuild indexes (pointers from index key to record identifier).
    for (const auto& [index_id, index] : object_store.indexes) {
      std::unique_ptr<BackingStore::Cursor> index_cursor =
          source_txn
              ->OpenIndexCursor(object_store.id, index_id,
                                /*key_range=*/{},
                                blink::mojom::IDBCursorDirection::Next)
              .value();

      for (bool has_values = !!index_cursor; has_values;
           has_values = index_cursor->Continue().value()) {
        blink::IndexedDBKey index_key = index_cursor->GetKey().Clone();
        const blink::IndexedDBKey& primary_key = index_cursor->GetPrimaryKey();

        // Look up the record identifier for this primary key.
        auto it = primary_key_to_record_id.find(primary_key.Clone());
        CHECK(it != primary_key_to_record_id.end());

        CHECK(target_txn
                  ->PutIndexDataForRecord(object_store.id, index_id, index_key,
                                          /*record=*/it->second)
                  .ok());
      }
    }

    // Update the key generator current number if the object store has
    // auto_increment.
    if (object_store.auto_increment) {
      int64_t current_number =
          source_txn->GetKeyGeneratorCurrentNumber(object_store.id).value();
      CHECK(target_txn
                ->MaybeUpdateKeyGeneratorCurrentNumber(
                    object_store.id, current_number, /*was_generated=*/false)
                .ok());
    }
  }

  // Commit the target transaction. We can skip phase one because there are no
  // async blob writing operations.
  return target_txn->CommitPhaseTwo();
}

}  // namespace content::indexed_db
