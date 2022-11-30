// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace blink {
struct IndexedDBObjectStoreMetadata;
}

namespace content {

class IndexedDBTransaction;

class IndexWriter {
 public:
  explicit IndexWriter(const blink::IndexedDBIndexMetadata& index_metadata);

  IndexWriter(const blink::IndexedDBIndexMetadata& index_metadata,
              const std::vector<blink::IndexedDBKey>& keys);

  [[nodiscard]] bool VerifyIndexKeys(
      IndexedDBBackingStore* store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      bool* can_add_keys,
      const blink::IndexedDBKey& primary_key,
      std::u16string* error_message) const;

  leveldb::Status WriteIndexKeys(
      const IndexedDBBackingStore::RecordIdentifier& record,
      IndexedDBBackingStore* store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id) const;

  IndexWriter(const IndexWriter&) = delete;
  IndexWriter& operator=(const IndexWriter&) = delete;

  ~IndexWriter();

 private:
  [[nodiscard]] bool AddingKeyAllowed(
      IndexedDBBackingStore* store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& index_key,
      const blink::IndexedDBKey& primary_key,
      bool* allowed) const;

  const blink::IndexedDBIndexMetadata index_metadata_;
  const std::vector<blink::IndexedDBKey> keys_;
};

[[nodiscard]] bool MakeIndexWriters(
    IndexedDBTransaction* transaction,
    IndexedDBBackingStore* store,
    int64_t database_id,
    const blink::IndexedDBObjectStoreMetadata& metadata,
    const blink::IndexedDBKey& primary_key,
    bool key_was_generated,
    const std::vector<blink::IndexedDBIndexKeys>& index_keys,
    std::vector<std::unique_ptr<IndexWriter>>* index_writers,
    std::u16string* error_message,
    bool* completed);

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
