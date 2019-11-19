// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"

#include <stddef.h>

#include <memory>
#include <unordered_map>

#include "base/containers/mru_cache.h"
#include "base/macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

struct NSObjectEqualTo {
  bool operator()(id<NSObject> obj1, id<NSObject> obj2) const {
    return [obj1 isEqual:obj2];
  }
};

struct NSObjectHash {
  std::size_t operator()(id<NSObject> obj) const { return [obj hash]; }
};

template <class KeyType, class ValueType, class HashType>
struct MRUCacheNSObjectHashMap {
  using Type =
      std::unordered_map<KeyType, ValueType, HashType, NSObjectEqualTo>;
};

using NSObjectMRUCache = base::MRUCacheBase<id<NSObject>,
                                            id<NSObject>,
                                            NSObjectHash,
                                            MRUCacheNSObjectHashMap>;

}  // namespace

@implementation SnapshotLRUCache {
  std::unique_ptr<NSObjectMRUCache> _cache;
}

- (instancetype)initWithCacheSize:(NSUInteger)maxCacheSize {
  if ((self = [super init])) {
    _cache = std::make_unique<NSObjectMRUCache>(maxCacheSize);
  }
  return self;
}

- (NSUInteger)maxCacheSize {
  return _cache->max_size();
}

- (id)objectForKey:(id<NSObject>)key {
  auto it = _cache->Get(key);
  if (it == _cache->end())
    return nil;
  return it->second;
}

- (void)setObject:(id<NSObject>)value forKey:(NSObject*)key {
  _cache->Put([key copy], value);
}

- (void)removeObjectForKey:(id<NSObject>)key {
  auto it = _cache->Peek(key);
  if (it != _cache->end())
    _cache->Erase(it);
}

- (void)removeAllObjects {
  _cache->Clear();
}

- (NSUInteger)count {
  return _cache->size();
}

- (BOOL)isEmpty {
  return _cache->empty();
}

@end
