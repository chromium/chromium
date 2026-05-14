// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_INDEX_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_INDEX_WRITER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/status.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

namespace content::indexed_db {

class Transaction;

struct IndexWriterError {
  enum class Type {
    kInvalidKey,
    kBackingStoreError,
    kConstraintError,
  };
  Type type;
  std::u16string message;
};

class IndexWriter {
 public:
  IndexWriter(const blink::IndexedDBIndexMetadata& index_metadata,
              std::vector<blink::IndexedDBKey> keys);

  [[nodiscard]] base::expected<void, IndexWriterError> VerifyIndexKeys(
      BackingStore::Transaction* transaction,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& primary_key) const;

  Status WriteIndexKeys(const BackingStore::RecordIdentifier& record,
                        BackingStore::Transaction* transaction,
                        int64_t object_store_id) const;

  IndexWriter(const IndexWriter&) = delete;
  IndexWriter& operator=(const IndexWriter&) = delete;

  ~IndexWriter();

 private:
  [[nodiscard]] StatusOr<bool> AddingKeyAllowed(
      BackingStore::Transaction* transaction,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& index_key,
      const blink::IndexedDBKey& primary_key) const;

  const blink::IndexedDBIndexMetadata index_metadata_;
  const std::vector<blink::IndexedDBKey> keys_;
};

[[nodiscard]] base::expected<std::vector<std::unique_ptr<IndexWriter>>,
                             IndexWriterError>
MakeIndexWriters(Transaction* transaction,
                 const blink::IndexedDBObjectStoreMetadata& metadata,
                 const blink::IndexedDBKey& primary_key,
                 bool key_was_generated,
                 std::vector<blink::IndexedDBIndexKeys> index_keys);

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_INDEX_WRITER_H_
