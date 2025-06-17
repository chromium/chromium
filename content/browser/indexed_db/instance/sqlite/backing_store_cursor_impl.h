// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/browser/indexed_db/instance/backing_store.h"

namespace content::indexed_db {
class Record;

namespace sqlite {
class RecordIterator;

class BackingStoreCursorImpl : public BackingStore::Cursor {
 public:
  using PassKey = base::PassKey<BackingStoreCursorImpl>;

  BackingStoreCursorImpl(std::unique_ptr<RecordIterator> iterator,
                         std::unique_ptr<Record> initial_record);
  ~BackingStoreCursorImpl() override;

  // BackingStore::Cursor:
  const blink::IndexedDBKey& GetKey() const override;
  blink::IndexedDBKey TakeKey() && override;
  const blink::IndexedDBKey& GetPrimaryKey() const override;
  IndexedDBValue& GetValue() override;
  std::unique_ptr<Cursor> Clone() const override;
  StatusOr<bool> Continue() override;
  StatusOr<bool> Continue(const blink::IndexedDBKey& key,
                          const blink::IndexedDBKey& primary_key) override;
  StatusOr<bool> Advance(uint32_t count) override;

 private:
  std::unique_ptr<RecordIterator> iterator_;
  std::unique_ptr<Record> current_record_;
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_
