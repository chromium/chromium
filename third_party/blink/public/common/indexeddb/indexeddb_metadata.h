// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_METADATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_METADATA_H_

#include <stdint.h>

#include <map>
#include <string>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

namespace blink {

struct BLINK_COMMON_EXPORT IndexedDBIndexMetadata {
  inline static const int64_t kInvalidId = -1;

  IndexedDBIndexMetadata();
  IndexedDBIndexMetadata(const std::u16string& name,
                         int64_t id,
                         const blink::IndexedDBKeyPath& key_path,
                         bool unique,
                         bool multi_entry);
  IndexedDBIndexMetadata(const IndexedDBIndexMetadata& other);
  IndexedDBIndexMetadata(IndexedDBIndexMetadata&& other);
  ~IndexedDBIndexMetadata();
  IndexedDBIndexMetadata& operator=(const IndexedDBIndexMetadata& other);
  IndexedDBIndexMetadata& operator=(IndexedDBIndexMetadata&& other);
  bool operator==(const IndexedDBIndexMetadata& other) const;

  std::u16string name;
  int64_t id = kInvalidId;
  blink::IndexedDBKeyPath key_path;
  bool unique = false;
  bool multi_entry = false;
};

struct BLINK_COMMON_EXPORT IndexedDBObjectStoreMetadata {
  inline static const int64_t kInvalidId = -1;

  IndexedDBObjectStoreMetadata();
  IndexedDBObjectStoreMetadata(const std::u16string& name,
                               int64_t id,
                               const blink::IndexedDBKeyPath& key_path,
                               bool auto_increment);
  IndexedDBObjectStoreMetadata(const IndexedDBObjectStoreMetadata& other);
  IndexedDBObjectStoreMetadata(IndexedDBObjectStoreMetadata&& other);
  ~IndexedDBObjectStoreMetadata();
  IndexedDBObjectStoreMetadata& operator=(
      const IndexedDBObjectStoreMetadata& other);
  IndexedDBObjectStoreMetadata& operator=(IndexedDBObjectStoreMetadata&& other);
  bool operator==(const IndexedDBObjectStoreMetadata& other) const;

  std::u16string name;
  int64_t id = kInvalidId;
  blink::IndexedDBKeyPath key_path;
  bool auto_increment = false;
  int64_t max_index_id = 0;

  std::map<int64_t, IndexedDBIndexMetadata> indexes;
};

struct BLINK_COMMON_EXPORT IndexedDBDatabaseMetadata {
  // TODO(jsbell): These can probably be collapsed into 0.
  enum { NO_VERSION = -1, DEFAULT_VERSION = 0 };

  IndexedDBDatabaseMetadata();
  IndexedDBDatabaseMetadata(const std::u16string& name);
  IndexedDBDatabaseMetadata(const IndexedDBDatabaseMetadata& other);
  IndexedDBDatabaseMetadata(IndexedDBDatabaseMetadata&& other);
  // TODO(estade): this is virtual because it's extended in backend code.
  // Backend code probably shouldn't be depending on Blink classes for its own
  // bookkeeping; fix this.
  virtual ~IndexedDBDatabaseMetadata();
  IndexedDBDatabaseMetadata& operator=(const IndexedDBDatabaseMetadata& other);
  IndexedDBDatabaseMetadata& operator=(IndexedDBDatabaseMetadata&& other);
  bool operator==(const IndexedDBDatabaseMetadata& other) const;

  std::u16string name;
  int64_t version = NO_VERSION;
  int64_t max_object_store_id = 0;

  std::map<int64_t, IndexedDBObjectStoreMetadata> object_stores;

  bool was_cold_open = true;
  bool is_sqlite = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_METADATA_H_
