// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/index_writer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/types/expected_macros.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/status.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBObjectStoreMetadata;

namespace content::indexed_db {

using Type = IndexWriterError::Type;

IndexWriter::IndexWriter(const IndexedDBIndexMetadata& index_metadata,
                         std::vector<IndexedDBKey> keys)
    : index_metadata_(index_metadata), keys_(std::move(keys)) {}

IndexWriter::~IndexWriter() = default;

base::expected<void, IndexWriterError> IndexWriter::VerifyIndexKeys(
    BackingStore::Transaction* transaction,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& primary_key) const {
  for (const IndexedDBKey& key : keys_) {
    if (!key.IsValid()) {
      return base::unexpected(IndexWriterError{Type::kInvalidKey});
    }
    ASSIGN_OR_RETURN(
        bool can_add_key,
        AddingKeyAllowed(transaction, object_store_id, index_id, key,
                         primary_key),
        [](Status) { return IndexWriterError{Type::kBackingStoreError}; });
    if (!can_add_key) {
      return base::unexpected(IndexWriterError{
          Type::kConstraintError,
          base::StrCat({u"Unable to add key to index '", index_metadata_.name,
                        u"': at least one key does not satisfy the uniqueness "
                        u"requirements."})});
    }
  }
  return {};
}

Status IndexWriter::WriteIndexKeys(
    const BackingStore::RecordIdentifier& record_identifier,
    BackingStore::Transaction* transaction,
    int64_t object_store_id) const {
  int64_t index_id = index_metadata_.id;
  for (const IndexedDBKey& key : keys_) {
    IDB_RETURN_IF_ERROR(transaction->PutIndexDataForRecord(
        object_store_id, index_id, key, record_identifier));
  }
  return Status::OK();
}

StatusOr<bool> IndexWriter::AddingKeyAllowed(
    BackingStore::Transaction* transaction,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& index_key,
    const IndexedDBKey& primary_key) const {
  if (!index_metadata_.unique) {
    return true;
  }
  ASSIGN_OR_RETURN(IndexedDBKey found_primary_key,
                   transaction->GetFirstPrimaryKeyForIndexKey(
                       object_store_id, index_id, index_key));
  return !found_primary_key.IsValid() ||
         (primary_key.IsValid() && found_primary_key.Equals(primary_key));
}

base::expected<std::vector<std::unique_ptr<IndexWriter>>, IndexWriterError>
MakeIndexWriters(Transaction* transaction,
                 const IndexedDBObjectStoreMetadata& object_store,
                 const IndexedDBKey& primary_key,
                 bool key_was_generated,
                 std::vector<IndexedDBIndexKeys> index_keys) {
  std::vector<std::unique_ptr<IndexWriter>> index_writers;

  for (IndexedDBIndexKeys& it : index_keys) {
    const IndexedDBIndexMetadata& index = object_store.indexes.at(it.id);

    // If the object_store is using a key generator to produce the primary key,
    // and the store uses in-line keys, index key paths may reference it.
    if (key_was_generated && !object_store.key_path.IsNull()) {
      if (index.key_path == object_store.key_path) {
        // The index key path is the same as the store's key path - no index key
        // will have been sent by the front end, so synthesize one here.
        it.keys.emplace_back(primary_key.Clone());
      } else if (index.key_path.type() == blink::mojom::IDBKeyPathType::Array) {
        // An index with compound keys for a store with a key generator and
        // in-line keys may need subkeys filled in. These are represented as
        // "holes", which are not otherwise allowed.
        for (IndexedDBKey& key : it.keys) {
          if (key.HasHoles()) {
            key = key.FillHoles(primary_key);
          }
        }
      }
    }

    auto index_writer =
        std::make_unique<IndexWriter>(index, std::move(it.keys));
    RETURN_IF_ERROR(
        index_writer->VerifyIndexKeys(transaction->BackingStoreTransaction(),
                                      object_store.id, index.id, primary_key));
    index_writers.emplace_back(std::move(index_writer));
  }

  return index_writers;
}

}  // namespace content::indexed_db
