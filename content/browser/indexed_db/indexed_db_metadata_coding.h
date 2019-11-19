// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_CODING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_CODING_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBTransaction;

// Creation, reading, and modification operations for IndexedDB metadata. All
// methods that write data to disk use the given |transaction| for writing (and
// thus the writes will be committed with the transaction).
//
// This class contains no state and thus could be static, but is held as a class
// to enable subclassing for testing.
class CONTENT_EXPORT IndexedDBMetadataCoding {
 public:
  IndexedDBMetadataCoding();
  virtual ~IndexedDBMetadataCoding();

  // Reads the list of database names for the given origin.
  virtual leveldb::Status ReadDatabaseNames(
      TransactionalLevelDBDatabase* db,
      const std::string& origin_identifier,
      std::vector<base::string16>* names);
  virtual leveldb::Status ReadDatabaseNames(
      TransactionalLevelDBTransaction* transaction,
      const std::string& origin_identifier,
      std::vector<base::string16>* names);

  // Reads in the list of database names and versions for the given origin.
  virtual leveldb::Status ReadDatabaseNamesAndVersions(
      TransactionalLevelDBDatabase* db,
      const std::string& origin_identifier,
      std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions);

  // Reads in metadata for the database and all object stores & indices.
  // Note: the database name is not populated in |metadata|.
  virtual leveldb::Status ReadMetadataForDatabaseName(
      TransactionalLevelDBDatabase* db,
      const std::string& origin_identifier,
      const base::string16& name,
      blink::IndexedDBDatabaseMetadata* metadata,
      bool* found);
  virtual leveldb::Status ReadMetadataForDatabaseName(
      TransactionalLevelDBTransaction* transaction,
      const std::string& origin_identifier,
      const base::string16& name,
      blink::IndexedDBDatabaseMetadata* metadata,
      bool* found);

  // Creates a new database metadata entry and writes it to disk.
  virtual leveldb::Status CreateDatabase(
      TransactionalLevelDBDatabase* database,
      const std::string& origin_identifier,
      const base::string16& name,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata);

  // Changes the database version to |version|.
  virtual leveldb::Status SetDatabaseVersion(
      TransactionalLevelDBTransaction* transaction,
      int64_t row_id,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata) WARN_UNUSED_RESULT;

  // Reads only the database id, if found.
  virtual leveldb::Status FindDatabaseId(TransactionalLevelDBDatabase* db,
                                         const std::string& origin_identifier,
                                         const base::string16& name,
                                         int64_t* id,
                                         bool* found);

  // Creates a new object store metadata entry and writes it to the transaction.
  virtual leveldb::Status CreateObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      base::string16 name,
      blink::IndexedDBKeyPath key_path,
      bool auto_increment,
      blink::IndexedDBObjectStoreMetadata* metadata);

  // Deletes the given object store metadata on the transaction (but not any
  // data entries or blobs in the object store).
  virtual leveldb::Status DeleteObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      const blink::IndexedDBObjectStoreMetadata& object_store);

  // Renames the given object store and writes it to the transaction.
  virtual leveldb::Status RenameObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      base::string16 new_name,
      base::string16* old_name,
      blink::IndexedDBObjectStoreMetadata* metadata);

  // Creates a new index metadata and writes it to the transaction.
  virtual leveldb::Status CreateIndex(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      base::string16 name,
      blink::IndexedDBKeyPath key_path,
      bool is_unique,
      bool is_multi_entry,
      blink::IndexedDBIndexMetadata* metadata);

  // Deletes the index metadata on the transaction (but not any index entries).
  virtual leveldb::Status DeleteIndex(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBIndexMetadata& metadata);

  // Renames the given index and writes it to the transaction.
  virtual leveldb::Status RenameIndex(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      base::string16 new_name,
      base::string16* old_name,
      blink::IndexedDBIndexMetadata* metadata);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBMetadataCoding);
};

}  // namespace content
#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_CODING_H_
