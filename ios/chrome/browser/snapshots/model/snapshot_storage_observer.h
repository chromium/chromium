// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_OBSERVER_H_

#import <Foundation/Foundation.h>

@class SnapshotStorage;

// Interface for listening to events occurring to the SnapshotStorage.
@protocol SnapshotStorageObserver
@optional
// Tells the observing object that the `snapshotStorage` was updated with a new
// snapshot corresponding to `snapshotID`.
- (void)snapshotStorage:(SnapshotStorage*)snapshotStorage
    didUpdateSnapshotForID:(SnapshotID)snapshotID;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_OBSERVER_H_
