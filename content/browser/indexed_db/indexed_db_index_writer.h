// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
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

  bool VerifyIndexKeys(IndexedDBBackingStore* store,
                       IndexedDBBackingStore::Transaction* transaction,
                       int64_t database_id,
                       int64_t object_store_id,
                       int64_t index_id,
                       bool* can_add_keys,
                       const blink::IndexedDBKey& primary_key,
                       base::string16* error_message) const WARN_UNUSED_RESULT;

  leveldb::Status WriteIndexKeys(
      const IndexedDBBackingStore::RecordIdentifier& record,
      IndexedDBBackingStore* store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id) const;

  ~IndexWriter();

 private:
  bool AddingKeyAllowed(IndexedDBBackingStore* store,
                        IndexedDBBackingStore::Transaction* transaction,
                        int64_t database_id,
                        int64_t object_store_id,
                        int64_t index_id,
                        const blink::IndexedDBKey& index_key,
                        const blink::IndexedDBKey& primary_key,
                        bool* allowed) const WARN_UNUSED_RESULT;

  const blink::IndexedDBIndexMetadata index_metadata_;
  const std::vector<blink::IndexedDBKey> keys_;

  DISALLOW_COPY_AND_ASSIGN(IndexWriter);
};

bool MakeIndexWriters(IndexedDBTransaction* transaction,
                      IndexedDBBackingStore* store,
                      int64_t database_id,
                      const blink::IndexedDBObjectStoreMetadata& metadata,
                      const blink::IndexedDBKey& primary_key,
                      bool key_was_generated,
                      const std::vector<blink::IndexedDBIndexKeys>& index_keys,
                      std::vector<std::unique_ptr<IndexWriter>>* index_writers,
                      base::string16* error_message,
                      bool* completed) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
