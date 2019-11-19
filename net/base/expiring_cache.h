// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_EXPIRING_CACHE_H_
#define NET_BASE_EXPIRING_CACHE_H_

#include <stddef.h>

#include <map>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace net {

template <typename KeyType,
          typename ValueType,
          typename ExpirationType>
class NoopEvictionHandler {
 public:
  void Handle(const KeyType& key,
              const ValueType& value,
              const ExpirationType& expiration,
              const ExpirationType& now,
              bool onGet) const {
  }
};

// Cache implementation where all entries have an explicit expiration policy. As
// new items are added, expired items will be removed first.
// The template types have the following requirements:
//  KeyType must be LessThanComparable, Assignable, and CopyConstructible.
//  ValueType must be CopyConstructible and Assignable.
//  ExpirationType must be CopyConstructible and Assignable.
//  ExpirationCompare is a function class that takes two arguments of the
//    type ExpirationType and returns a bool. If |comp| is an instance of
//    ExpirationCompare, then the expression |comp(current, expiration)| shall
//    return true iff |current| is still valid within |expiration|.
//
// A simple use of this class may use base::TimeTicks, which provides a
// monotonically increasing clock, for the expiration type. Because it's always
// increasing, std::less<> can be used, which will simply ensure that |now| is
// sorted before |expiration|:
//
//   ExpiringCache<std::string, std::string, base::TimeTicks,
//                 std::less<base::TimeTicks> > cache(0);
//   // Add a value that expires in 5 minutes
//   cache.Put("key1", "value1", base::TimeTicks::Now(),
//             base::TimeTicks::Now() + base::TimeDelta::FromMinutes(5));
//   // Add another value that expires in 10 minutes.
//   cache.Put("key2", "value2", base::TimeTicks::Now(),
//             base::TimeTicks::Now() + base::TimeDelta::FromMinutes(10));
//
// Alternatively, there may be some more complex expiration criteria, at which
// point a custom functor may be used:
//
//   struct ComplexExpirationFunctor {
//     bool operator()(const ComplexExpiration& now,
//                     const ComplexExpiration& expiration) const;
//   };
//   ExpiringCache<std::string, std::string, ComplexExpiration,
//                 ComplexExpirationFunctor> cache(15);
//   // Add a value that expires once the 'sprocket' has 'cog'-ified.
//   cache.Put("key1", "value1", ComplexExpiration("sprocket"),
//             ComplexExpiration("cog"));
template <typename KeyType,
          typename ValueType,
          typename ExpirationType,
          typename ExpirationCompare,
          typename EvictionHandler = NoopEvictionHandler<KeyType,
                                                         ValueType,
                                                         ExpirationType> >
class ExpiringCache {
 private:
  // Intentionally violate the C++ Style Guide so that EntryMap is known to be
  // a dependent type. Without this, Clang's two-phase lookup complains when
  // using EntryMap::const_iterator, while GCC and MSVC happily resolve the
  // typename.

  // Tuple to represent the value and when it expires.
  typedef std::pair<ValueType, ExpirationType> Entry;
  typedef std::map<KeyType, Entry> EntryMap;

 public:
  typedef KeyType key_type;
  typedef ValueType value_type;
  typedef ExpirationType expiration_type;

  // This class provides a read-only iterator over items in the ExpiringCache
  class Iterator {
   public:
    explicit Iterator(const ExpiringCache& cache)
        : cache_(cache),
          it_(cache_.entries_.begin()) {
    }
    ~Iterator() {}

    bool HasNext() const { return it_ != cache_.entries_.end(); }
    void Advance() { ++it_; }

    const KeyType& key() const { return it_->first; }
    const ValueType& value() const { return it_->second.first; }
    const ExpirationType& expiration() const { return it_->second.second; }

   private:
    const ExpiringCache& cache_;

    // Use a second layer of type indirection, as both EntryMap and
    // EntryMap::const_iterator are dependent types.
    typedef typename ExpiringCache::EntryMap EntryMap;
    typename EntryMap::const_iterator it_;
  };


  // Constructs an ExpiringCache that stores up to |max_entries|.
  explicit ExpiringCache(size_t max_entries) : max_entries_(max_entries) {}
  ~ExpiringCache() {}

  // Returns the value matching |key|, which must be valid at the time |now|.
  // Returns NULL if the item is not found or has expired. If the item has
  // expired, it is immediately removed from the cache.
  // Note: The returned pointer remains owned by the ExpiringCache and is
  // invalidated by a call to a non-const method.
  const ValueType* Get(const KeyType& key, const ExpirationType& now) {
    typename EntryMap::iterator it = entries_.find(key);
    if (it == entries_.end())
      return nullptr;

    // Immediately remove expired entries.
    if (!expiration_comp_(now, it->second.second)) {
      Evict(it, now, true);
      return nullptr;
    }

    return &it->second.first;
  }

  // Updates or replaces the value associated with |key|.
  void Put(const KeyType& key,
           const ValueType& value,
           const ExpirationType& now,
           const ExpirationType& expiration) {
    typename EntryMap::iterator it = entries_.find(key);
    if (it == entries_.end()) {
      // Compact the cache if it grew beyond the limit.
      if (entries_.size() == max_entries_ )
        Compact(now);

      // No existing entry. Creating a new one.
      entries_.insert(std::make_pair(key, Entry(value, expiration)));
    } else {
      // Update an existing cache entry.
      it->second.first = value;
      it->second.second = expiration;
    }
  }

  // Empties the cache.
  void Clear() {
    entries_.clear();
  }

  // Returns the number of entries in the cache.
  size_t size() const { return entries_.size(); }

  // Returns the maximum number of entries in the cache.
  size_t max_entries() const { return max_entries_; }

  bool empty() const { return entries_.empty(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExpiringCacheTest, Compact);
  FRIEND_TEST_ALL_PREFIXES(ExpiringCacheTest, CustomFunctor);

  // Prunes entries from the cache to bring it below |max_entries()|.
  void Compact(const ExpirationType& now) {
    // Clear out expired entries.
    typename EntryMap::iterator it;
    for (it = entries_.begin(); it != entries_.end(); ) {
      if (!expiration_comp_(now, it->second.second)) {
        Evict(it++, now, false);
      } else {
        ++it;
      }
    }

    if (entries_.size() < max_entries_)
      return;

    // If the cache is still too full, start deleting items 'randomly'.
    for (it = entries_.begin();
         it != entries_.end() && entries_.size() >= max_entries_;) {
      Evict(it++, now, false);
    }
  }

  void Evict(typename EntryMap::iterator it,
             const ExpirationType& now,
             bool on_get) {
    eviction_handler_.Handle(it->first, it->second.first, it->second.second,
                             now, on_get);
    entries_.erase(it);
  }

  // Bound on total size of the cache.
  size_t max_entries_;

  EntryMap entries_;
  ExpirationCompare expiration_comp_;
  EvictionHandler eviction_handler_;

  DISALLOW_COPY_AND_ASSIGN(ExpiringCache);
};

}  // namespace net

#endif  // NET_BASE_EXPIRING_CACHE_H_
