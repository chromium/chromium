// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_LRU_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_LRU_H_

#include <stddef.h>

#include <list>
#include <unordered_map>

#include "base/check.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Simple LRU (least recently used) class.
// Keeps track of a set of data and lets you get the least recently used
// (oldest) element at any time. All operations are O(1). Elements are expected
// to be hashable and unique.
// Example:
//  LRU<int> lru;
//  lru.Insert(1);
//  lru.Insert(2);
//  lru.Insert(3);
//  lru.Use(1);
//  cout << lru.Pop();  // this will print "2"
template <typename T>
class BLINK_PLATFORM_EXPORT LRU {
 public:
  LRU() = default;
  LRU(const LRU&) = delete;
  LRU& operator=(const LRU&) = delete;

  // Adds |x| to LRU.
  // |x| must not already be in the LRU.
  // Faster than Use(), and will DCHECK that |x| is not in the LRU.
  void Insert(const T& x) {
    DCHECK(!Contains(x));
    lru_.push_front(x);
    pos_[x] = lru_.begin();
  }

  // Removes |x| from LRU.
  // |x| must be in the LRU.
  void Remove(const T& x) {
    DCHECK(Contains(x));
    lru_.erase(pos_[x]);
    pos_.erase(x);
  }

  // Moves |x| to front of LRU. (most recently used)
  // If |x| is not in LRU, it is added.
  // Please call Insert() if you know that |x| is not in the LRU.
  void Use(const T& x) {
    if (Contains(x))
      Remove(x);
    Insert(x);
  }

  bool Empty() const { return lru_.empty(); }

  // Returns the Least Recently Used T and removes it.
  T Pop() {
    DCHECK(!Empty());
    T ret = lru_.back();
    lru_.pop_back();
    pos_.erase(ret);
    return ret;
  }

  // Returns the Least Recently Used T _without_ removing it.
  T Peek() const {
    DCHECK(!Empty());
    return lru_.back();
  }

  bool Contains(const T& x) const { return pos_.find(x) != pos_.end(); }

  size_t Size() const { return pos_.size(); }

 private:
  friend class LRUTest;

  // Linear list of elements, most recently used first.
  std::list<T> lru_;

  // Maps element values to positions in the list so that we
  // can quickly remove elements.
  std::unordered_map<T, typename std::list<T>::iterator> pos_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_LRU_H_
