// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_area_map.h"

namespace blink {

namespace {

// For quota purposes we count each character as 2 bytes.
size_t QuotaForString(const String& s) {
  return s.length() * sizeof(UChar);
}

size_t MemoryForString(const String& s) {
  return s.CharactersSizeInBytes();
}

}  // namespace

StorageAreaMap::StorageAreaMap(size_t quota) : quota_(quota) {
  ResetKeyIterator();
}

unsigned StorageAreaMap::GetLength() const {
  return keys_values_.size();
}

String StorageAreaMap::GetKey(unsigned index) const {
  if (index >= GetLength())
    return String();

  // Decide if we should leave |key_iterator_| alone, or reset to either the
  // beginning or end of the map for shortest iteration distance.
  const unsigned distance_to_current = index > last_key_index_
                                           ? index - last_key_index_
                                           : last_key_index_ - index;
  const unsigned distance_to_end = GetLength() - index;
  if (index < distance_to_current && index < distance_to_end) {
    // Distance from start is shortest, so reset iterator to begin.
    last_key_index_ = 0;
    key_iterator_ = keys_values_.begin();
  } else if (distance_to_end < distance_to_current && distance_to_end < index) {
    // Distance from end is shortest, so reset iterator to end.
    last_key_index_ = GetLength();
    key_iterator_ = keys_values_.end();
  }

  while (last_key_index_ < index) {
    ++key_iterator_;
    ++last_key_index_;
  }
  while (last_key_index_ > index) {
    --key_iterator_;
    --last_key_index_;
  }
  return key_iterator_->key;
}

String StorageAreaMap::GetItem(const String& key) const {
  auto it = keys_values_.find(key);
  if (it == keys_values_.end())
    return String();
  return it->value;
}

bool StorageAreaMap::SetItem(const String& key,
                             const String& value,
                             String* old_value) {
  return SetItemInternal(key, value, old_value, true);
}

void StorageAreaMap::SetItemIgnoringQuota(const String& key,
                                          const String& value) {
  SetItemInternal(key, value, nullptr, false);
}

bool StorageAreaMap::RemoveItem(const String& key, String* old_value) {
  const auto it = keys_values_.find(key);
  if (it == keys_values_.end())
    return false;
  quota_used_ -= QuotaForString(key) + QuotaForString(it->value);
  memory_used_ -= MemoryForString(key) + MemoryForString(it->value);
  if (old_value)
    *old_value = it->value;
  keys_values_.erase(it);
  ResetKeyIterator();
  return true;
}

void StorageAreaMap::ResetKeyIterator() const {
  key_iterator_ = keys_values_.begin();
  last_key_index_ = 0;
}

bool StorageAreaMap::SetItemInternal(const String& key,
                                     const String& value,
                                     String* old_value,
                                     bool check_quota) {
  const auto it = keys_values_.find(key);
  size_t old_item_size = 0;
  size_t old_item_memory = 0;
  if (it != keys_values_.end()) {
    old_item_size = QuotaForString(key) + QuotaForString(it->value);
    old_item_memory = MemoryForString(key) + MemoryForString(it->value);
    if (old_value)
      *old_value = it->value;
  }
  DCHECK_GE(quota_used_, old_item_size);
  size_t new_item_size = QuotaForString(key) + QuotaForString(value);
  size_t new_item_memory = MemoryForString(key) + MemoryForString(value);
  size_t new_quota_used = quota_used_ - old_item_size + new_item_size;
  size_t new_memory_used = memory_used_ - old_item_memory + new_item_memory;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing files that are over budget.
  if (check_quota && new_item_size > old_item_size && new_quota_used > quota_)
    return false;

  keys_values_.Set(key, value);
  ResetKeyIterator();
  quota_used_ = new_quota_used;
  memory_used_ = new_memory_used;
  return true;
}

}  // namespace blink
