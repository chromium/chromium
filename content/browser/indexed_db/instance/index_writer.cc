// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/index_writer.h"

#include <stddef.h>

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBObjectStoreMetadata;

namespace content::indexed_db {

IndexWriter::IndexWriter(const IndexedDBIndexMetadata& index_metadata)
    : index_metadata_(index_metadata) {}

IndexWriter::IndexWriter(const IndexedDBIndexMetadata& index_metadata,
                         const std::vector<IndexedDBKey>& keys)
    : index_metadata_(index_metadata), keys_(keys) {}

IndexWriter::~IndexWriter() {}

bool IndexWriter::VerifyIndexKeys(BackingStore* backing_store,
                                  BackingStore::Transaction* transaction,
                                  int64_t database_id,
                                  int64_t object_store_id,
                                  int64_t index_id,
                                  bool* can_add_keys,
                                  const IndexedDBKey& primary_key,
                                  std::string* error_message) const {
  *can_add_keys = false;
  for (const auto& key : keys_) {
    bool ok = AddingKeyAllowed(backing_store, transaction, database_id,
                               object_store_id, index_id, key, primary_key,
                               can_add_keys);
    if (!ok) {
      return false;
    }
    if (!*can_add_keys) {
      if (error_message) {
        *error_message = "Unable to add key to index '" +
                         base::UTF16ToUTF8(index_metadata_.name) +
                         "': at least one key does not satisfy the uniqueness "
                         "requirements.";
      }
      return true;
    }
  }
  *can_add_keys = true;
  return true;
}

Status IndexWriter::WriteIndexKeys(
    const BackingStore::RecordIdentifier& record_identifier,
    BackingStore* backing_store,
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id) const {
  int64_t index_id = index_metadata_.id;
  for (const auto& key : keys_) {
    Status s = backing_store->PutIndexDataForRecord(transaction, database_id,
                                                    object_store_id, index_id,
                                                    key, record_identifier);
    if (!s.ok()) {
      return s;
    }
  }
  return Status::OK();
}

bool IndexWriter::AddingKeyAllowed(BackingStore* backing_store,
                                   BackingStore::Transaction* transaction,
                                   int64_t database_id,
                                   int64_t object_store_id,
                                   int64_t index_id,
                                   const IndexedDBKey& index_key,
                                   const IndexedDBKey& primary_key,
                                   bool* allowed) const {
  *allowed = false;
  if (!index_metadata_.unique) {
    *allowed = true;
    return true;
  }

  std::unique_ptr<IndexedDBKey> found_primary_key;
  bool found = false;
  Status s = backing_store->KeyExistsInIndex(
      transaction, database_id, object_store_id, index_id, index_key,
      &found_primary_key, &found);
  if (!s.ok()) {
    return false;
  }
  if (!found ||
      (primary_key.IsValid() && found_primary_key->Equals(primary_key))) {
    *allowed = true;
  }
  return true;
}

bool MakeIndexWriters(Transaction* transaction,
                      BackingStore* backing_store,
                      int64_t database_id,
                      const IndexedDBObjectStoreMetadata& object_store,
                      const IndexedDBKey& primary_key,  // makes a copy
                      bool key_was_generated,
                      const std::vector<IndexedDBIndexKeys>& index_keys,
                      std::vector<std::unique_ptr<IndexWriter>>* index_writers,
                      std::string* error_message,
                      bool* completed) {
  *completed = false;

  for (const auto& it : index_keys) {
    auto found = object_store.indexes.find(it.id);
    if (found == object_store.indexes.end()) {
      continue;
    }
    const IndexedDBIndexMetadata& index = found->second;
    // A copy is made because additional keys may be added.
    std::vector<IndexedDBKey> keys = it.keys;

    // If the object_store is using a key generator to produce the primary key,
    // and the store uses in-line keys, index key paths may reference it.
    if (key_was_generated && !object_store.key_path.IsNull()) {
      if (index.key_path == object_store.key_path) {
        // The index key path is the same as the store's key path - no index key
        // will have been sent by the front end, so synthesize one here.
        keys.push_back(primary_key);

      } else if (index.key_path.type() == blink::mojom::IDBKeyPathType::Array) {
        // An index with compound keys for a store with a key generator and
        // in-line keys may need subkeys filled in. These are represented as
        // "holes", which are not otherwise allowed.
        for (size_t i = 0; i < keys.size(); ++i) {
          if (keys[i].HasHoles()) {
            keys[i] = keys[i].FillHoles(primary_key);
          }
        }
      }
    }

    std::unique_ptr<IndexWriter> index_writer(
        std::make_unique<IndexWriter>(index, std::move(keys)));
    bool can_add_keys = false;
    bool backing_store_success = index_writer->VerifyIndexKeys(
        backing_store, transaction->BackingStoreTransaction(), database_id,
        object_store.id, index.id, &can_add_keys, primary_key, error_message);
    if (!backing_store_success) {
      return false;
    }
    if (!can_add_keys) {
      return true;
    }

    index_writers->push_back(std::move(index_writer));
  }

  *completed = true;
  return true;
}

}  // namespace content::indexed_db
