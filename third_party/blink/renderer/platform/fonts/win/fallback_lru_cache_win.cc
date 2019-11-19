// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/fallback_lru_cache_win.h"

#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FallbackLruCache::FallbackLruCache(size_t max_size) : max_size_(max_size) {
  DCHECK_GT(max_size_, 0u);
}

FallbackLruCache::TypefaceVector* FallbackLruCache::Get(const String& key) {
  HashMapType::iterator find_result = map_.find(key);
  if (find_result == map_.end())
    return nullptr;

  // Move result to beginning of list.
  KeyListNode* node = find_result->value.ListNode();
  ordering_.Remove(node);
  ordering_.Push(node);
  return find_result->value.value();
}

void FallbackLruCache::Put(String&& key, TypefaceVector&& arg) {
  HashMapType::iterator find_result = map_.find(key);
  if (find_result != map_.end()) {
    ordering_.Remove(find_result->value.ListNode());
    map_.erase(find_result);
  }

  if (map_.size() >= max_size_) {
    RemoveLeastRecentlyUsed();
  }

  std::unique_ptr<KeyListNode> list_node = std::make_unique<KeyListNode>(key);
  HashMapType::AddResult add_result = map_.insert(
      std::move(key), MappedWithListNode(std::move(arg), std::move(list_node)));
  DCHECK(add_result.is_new_entry);
  ordering_.Push(add_result.stored_value->value.ListNode());
}

void FallbackLruCache::Clear() {
  map_.clear();
  ordering_.Clear();
}

void FallbackLruCache::RemoveLeastRecentlyUsed() {
  KeyListNode* tail = ordering_.Tail();
  ordering_.Remove(tail);
  map_.erase(tail->key());
}

}  // namespace blink
