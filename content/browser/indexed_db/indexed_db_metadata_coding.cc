// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_metadata_coding.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope_deletion_mode.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using base::StringPiece;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKeyPath;
using blink::mojom::IDBNameAndVersionPtr;
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

namespace {

std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
    TransactionalLevelDBDatabase* database) {
  return database->CreateIterator(database->DefaultReadOptions());
}

std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
    TransactionalLevelDBTransaction* transaction) {
  return transaction->CreateIterator();
}

// Reads all indexes for the given database and object store in |indexes|.
// TODO(jsbell): This should do some error handling rather than plowing ahead
// when bad data is encountered.
template <typename DatabaseOrTransaction>
Status ReadIndexes(DatabaseOrTransaction* db_or_transaction,
                   int64_t database_id,
                   int64_t object_store_id,
                   std::map<int64_t, IndexedDBIndexMetadata>* indexes) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      IndexMetaDataKey::Encode(database_id, object_store_id, 0, 0);
  const std::string stop_key =
      IndexMetaDataKey::Encode(database_id, object_store_id + 1, 0, 0);

  DCHECK(indexes->empty());

  std::unique_ptr<TransactionalLevelDBIterator> it =
      CreateIterator(db_or_transaction);
  Status s = it->Seek(start_key);
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    IndexMetaDataKey meta_data_key;
    {
      StringPiece slice(it->Key());
      bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
      DCHECK(ok);
    }
    if (meta_data_key.meta_data_type() != IndexMetaDataKey::NAME) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      // Possible stale metadata due to http://webkit.org/b/85557 but don't fail
      // the load.
      s = it->Next();
      if (!s.ok())
        break;
      continue;
    }

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    int64_t index_id = meta_data_key.IndexId();
    base::string16 index_name;
    {
      StringPiece slice(it->Value());
      if (!DecodeString(&slice, &index_name) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // unique flag
    if (!s.ok())
      break;
    if (!CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                  IndexMetaDataKey::UNIQUE)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      break;
    }
    bool index_unique;
    {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &index_unique) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // key_path
    if (!s.ok())
      break;
    if (!CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                  IndexMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
      break;
    }
    IndexedDBKeyPath key_path;
    {
      StringPiece slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);
    }

    s = it->Next();  // [optional] multi_entry flag
    if (!s.ok())
      break;
    bool index_multi_entry = false;
    if (CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                 IndexMetaDataKey::MULTI_ENTRY)) {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &index_multi_entry) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_INDEXES);

      s = it->Next();
      if (!s.ok())
        break;
    }

    (*indexes)[index_id] = IndexedDBIndexMetadata(
        index_name, index_id, key_path, index_unique, index_multi_entry);
  }

  if (!s.ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_INDEXES);

  return s;
}

// Reads all object stores and indexes for the given |database_id| into
// |object_stores|.
// TODO(jsbell): This should do some error handling rather than plowing ahead
// when bad data is encountered.
template <typename DatabaseOrTransaction>
Status ReadObjectStores(
    DatabaseOrTransaction* db_or_transaction,
    int64_t database_id,
    std::map<int64_t, IndexedDBObjectStoreMetadata>* object_stores) {
  if (!KeyPrefix::IsValidDatabaseId(database_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      ObjectStoreMetaDataKey::Encode(database_id, 1, 0);
  const std::string stop_key =
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id);

  DCHECK(object_stores->empty());

  std::unique_ptr<TransactionalLevelDBIterator> it =
      CreateIterator(db_or_transaction);
  Status s = it->Seek(start_key);
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    ObjectStoreMetaDataKey meta_data_key;
    {
      StringPiece slice(it->Key());
      bool ok = ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key) &&
                slice.empty();
      DCHECK(ok);
      if (!ok || meta_data_key.MetaDataType() != ObjectStoreMetaDataKey::NAME) {
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
        // Possible stale metadata, but don't fail the load.
        s = it->Next();
        if (!s.ok())
          break;
        continue;
      }
    }

    int64_t object_store_id = meta_data_key.ObjectStoreId();

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    base::string16 object_store_name;
    {
      StringPiece slice(it->Value());
      if (!DecodeString(&slice, &object_store_name) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                         ObjectStoreMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    IndexedDBKeyPath key_path;
    {
      StringPiece slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::AUTO_INCREMENT)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    bool auto_increment;
    {
      StringPiece slice(it->Value());
      if (!DecodeBool(&slice, &auto_increment) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();  // Is evictable.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                         ObjectStoreMetaDataKey::EVICTABLE)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Last version.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::LAST_VERSION)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Maximum index id allocated.
    if (!s.ok())
      break;
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::MAX_INDEX_ID)) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      break;
    }
    int64_t max_index_id;
    {
      StringPiece slice(it->Value());
      if (!DecodeInt(&slice, &max_index_id) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
    }

    s = it->Next();  // [optional] has key path (is not null)
    if (!s.ok())
      break;
    if (CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                        ObjectStoreMetaDataKey::HAS_KEY_PATH)) {
      bool has_key_path;
      {
        StringPiece slice(it->Value());
        if (!DecodeBool(&slice, &has_key_path))
          INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
      }
      // This check accounts for two layers of legacy coding:
      // (1) Initially, has_key_path was added to distinguish null vs. string.
      // (2) Later, null vs. string vs. array was stored in the key_path itself.
      // So this check is only relevant for string-type key_paths.
      if (!has_key_path &&
          (key_path.type() == blink::mojom::IDBKeyPathType::String &&
           !key_path.string().empty())) {
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);
        break;
      }
      if (!has_key_path)
        key_path = IndexedDBKeyPath();
      s = it->Next();
      if (!s.ok())
        break;
    }

    int64_t key_generator_current_number = -1;
    if (CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER)) {
      StringPiece slice(it->Value());
      if (!DecodeInt(&slice, &key_generator_current_number) || !slice.empty())
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_OBJECT_STORES);

      // TODO(jsbell): Return key_generator_current_number, cache in
      // object store, and write lazily to backing store.  For now,
      // just assert that if it was written it was valid.
      DCHECK_GE(key_generator_current_number,
                ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
      s = it->Next();
      if (!s.ok())
        break;
    }

    IndexedDBObjectStoreMetadata metadata(object_store_name, object_store_id,
                                          key_path, auto_increment,
                                          max_index_id);
    s = ReadIndexes(db_or_transaction, database_id, object_store_id,
                    &metadata.indexes);
    if (!s.ok())
      break;
    (*object_stores)[object_store_id] = metadata;
  }

  if (!s.ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_OBJECT_STORES);

  return s;
}

