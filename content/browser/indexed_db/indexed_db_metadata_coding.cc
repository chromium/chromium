// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_metadata_coding.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope_deletion_mode.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using base::StringPiece;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKeyPath;
using blink::IndexedDBObjectStoreMetadata;
using leveldb::Status;

namespace content {
using indexed_db::CheckIndexAndMetaDataKey;
using indexed_db::CheckObjectStoreAndMetaDataType;
using indexed_db::GetInt;
using indexed_db::GetVarInt;
using indexed_db::GetString;
using indexed_db::InternalInconsistencyStatus;
using indexed_db::InvalidDBKeyStatus;
using indexed_db::PutBool;
using indexed_db::PutIDBKeyPath;
using indexed_db::PutInt;
using indexed_db::PutString;
using indexed_db::PutVarInt;

IndexedDBMetadataCoding::IndexedDBMetadataCoding() = default;
IndexedDBMetadataCoding::~IndexedDBMetadataCoding() = default;

leveldb::Status IndexedDBMetadataCoding::SetDatabaseVersion(
    TransactionalLevelDBTransaction* transaction,
    int64_t row_id,
    int64_t version,
    IndexedDBDatabaseMetadata* c) {
  if (version == IndexedDBDatabaseMetadata::NO_VERSION)
    version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
  DCHECK_GE(version, 0) << "version was " << version;
  c->version = version;
  return PutVarInt(
      transaction,
      DatabaseMetaDataKey::Encode(row_id, DatabaseMetaDataKey::USER_VERSION),
      version);
}

Status IndexedDBMetadataCoding::FindDatabaseId(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    const std::u16string& name,
    int64_t* id,
    bool* found) {
  const std::string key = DatabaseNameKey::Encode(origin_identifier, name);

  Status s = GetInt(db, key, id, found);
  if (!s.ok())
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);

  return s;
}

Status IndexedDBMetadataCoding::CreateObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    std::u16string name,
    IndexedDBKeyPath key_path,
    bool auto_increment,
    IndexedDBObjectStoreMetadata* metadata) {
  DCHECK(transaction);
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  Status s = indexed_db::SetMaxObjectStoreId(transaction, database_id,
                                             object_store_id);
  if (!s.ok())
    return s;

  static const constexpr int64_t kInitialLastVersionNumber = 1;
  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::NAME);
  const std::string key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::KEY_PATH);
  const std::string auto_increment_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::AUTO_INCREMENT);
  const std::string evictable_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::EVICTABLE);
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  const std::string has_key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::HAS_KEY_PATH);
  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  const std::string names_key = ObjectStoreNamesKey::Encode(database_id, name);

  s = PutString(transaction, name_key, name);
  if (!s.ok())
    return s;
  s = PutIDBKeyPath(transaction, key_path_key, key_path);
  if (!s.ok())
    return s;
  s = PutInt(transaction, auto_increment_key, auto_increment);
  if (!s.ok())
    return s;
  s = PutInt(transaction, evictable_key, false);
  if (!s.ok())
    return s;
  s = PutInt(transaction, last_version_key, kInitialLastVersionNumber);
  if (!s.ok())
    return s;
  s = PutInt(transaction, max_index_id_key, kMinimumIndexId);
  if (!s.ok())
    return s;
  s = PutBool(transaction, has_key_path_key, !key_path.IsNull());
  if (!s.ok())
    return s;
  s = PutInt(transaction, key_generator_current_number_key,
             ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
  if (!s.ok())
    return s;
  s = PutInt(transaction, names_key, object_store_id);
  if (!s.ok())
    return s;

  metadata->name = std::move(name);
  metadata->id = object_store_id;
  metadata->key_path = std::move(key_path);
  metadata->auto_increment = auto_increment;
  metadata->max_index_id = IndexedDBObjectStoreMetadata::kMinimumIndexId;
  metadata->indexes.clear();
  return s;
}

