// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"
#include "content/browser/indexed_db/instance/transaction.h"

namespace content::indexed_db {

struct IndexedDBValue;

// NB: This interface is a WIP and is expected to experience heavy churn in the
// near future as additional interfaces are introduced to appropriately abstract
// the different database engines. See crbug.com/40273263.
//
// This interface abstracts the implementation of the data store for IDB data
// for a single bucket (which may contain many IDB databases). The current
// complete implementation uses LevelDB as its engine, along with a bespoke
// flat-file store for blobs, and is called level_db::BackingStore. In this
// system, each BackingStore corresponds to one LevelDB database.
//
// The SQLite version is a work in progress. There, each IDBDatabase correlates
// to a different database, so the BackingStore uses a collection of .sql
// files.
//
// Many of the methods here are likely to be moved to sibling interfaces that
// better encapsulate semantic objects. Eventually, all access to the
// BackingStore should be routed through this interface or its siblings rather
// than reaching directly into level_db::BackingStore.
class BackingStore {
 public:
  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier() = default;
    RecordIdentifier(std::string primary_key, int64_t version)
        : primary_key_(std::move(primary_key)), version_(version) {
      DCHECK(!primary_key_.empty());
    }

    RecordIdentifier(const RecordIdentifier&) = delete;
    RecordIdentifier& operator=(const RecordIdentifier&) = delete;

    ~RecordIdentifier() = default;

    const std::string& primary_key() const { return primary_key_; }
    int64_t version() const { return version_; }

    void Reset(std::string primary_key, int64_t version) {
      primary_key_ = std::move(primary_key);
      version_ = version;
    }

   private:
    // TODO(jsbell): Make it more clear that this is the *encoded* version of
    // the key.
    std::string primary_key_;
    int64_t version_ = -1;
  };

  virtual ~BackingStore() = default;

  // Get tasks to be run after a BackingStore no longer has any connections.
  virtual std::list<
      std::unique_ptr<BackingStorePreCloseTaskQueue::PreCloseTask>>
  GetPreCloseTasks() = 0;
  virtual void TearDown(base::WaitableEvent* signal_on_destruction) = 0;
  // Fill in the provided list with existing database names.
  [[nodiscard]] virtual Status GetDatabaseNames(
      std::vector<std::u16string>* names) = 0;
  // Fill in the provided list with existing database names and versions.
  [[nodiscard]] virtual Status GetDatabaseNamesAndVersions(
      std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions) = 0;
  // Creates a new database in the backing store. `metadata` is an in-out param.
  // The `name` and `version` fields are inputs, while the `id` and
  // `max_object_store_id` fields are outputs.
  [[nodiscard]] virtual Status CreateDatabase(
      blink::IndexedDBDatabaseMetadata& metadata) = 0;
  [[nodiscard]] virtual Status DeleteDatabase(
      const std::u16string& name,
      std::vector<PartitionedLock> locks,
      base::OnceClosure on_complete) = 0;
  // Reads in metadata for the database and all object stores & indices.
  // Note: the database name is not populated in |metadata|. Virtual for
  // testing.
  [[nodiscard]] virtual Status ReadMetadataForDatabaseName(
      const std::u16string& name,
      blink::IndexedDBDatabaseMetadata* metadata,
      bool* found) = 0;
  virtual std::unique_ptr<Transaction::Delegate> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode) = 0;
  // TODO(crbug.com/40273263): move these transaction-scoped operations to a
  // separate interface.
  // Changes the database version to |version|.
  [[nodiscard]] virtual Status SetDatabaseVersion(
      Transaction::Delegate* transaction,
      int64_t row_id,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata) = 0;
  [[nodiscard]] virtual Status CreateObjectStore(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      std::u16string name,
      blink::IndexedDBKeyPath key_path,
      bool auto_increment,
      blink::IndexedDBObjectStoreMetadata* metadata) = 0;
  [[nodiscard]] virtual Status DeleteObjectStore(
      Transaction::Delegate* transaction,
      int64_t database_id,
      const blink::IndexedDBObjectStoreMetadata& object_store) = 0;
  [[nodiscard]] virtual Status RenameObjectStore(
      Transaction::Delegate* transaction,
      int64_t database_id,
      std::u16string new_name,
      std::u16string* old_name,
      blink::IndexedDBObjectStoreMetadata* metadata) = 0;

  // Creates a new index metadata and writes it to the transaction.
  [[nodiscard]] virtual Status CreateIndex(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      std::u16string name,
      blink::IndexedDBKeyPath key_path,
      bool is_unique,
      bool is_multi_entry,
      blink::IndexedDBIndexMetadata* metadata) = 0;
  // Deletes the index metadata on the transaction (but not any index entries).
  [[nodiscard]] virtual Status DeleteIndex(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBIndexMetadata& metadata) = 0;
  // Renames the given index and writes it to the transaction.
  [[nodiscard]] virtual Status RenameIndex(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      std::u16string new_name,
      std::u16string* old_name,
      blink::IndexedDBIndexMetadata* metadata) = 0;
  [[nodiscard]] virtual Status GetRecord(Transaction::Delegate* transaction,
                                         int64_t database_id,
                                         int64_t object_store_id,
                                         const blink::IndexedDBKey& key,
                                         IndexedDBValue* record) = 0;
  [[nodiscard]] virtual Status PutRecord(Transaction::Delegate* transaction,
                                         int64_t database_id,
                                         int64_t object_store_id,
                                         const blink::IndexedDBKey& key,
                                         IndexedDBValue* value,
                                         RecordIdentifier* record) = 0;
  [[nodiscard]] virtual Status ClearObjectStore(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id) = 0;
  [[nodiscard]] virtual Status DeleteRecord(Transaction::Delegate* transaction,
                                            int64_t database_id,
                                            int64_t object_store_id,
                                            const RecordIdentifier& record) = 0;
  [[nodiscard]] virtual Status DeleteRange(Transaction::Delegate* transaction,
                                           int64_t database_id,
                                           int64_t object_store_id,
                                           const blink::IndexedDBKeyRange&) = 0;
  [[nodiscard]] virtual Status GetKeyGeneratorCurrentNumber(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number) = 0;
  [[nodiscard]] virtual Status MaybeUpdateKeyGeneratorCurrentNumber(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_state,
      bool check_current) = 0;
  [[nodiscard]] virtual Status KeyExistsInObjectStore(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found) = 0;
  [[nodiscard]] virtual Status ClearIndex(Transaction::Delegate* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id,
                                          int64_t index_id) = 0;
  [[nodiscard]] virtual Status PutIndexDataForRecord(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const RecordIdentifier& record) = 0;
  [[nodiscard]] virtual Status GetPrimaryKeyViaIndex(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* primary_key) = 0;
  [[nodiscard]] virtual Status KeyExistsInIndex(
      Transaction::Delegate* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
      bool* exists) = 0;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_H_
