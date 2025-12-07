// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/record.h"

#include <utility>

#include "base/notreached.h"

namespace content::indexed_db {

ObjectStoreRecord::ObjectStoreRecord(blink::IndexedDBKey key,
                                     IndexedDBValue value)
    : key_(std::move(key)), value_(std::move(value)) {}

ObjectStoreRecord::~ObjectStoreRecord() = default;

blink::IndexedDBKey& ObjectStoreRecord::key() {
  return key_;
}

blink::IndexedDBKey& ObjectStoreRecord::primary_key() {
  return key_;
}

IndexedDBValue& ObjectStoreRecord::value() {
  return value_;
}

ObjectStoreKeyOnlyRecord::ObjectStoreKeyOnlyRecord(blink::IndexedDBKey key)
    : ObjectStoreRecord(std::move(key), /*value=*/{}) {}

ObjectStoreKeyOnlyRecord::~ObjectStoreKeyOnlyRecord() = default;

IndexedDBValue& ObjectStoreKeyOnlyRecord::value() {
  NOTREACHED();
}

IndexRecord::IndexRecord(blink::IndexedDBKey key,
                         blink::IndexedDBKey primary_key,
                         IndexedDBValue value)
    : key_(std::move(key)),
      primary_key_(std::move(primary_key)),
      value_(std::move(value)) {}

IndexRecord::~IndexRecord() = default;

blink::IndexedDBKey& IndexRecord::key() {
  return key_;
}

blink::IndexedDBKey& IndexRecord::primary_key() {
  return primary_key_;
}

IndexedDBValue& IndexRecord::value() {
  return value_;
}

IndexKeyOnlyRecord::IndexKeyOnlyRecord(blink::IndexedDBKey key,
                                       blink::IndexedDBKey primary_key)
    : IndexRecord(std::move(key), std::move(primary_key), /*value=*/{}) {}

IndexKeyOnlyRecord::~IndexKeyOnlyRecord() = default;

IndexedDBValue& IndexKeyOnlyRecord::value() {
  NOTREACHED();
}

}  // namespace content::indexed_db
