// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LRU_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LRU_CACHE_H_

#include <memory>

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"

namespace WTF {

// LruCache is a simple least-recently-used cache based on HashMap and
// DoublyLinkedList. Useful in situations where caching by using a HashMap is
// desirable but needs to be limited in size to not grow out of proportions.
// LruCache uses a HashMap to store cache entries, and uses a DoublyLinkedList
// in parallel to keep track of the age of entries. Accessing an entry using
// Get() refreshes its age, Put() places a new entry into the HashMap with
// youngest age as well. The least recently used entry of the list is pruned
// when a Put() call would otherwise exceed the max_size limit.
//
// Example:
// const size_t kMaxSize = 2;
// LruCache<uint16_t, String> my_cache(kMaxSize);
// my_cache.Put(13, "first string");
// my_cache.Put(42, "second string");
// my_cache.Put(256, "third string");
// my_cache.Get(13) // -> nullptr, has been removed due to kMaxSize == 2.
// my_cache.Get(42) // -> String* "second string"
// my_cache.Get(256) // -> String* "third_string"
//
// See lru_cache_test.cc for more examples.
template <typename KeyArg,
          typename MappedArg,
          typename HashArg = typename DefaultHash<KeyArg>::Hash,
          typename KeyTraitsArg = HashTraits<KeyArg>>
class LruCache {
  USING_FAST_MALLOC(LruCache);

 private:
  class MappedListNodeWithKey final
      : public DoublyLinkedListNode<MappedListNodeWithKey> {
    USING_FAST_MALLOC(MappedListNodeWithKey);

   public:
    friend class DoublyLinkedListNode<MappedListNodeWithKey>;

    MappedListNodeWithKey(const KeyArg& key, MappedArg&& mapped_arg)
        : key_(key), mapped_value_(std::move(mapped_arg)) {}

    MappedArg* Value() { return &mapped_value_; }

    void SetValue(MappedArg&& mapped_arg) {
      mapped_value_ = std::move(mapped_arg);
    }

    const KeyArg& Key() const { return key_; }

   private:
    KeyArg key_;
    MappedArg mapped_value_;
    MappedListNodeWithKey* prev_{nullptr};
    MappedListNodeWithKey* next_{nullptr};
  };

  using MappedListNode = std::unique_ptr<MappedListNodeWithKey>;

  using HashMapType = HashMap<KeyArg, MappedListNode, HashArg, KeyTraitsArg>;

 public:
  LruCache(size_t max_size) : max_size_(max_size) {
    static_assert(!IsGarbageCollectedType<KeyArg>::value ||
                      !IsGarbageCollectedType<MappedArg>::value,
                  "Cannot use LruCache<> with garbage collected types.");
    CHECK_GT(max_size_, 0u);
  }

  // Retrieve cache entry under |key| if it exists and refresh its age.
  // Returns: pointer to cache entry or nullptr if no entry is found for that
  // key.
  MappedArg* Get(const KeyArg& key) {
    if (map_.IsEmpty())
      return nullptr;

    typename HashMapType::iterator find_result = map_.find(key);
    if (find_result == map_.end())
      return nullptr;

    // Move result to beginning of list.
    MappedListNodeWithKey* node = find_result->value.get();
    ordering_.Remove(node);
    ordering_.Push(node);
    return find_result->value->Value();
  }

  // Place entry in cache as new / youngest. Multiple calls to Put() with an
  // identical key but differing MappedArg will override the stored value and
  // refresh the age.
  void Put(const KeyArg& key, MappedArg&& arg) {
    {
      typename HashMapType::iterator find_result = map_.find(key);
      if (find_result != map_.end()) {
        find_result->value->SetValue(std::move(arg));
        ordering_.Remove(find_result->value.get());
        ordering_.Push(find_result->value.get());
      } else {
        auto list_node =
            std::make_unique<MappedListNodeWithKey>(key, std::move(arg));
        typename HashMapType::AddResult add_result =
            map_.insert(key, std::move(list_node));
        DCHECK(add_result.is_new_entry);
        ordering_.Push(add_result.stored_value->value.get());
      }
    }

    if (map_.size() > max_size_) {
      RemoveLeastRecentlyUsed();
    }
  }

  // Clear the cache, remove all elements.
  void Clear() {
    map_.clear();
    ordering_.Clear();
  }

  size_t size() { return map_.size(); }

 private:
  void RemoveLeastRecentlyUsed() {
    MappedListNodeWithKey* tail = ordering_.Tail();
    ordering_.Remove(tail);
    map_.erase(tail->Key());
  }

  HashMapType map_;
  DoublyLinkedList<MappedListNodeWithKey> ordering_;
  size_t max_size_;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LRU_CACHE_H_
