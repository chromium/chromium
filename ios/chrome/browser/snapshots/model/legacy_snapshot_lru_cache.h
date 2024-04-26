// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_LRU_CACHE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_LRU_CACHE_H_

#import <Foundation/Foundation.h>

class SnapshotID;

// This class implements a cache with a limited size. Once the cache reach its
// size limit, it will start to evict items in a Least Recently Used order
// (where the term "used" is determined in terms of query to the cache).
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotLRUCache<ObjectType> : NSObject

// The maximum amount of items that the cache can hold before starting to
// evict. The value 0 is used to signify that the cache can hold an unlimited
// amount of elements (i.e. never evicts).
@property(nonatomic, readonly) NSUInteger maxCacheSize;

// Use the initWithCacheSize: designated initializer. The is no good general
// default value for the cache size.
- (instancetype)init NS_UNAVAILABLE;

// `maxCacheSize` value is used to specify the maximum amount of items that the
// cache can hold before starting to evict items.
- (instancetype)initWithCacheSize:(NSUInteger)maxCacheSize
    NS_DESIGNATED_INITIALIZER;

// Query the cache for an item corresponding to the `key`. Returns nil if there
// is no item corresponding to that key.
- (ObjectType)objectForKey:(SnapshotID)key;

// Adds the pair `key`, `obj` to the cache. If the value of the maxCacheSize
// property is non zero, the cache may evict an elements if the maximum cache
// size is reached. If the `key` is already present in the cache, the value for
// that key is replaced by `object`.
- (void)setObject:(ObjectType)object forKey:(SnapshotID)key;

// Remove the key, value pair corresponding to the given `key`.
- (void)removeObjectForKey:(SnapshotID)key;

// Remove all objects from the cache.
- (void)removeAllObjects;

// Returns the amount of items that the cache currently hold.
- (NSUInteger)count;

// Returns true if the cache is empty.
- (BOOL)isEmpty;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_LRU_CACHE_H_
