// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_cursor_impl.h"

#include <memory>
#include <utility>

#include "base/types/expected.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/instance/sqlite/record_iterator.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

content::indexed_db::sqlite::BackingStoreCursorImpl::BackingStoreCursorImpl(
    std::unique_ptr<RecordIterator> iterator,
    std::unique_ptr<Record> initial_record)
    : iterator_(std::move(iterator)),
      current_record_(std::move(initial_record)) {}

BackingStoreCursorImpl::~BackingStoreCursorImpl() = default;

const blink::IndexedDBKey& BackingStoreCursorImpl::GetKey() const {
  return current_record_->key();
}

blink::IndexedDBKey BackingStoreCursorImpl::TakeKey() && {
  return std::move(current_record_->key());
}

const blink::IndexedDBKey& BackingStoreCursorImpl::GetPrimaryKey() const {
  return current_record_->primary_key();
}

IndexedDBValue& BackingStoreCursorImpl::GetValue() {
  return current_record_->value();
}

std::unique_ptr<BackingStore::Cursor> BackingStoreCursorImpl::Clone() const {
  // This is needed by `Cursor::PrefetchIterationOperation()`.
  // TODO(crbug.com/419208481): Implement prefetch without using `Clone()`.
  return nullptr;
}

StatusOr<bool> BackingStoreCursorImpl::Continue() {
  return Advance(1);
}

StatusOr<bool> BackingStoreCursorImpl::Continue(
    const blink::IndexedDBKey& key,
    const blink::IndexedDBKey& primary_key) {
  return iterator_->Iterate(key, primary_key)
      .transform([this](std::unique_ptr<Record> new_record) {
        current_record_ = std::move(new_record);
        return current_record_ != nullptr;
      });
}

StatusOr<bool> BackingStoreCursorImpl::Advance(uint32_t count) {
  return iterator_->Iterate(count).transform(
      [this](std::unique_ptr<Record> new_record) {
        current_record_ = std::move(new_record);
        return current_record_ != nullptr;
      });
}

}  // namespace content::indexed_db::sqlite
