// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_LRU_CACHE_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_LRU_CACHE_WIN_H_

namespace blink {

/* A LRU cache for storing a a vector of typefaces for a particular key string,
 * which would usually be a locale plus potential additional parameters. Uses a
 * HashMap for storage and access and a DoublyLinkedList for managing age of
 * entries. TODO(https://crbug.com/1010925): Potentially move this to a generic
 * LRU Cache implementation once we have such in WTF. */
class PLATFORM_EXPORT FallbackLruCache {
  USING_FAST_MALLOC(FallbackLruCache);

 public:
  FallbackLruCache(size_t max_size);

  using TypefaceVector = Vector<sk_sp<SkTypeface>>;

  TypefaceVector* Get(const String& key);
  void Put(String&& key, TypefaceVector&& arg);

  void Clear();

  size_t size() const { return map_.size(); }

 private:
  class KeyListNode final : public DoublyLinkedListNode<KeyListNode> {
    USING_FAST_MALLOC(KeyListNode);

   public:
    friend class DoublyLinkedListNode<KeyListNode>;
    KeyListNode(const String& key) : key_(key) {}

    const String& key() const { return key_; }

   private:
    String key_;
    KeyListNode* prev_{nullptr};
    KeyListNode* next_{nullptr};
  };

  class MappedWithListNode {
    USING_FAST_MALLOC(MappedWithListNode);

   public:
    MappedWithListNode(TypefaceVector&& mapped_arg,
                       std::unique_ptr<KeyListNode>&& list_node)
        : mapped_value_(std::move(mapped_arg)),
          list_node_(std::move(list_node)) {}

    MappedWithListNode(WTF::HashTableDeletedValueType) {
      list_node_.reset(reinterpret_cast<KeyListNode*>(-1));
    }

    TypefaceVector* value() { return &mapped_value_; }
    KeyListNode* ListNode() { return list_node_.get(); }

   private:
    TypefaceVector mapped_value_;
    std::unique_ptr<KeyListNode> list_node_;
  };

  void RemoveLeastRecentlyUsed();

  using HashMapType = HashMap<String,
                              MappedWithListNode,
                              DefaultHash<String>::Hash,
                              HashTraits<String>,
                              SimpleClassHashTraits<MappedWithListNode>>;

  HashMapType map_;
  DoublyLinkedList<KeyListNode> ordering_;
  size_t max_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_LRU_CACHE_WIN_H_
