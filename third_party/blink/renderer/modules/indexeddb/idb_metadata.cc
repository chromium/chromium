// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"

#include <utility>

namespace blink {

constexpr int64_t IDBIndexMetadata::kInvalidId;

constexpr int64_t IDBObjectStoreMetadata::kInvalidId;

IDBIndexMetadata::IDBIndexMetadata() = default;

IDBIndexMetadata::IDBIndexMetadata(const String& name,
                                   int64_t id,
                                   const IDBKeyPath& key_path,
                                   bool unique,
                                   bool multi_entry)
    : name(name),
      id(id),
      key_path(key_path),
      unique(unique),
      multi_entry(multi_entry) {}

// static
scoped_refptr<IDBIndexMetadata> IDBIndexMetadata::Create() {
  return base::AdoptRef(new IDBIndexMetadata());
}

IDBObjectStoreMetadata::IDBObjectStoreMetadata() = default;

IDBObjectStoreMetadata::IDBObjectStoreMetadata(const String& name,
                                               int64_t id,
                                               const IDBKeyPath& key_path,
                                               bool auto_increment,
                                               int64_t max_index_id)
    : name(name),
      id(id),
      key_path(key_path),
      auto_increment(auto_increment),
      max_index_id(max_index_id) {}

// static
scoped_refptr<IDBObjectStoreMetadata> IDBObjectStoreMetadata::Create() {
  return base::AdoptRef(new IDBObjectStoreMetadata());
}

scoped_refptr<IDBObjectStoreMetadata> IDBObjectStoreMetadata::CreateCopy()
    const {
  scoped_refptr<IDBObjectStoreMetadata> copy =
      base::AdoptRef(new IDBObjectStoreMetadata(name, id, key_path,
                                                auto_increment, max_index_id));

  for (const auto& it : indexes) {
    IDBIndexMetadata* index = it.value.get();
    scoped_refptr<IDBIndexMetadata> index_copy = base::AdoptRef(
        new IDBIndexMetadata(index->name, index->id, index->key_path,
                             index->unique, index->multi_entry));
    copy->indexes.insert(it.key, std::move(index_copy));
  }
  return copy;
}

IDBDatabaseMetadata::IDBDatabaseMetadata()
    : version(IDBDatabaseMetadata::kNoVersion) {}

IDBDatabaseMetadata::IDBDatabaseMetadata(const String& name,
                                         int64_t id,
                                         int64_t version,
                                         int64_t max_object_store_id,
                                         bool was_cold_open)
    : name(name),
      id(id),
      version(version),
      max_object_store_id(max_object_store_id),
      was_cold_open(was_cold_open) {}

void IDBDatabaseMetadata::CopyFrom(const IDBDatabaseMetadata& metadata) {
  name = metadata.name;
  id = metadata.id;
  version = metadata.version;
  max_object_store_id = metadata.max_object_store_id;
  was_cold_open = metadata.was_cold_open;
}

}  // namespace blink
