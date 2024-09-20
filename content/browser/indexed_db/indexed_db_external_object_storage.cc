// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_external_object_storage.h"

#include "base/functional/callback.h"

namespace content::indexed_db {

IndexedDBExternalObjectChangeRecord::IndexedDBExternalObjectChangeRecord(
    const std::string& object_store_data_key)
    : object_store_data_key_(object_store_data_key) {}

IndexedDBExternalObjectChangeRecord::~IndexedDBExternalObjectChangeRecord() =
    default;

void IndexedDBExternalObjectChangeRecord::SetExternalObjects(
    std::vector<IndexedDBExternalObject>* external_objects) {
  external_objects_.clear();
  if (external_objects)
    external_objects_.swap(*external_objects);
}

std::unique_ptr<IndexedDBExternalObjectChangeRecord>
IndexedDBExternalObjectChangeRecord::Clone() const {
  auto record = std::make_unique<IndexedDBExternalObjectChangeRecord>(
      object_store_data_key_);
  record->external_objects_ = external_objects_;

  return record;
}

}  // namespace content::indexed_db