Status IndexedDBMetadataCoding::DeleteObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    const IndexedDBObjectStoreMetadata& object_store) {
  if (!KeyPrefix::ValidIds(database_id, object_store.id))
    return InvalidDBKeyStatus();

  std::u16string object_store_name;
  bool found = false;
  Status s =
      GetString(transaction,
                ObjectStoreMetaDataKey::Encode(database_id, object_store.id,
                                               ObjectStoreMetaDataKey::NAME),
                &object_store_name, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  s = transaction->RemoveRange(
      ObjectStoreMetaDataKey::Encode(database_id, object_store.id, 0),
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id, object_store.id),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);

  if (s.ok()) {
    s = transaction->Remove(
        ObjectStoreNamesKey::Encode(database_id, object_store_name));
    if (!s.ok()) {
      INTERNAL_WRITE_ERROR(DELETE_OBJECT_STORE);
      return s;
    }

    s = transaction->RemoveRange(
        IndexFreeListKey::Encode(database_id, object_store.id, 0),
        IndexFreeListKey::EncodeMaxKey(database_id, object_store.id),
        LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  }

  if (s.ok()) {
    s = transaction->RemoveRange(
        IndexMetaDataKey::Encode(database_id, object_store.id, 0, 0),
        IndexMetaDataKey::EncodeMaxKey(database_id, object_store.id),
        LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  }

  if (!s.ok())
    INTERNAL_WRITE_ERROR(DELETE_OBJECT_STORE);
  return s;
}

Status IndexedDBMetadataCoding::RenameObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    std::u16string new_name,
    std::u16string* old_name,
    IndexedDBObjectStoreMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, metadata->id))
    return InvalidDBKeyStatus();

  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, metadata->id, ObjectStoreMetaDataKey::NAME);
  const std::string new_names_key =
      ObjectStoreNamesKey::Encode(database_id, new_name);

  std::u16string old_name_check;
  bool found = false;
  Status s = GetString(transaction, name_key, &old_name_check, &found);
  // TODO(dmurph): Change DELETE_OBJECT_STORE to RENAME_OBJECT_STORE & fix UMA.
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found || old_name_check != metadata->name) {
    INTERNAL_CONSISTENCY_ERROR(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }
  const std::string old_names_key =
      ObjectStoreNamesKey::Encode(database_id, metadata->name);

  s = PutString(transaction, name_key, new_name);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  s = PutInt(transaction, new_names_key, metadata->id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  s = transaction->Remove(old_names_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return s;
}

Status IndexedDBMetadataCoding::CreateIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    std::u16string name,
    IndexedDBKeyPath key_path,
    bool is_unique,
    bool is_multi_entry,
    IndexedDBIndexMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();
  Status s = indexed_db::SetMaxIndexId(transaction, database_id,
                                       object_store_id, index_id);

  if (!s.ok())
    return s;

  const std::string name_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::NAME);
  const std::string unique_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::UNIQUE);
  const std::string key_path_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::KEY_PATH);
  const std::string multi_entry_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::MULTI_ENTRY);

  s = PutString(transaction, name_key, name);
  if (!s.ok())
    return s;
  s = PutBool(transaction, unique_key, is_unique);
  if (!s.ok())
    return s;
  s = PutIDBKeyPath(transaction, key_path_key, key_path);
  if (!s.ok())
    return s;
  s = PutBool(transaction, multi_entry_key, is_multi_entry);
  if (!s.ok())
    return s;

  metadata->name = std::move(name);
  metadata->id = index_id;
  metadata->key_path = std::move(key_path);
  metadata->unique = is_unique;
  metadata->multi_entry = is_multi_entry;

  return s;
}

Status IndexedDBMetadataCoding::DeleteIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBIndexMetadata& metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, metadata.id))
    return InvalidDBKeyStatus();

  const std::string index_meta_data_start =
      IndexMetaDataKey::Encode(database_id, object_store_id, metadata.id, 0);
  const std::string index_meta_data_end =
      IndexMetaDataKey::EncodeMaxKey(database_id, object_store_id, metadata.id);
  Status s = transaction->RemoveRange(
      index_meta_data_start, index_meta_data_end,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive);
  return s;
}

Status IndexedDBMetadataCoding::RenameIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    std::u16string new_name,
    std::u16string* old_name,
    IndexedDBIndexMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, metadata->id))
    return InvalidDBKeyStatus();

  const std::string name_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, metadata->id, IndexMetaDataKey::NAME);

  // TODO(dmurph): Add consistancy checks & umas for old name.
  leveldb::Status s = PutString(transaction, name_key, new_name);
  if (!s.ok())
    return s;
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

}  // namespace content