template <typename DatabaseOrTransaction>
Status ReadDatabaseNamesAndVersionsInternal(
    DatabaseOrTransaction* db_or_transaction,
    const std::string& origin_identifier,
    std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions) {
  const std::string start_key =
      DatabaseNameKey::EncodeMinKeyForOrigin(origin_identifier);
  const std::string stop_key =
      DatabaseNameKey::EncodeStopKeyForOrigin(origin_identifier);

  DCHECK(names_and_versions->empty());
  std::unique_ptr<TransactionalLevelDBIterator> it =
      CreateIterator(db_or_transaction);
  Status s;
  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next()) {
    // Decode database name (in iterator key).
    StringPiece slice(it->Key());
    DatabaseNameKey database_name_key;
    if (!DatabaseNameKey::Decode(&slice, &database_name_key) ||
        !slice.empty()) {
      // TODO(dmurph): Change UMA name to ReadDatabaseNamesAndVersionsInternal.
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_DATABASE_NAMES);
      continue;
    }

    // Decode database id (in iterator value).
    int64_t database_id = 0;
    StringPiece value_slice(it->Value());
    if (!DecodeInt(&value_slice, &database_id) || !value_slice.empty()) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_DATABASE_NAMES);
      continue;
    }

    // Look up version by id.
    bool found = false;
    int64_t database_version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
    s = GetVarInt(db_or_transaction,
                  DatabaseMetaDataKey::Encode(
                      database_id, DatabaseMetaDataKey::USER_VERSION),
                  &database_version, &found);
    if (!s.ok() || !found) {
      INTERNAL_READ_ERROR(GET_DATABASE_NAMES);
      continue;
    }

    // Ignore stale metadata from failed initial opens.
    if (database_version != IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
      names_and_versions->push_back(blink::mojom::IDBNameAndVersion::New(
          database_name_key.database_name(), database_version));
    }
  }
  if (!s.ok())
    INTERNAL_READ_ERROR(GET_DATABASE_NAMES);

  return s;
}

// TODO(jsbell): This should do some error handling rather than
// plowing ahead when bad data is encountered.
template <typename DatabaseOrTransaction>
Status ReadMetadataForDatabaseNameInternal(
    DatabaseOrTransaction* db_or_transaction,
    const std::string& origin_identifier,
    const base::string16& name,
    IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  IDB_TRACE("IndexedDBMetadataCoding::ReadMetadataForDatabaseName");
  const std::string key = DatabaseNameKey::Encode(origin_identifier, name);
  *found = false;

  Status s = GetInt(db_or_transaction, key, &metadata->id, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found)
    return Status::OK();

  s = GetVarInt(db_or_transaction,
                DatabaseMetaDataKey::Encode(metadata->id,
                                            DatabaseMetaDataKey::USER_VERSION),
                &metadata->version, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  if (metadata->version == IndexedDBDatabaseMetadata::DEFAULT_VERSION)
    metadata->version = IndexedDBDatabaseMetadata::NO_VERSION;

  s = indexed_db::GetMaxObjectStoreId(db_or_transaction, metadata->id,
                                      &metadata->max_object_store_id);
  if (!s.ok())
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);

  // We don't cache this, we just check it if it's there.
  int64_t blob_key_generator_current_number =
      DatabaseMetaDataKey::kInvalidBlobKey;

  s = GetVarInt(
      db_or_transaction,
      DatabaseMetaDataKey::Encode(
          metadata->id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER),
      &blob_key_generator_current_number, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    // This database predates blob support.
    *found = true;
  } else if (!DatabaseMetaDataKey::IsValidBlobKey(
                 blob_key_generator_current_number)) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  s = ReadObjectStores(db_or_transaction, metadata->id,
                       &metadata->object_stores);

  return s;
}
}  // namespace

