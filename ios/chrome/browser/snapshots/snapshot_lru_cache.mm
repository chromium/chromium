// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"

#import <stddef.h>

#import <memory>
#import <unordered_map>

#import "base/containers/lru_cache.h"

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

using NSObjectLRUCache = base::
    HashingLRUCache<id<NSObject>, id<NSObject>, NSObjectHash, NSObjectEqualTo>;

}  // namespace

@implementation SnapshotLRUCache {
  std::unique_ptr<NSObjectLRUCache> _cache;
}

- (instancetype)initWithCacheSize:(NSUInteger)maxCacheSize {
  if ((self = [super init])) {
    _cache = std::make_unique<NSObjectLRUCache>(maxCacheSize);
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
