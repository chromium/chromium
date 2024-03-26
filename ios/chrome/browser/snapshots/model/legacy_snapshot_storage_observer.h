// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_OBSERVER_H_

#import <Foundation/Foundation.h>

@class LegacySnapshotStorage;

// Interface for listening to events occurring to the LegacySnapshotStorage.
// TODO(crbug.com/1502841): Remove this protocol once the new implementation
// written in Swift is used by default.
@protocol LegacySnapshotStorageObserver
@optional
// Tells the observing object that the `snapshotStorage` was updated with a new
// snapshot corresponding to `snapshotID`.
- (void)snapshotStorage:(LegacySnapshotStorage*)snapshotStorage
    didUpdateSnapshotForID:(SnapshotID)snapshotID;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_OBSERVER_H_