IndexedDBMetadataCoding::IndexedDBMetadataCoding() = default;
IndexedDBMetadataCoding::~IndexedDBMetadataCoding() = default;

Status IndexedDBMetadataCoding::ReadDatabaseNamesAndVersions(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions) {
  return ReadDatabaseNamesAndVersionsInternal(db, origin_identifier,
                                              names_and_versions);
}

Status IndexedDBMetadataCoding::ReadDatabaseNames(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    std::vector<base::string16>* names) {
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  Status s = ReadDatabaseNamesAndVersionsInternal(db, origin_identifier,
                                                  &names_and_versions);
  for (const blink::mojom::IDBNameAndVersionPtr& nav : names_and_versions) {
    names->push_back(nav->name);
  }
  return s;
}

Status IndexedDBMetadataCoding::ReadDatabaseNames(
    TransactionalLevelDBTransaction* transaction,
    const std::string& origin_identifier,
    std::vector<base::string16>* names) {
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  Status s = ReadDatabaseNamesAndVersionsInternal(
      transaction, origin_identifier, &names_and_versions);
  for (const blink::mojom::IDBNameAndVersionPtr& nav : names_and_versions) {
    names->push_back(nav->name);
  }
  return s;
}

Status IndexedDBMetadataCoding::ReadMetadataForDatabaseName(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    const base::string16& name,
    IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  return ReadMetadataForDatabaseNameInternal(db, origin_identifier, name,
                                             metadata, found);
}

Status IndexedDBMetadataCoding::ReadMetadataForDatabaseName(
    TransactionalLevelDBTransaction* transaction,
    const std::string& origin_identifier,
    const base::string16& name,
    IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  return ReadMetadataForDatabaseNameInternal(transaction, origin_identifier,
                                             name, metadata, found);
}

Status IndexedDBMetadataCoding::CreateDatabase(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    const base::string16& name,
    int64_t version,
    IndexedDBDatabaseMetadata* metadata) {
  // TODO(jsbell): Don't persist metadata if open fails. http://crbug.com/395472
  std::unique_ptr<LevelDBDirectTransaction> transaction =
      db->class_factory()->CreateLevelDBDirectTransaction(db);

  int64_t row_id = 0;
  Status s = indexed_db::GetNewDatabaseId(transaction.get(), &row_id);
  if (!s.ok())
    return s;
  DCHECK_GE(row_id, 0);

  if (version == IndexedDBDatabaseMetadata::NO_VERSION)
    version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;

  s = PutInt(transaction.get(),
             DatabaseNameKey::Encode(origin_identifier, name), row_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(CREATE_IDBDATABASE_METADATA);
    return s;
  }
  s = PutVarInt(
      transaction.get(),
      DatabaseMetaDataKey::Encode(row_id, DatabaseMetaDataKey::USER_VERSION),
      version);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(CREATE_IDBDATABASE_METADATA);
    return s;
  }
  s = PutVarInt(
      transaction.get(),
      DatabaseMetaDataKey::Encode(
          row_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER),
      DatabaseMetaDataKey::kBlobKeyGeneratorInitialNumber);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(CREATE_IDBDATABASE_METADATA);
    return s;
  }

  s = transaction->Commit();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(CREATE_IDBDATABASE_METADATA);
    return s;
  }

  // Note: |version| is not stored on purpose.
  metadata->name = name;
  metadata->id = row_id;
  metadata->max_object_store_id = 0;

  return s;
}

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
    const base::string16& name,
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
    base::string16 name,
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

  base::string16 object_store_name;
  bool found = false;
  Status s =
      GetString(transaction,
                ObjectStoreMetaDataKey::Encode(database_id, object_store.id,
                                               ObjectStoreMetaDataKey::NAME),
                &object_store_name, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(DELETE_OBJECT_STORE);
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
      INTERNAL_WRITE_ERROR_UNTESTED(DELETE_OBJECT_STORE);
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
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_OBJECT_STORE);
  return s;
}

Status IndexedDBMetadataCoding::RenameObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    base::string16 new_name,
    base::string16* old_name,
    IndexedDBObjectStoreMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, metadata->id))
    return InvalidDBKeyStatus();

  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, metadata->id, ObjectStoreMetaDataKey::NAME);
  const std::string new_names_key =
      ObjectStoreNamesKey::Encode(database_id, new_name);

  base::string16 old_name_check;
  bool found = false;
  Status s = GetString(transaction, name_key, &old_name_check, &found);
  // TODO(dmurph): Change DELETE_OBJECT_STORE to RENAME_OBJECT_STORE & fix UMA.
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found || old_name_check != metadata->name) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }
  const std::string old_names_key =
      ObjectStoreNamesKey::Encode(database_id, metadata->name);

  s = PutString(transaction, name_key, new_name);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }
  s = PutInt(transaction, new_names_key, metadata->id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
    return s;
  }
  s = transaction->Remove(old_names_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(DELETE_OBJECT_STORE);
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
    base::string16 name,
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
    base::string16 new_name,
    base::string16* old_name,
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
