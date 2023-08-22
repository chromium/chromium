// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_OBSERVER_H_

#import <Foundation/Foundation.h>

@class SnapshotCache;

// Interface for listening to events occurring to the SnapshotCache.
@protocol SnapshotCacheObserver
@optional
// Tells the observing object that the `snapshotCache` was updated with a new
// snapshot corresponding to `snapshotID`.
- (void)snapshotCache:(SnapshotCache*)snapshotCache
    didUpdateSnapshotForID:(SnapshotID)snapshotID;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_OBSERVER_H_
