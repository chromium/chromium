// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_RECORD_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_RECORD_H_

#include "content/browser/indexed_db/indexed_db_value.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace content::indexed_db {

// Base class for an IndexedDB record.
class Record {
 public:
  virtual ~Record() = default;

  virtual blink::IndexedDBKey& key() = 0;
  virtual blink::IndexedDBKey& primary_key() = 0;
  virtual IndexedDBValue& value() = 0;
};

// An object store record has a key and value, with `primary_key()` being the
// key itself.
class ObjectStoreRecord : public Record {
 public:
  ObjectStoreRecord(blink::IndexedDBKey key, IndexedDBValue value);
  ~ObjectStoreRecord() override;

  blink::IndexedDBKey& key() override;
  blink::IndexedDBKey& primary_key() override;
  IndexedDBValue& value() override;

 private:
  blink::IndexedDBKey key_;
  IndexedDBValue value_;
};

// It is an error to call `value()` on this type of record.
class ObjectStoreKeyOnlyRecord : public ObjectStoreRecord {
 public:
  explicit ObjectStoreKeyOnlyRecord(blink::IndexedDBKey key);
  ~ObjectStoreKeyOnlyRecord() override;

  IndexedDBValue& value() override;
};

class IndexRecord : public Record {
 public:
  IndexRecord(blink::IndexedDBKey key,
              blink::IndexedDBKey primary_key,
              IndexedDBValue value);
  ~IndexRecord() override;

  blink::IndexedDBKey& key() override;
  blink::IndexedDBKey& primary_key() override;
  IndexedDBValue& value() override;

 private:
  blink::IndexedDBKey key_;
  blink::IndexedDBKey primary_key_;
  IndexedDBValue value_;
};

// It is an error to call `value()` on this type of record.
class IndexKeyOnlyRecord : public IndexRecord {
 public:
  explicit IndexKeyOnlyRecord(blink::IndexedDBKey key,
                              blink::IndexedDBKey primary_key);
  ~IndexKeyOnlyRecord() override;

  IndexedDBValue& value() override;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_RECORD_H_
