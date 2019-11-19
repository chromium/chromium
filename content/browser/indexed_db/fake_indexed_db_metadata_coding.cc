// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/fake_indexed_db_metadata_coding.h"

#include <utility>

#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBObjectStoreMetadata;
using leveldb::Status;

namespace content {

FakeIndexedDBMetadataCoding::FakeIndexedDBMetadataCoding() {}
FakeIndexedDBMetadataCoding::~FakeIndexedDBMetadataCoding() {}

leveldb::Status FakeIndexedDBMetadataCoding::ReadDatabaseNames(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    std::vector<base::string16>* names) {
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::ReadMetadataForDatabaseName(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    const base::string16& name,
    IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::CreateDatabase(
    TransactionalLevelDBDatabase* database,
    const std::string& origin_identifier,
    const base::string16& name,
    int64_t version,
    IndexedDBDatabaseMetadata* metadata) {
  metadata->name = name;
  metadata->version = version;
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::SetDatabaseVersion(
    TransactionalLevelDBTransaction* transaction,
    int64_t row_id,
    int64_t version,
    IndexedDBDatabaseMetadata* metadata) {
  metadata->version = version;
  return leveldb::Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::FindDatabaseId(
    TransactionalLevelDBDatabase* db,
    const std::string& origin_identifier,
    const base::string16& name,
    int64_t* id,
    bool* found) {
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::CreateObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    base::string16 name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment,
    IndexedDBObjectStoreMetadata* metadata) {
  metadata->name = std::move(name);
  metadata->id = object_store_id;
  metadata->key_path = std::move(key_path);
  metadata->auto_increment = auto_increment;
  metadata->max_index_id = IndexedDBObjectStoreMetadata::kMinimumIndexId;
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::RenameObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    base::string16 new_name,
    base::string16* old_name,
    IndexedDBObjectStoreMetadata* metadata) {
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::DeleteObjectStore(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    const IndexedDBObjectStoreMetadata& object_store) {
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::CreateIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    base::string16 name,
    blink::IndexedDBKeyPath key_path,
    bool is_unique,
    bool is_multi_entry,
    IndexedDBIndexMetadata* metadata) {
  metadata->id = index_id;
  metadata->name = std::move(name);
  metadata->key_path = key_path;
  metadata->unique = is_unique;
  metadata->multi_entry = is_multi_entry;
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::RenameIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    base::string16 new_name,
    base::string16* old_name,
    IndexedDBIndexMetadata* metadata) {
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

leveldb::Status FakeIndexedDBMetadataCoding::DeleteIndex(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBIndexMetadata& metadata) {
  return Status::OK();
}

}  // namespace content
