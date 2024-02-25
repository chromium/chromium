// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_lru_cache.h"

#import <memory>

#import "base/containers/lru_cache.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"

@implementation LegacySnapshotLRUCache {
  std::unique_ptr<base::LRUCache<SnapshotID, id>> _cache;
}

- (instancetype)initWithCacheSize:(NSUInteger)maxCacheSize {
  if ((self = [super init])) {
    _cache = std::make_unique<base::LRUCache<SnapshotID, id>>(maxCacheSize);
  }
  return self;
}

- (NSUInteger)maxCacheSize {
  return _cache->max_size();
}

- (id)objectForKey:(SnapshotID)key {
  auto it = _cache->Get(key);
  if (it == _cache->end()) {
    return nil;
  }
  return it->second;
}

- (void)setObject:(id)value forKey:(SnapshotID)key {
  _cache->Put(key, value);
}

- (void)removeObjectForKey:(SnapshotID)key {
  auto it = _cache->Peek(key);
  if (it != _cache->end()) {
    _cache->Erase(it);
  }
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
