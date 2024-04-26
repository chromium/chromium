// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_

#import <UIKit/UIKit.h>

class SnapshotID;
@class SnapshotStorageWrapper;
@class LegacySnapshotGenerator;
@protocol SnapshotGeneratorDelegate;

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page. This lives on the UI thread.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotManager : NSObject

// Strong reference to the snapshot generator which is used to generate
// snapshots.
@property(nonatomic, readonly) LegacySnapshotGenerator* snapshotGenerator;

// Weak reference to the snapshot storage which is used to store and retrieve
// snapshots. SnapshotStorage is owned by SnapshotBrowserAgent.
@property(nonatomic, weak) SnapshotStorageWrapper* snapshotStorage;

// The snapshot ID.
@property(nonatomic, readonly) SnapshotID snapshotID;

// Designated initializer.
- (instancetype)initWithGenerator:(LegacySnapshotGenerator*)generator
                       snapshotID:(SnapshotID)snapshotID
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Gets a color snapshot for the current page, calling `callback` once it has
// been retrieved. Invokes `callback` with nil if a snapshot does not exist.
- (void)retrieveSnapshot:(void (^)(UIImage*))callback;

// Gets a grey snapshot for the current page, calling `callback` once it has
// been retrieved or regenerated. If the snapshot cannot be generated, the
// `callback` will be called with nil.
- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback;

// Generates a new snapshot, updates the snapshot storage, and runs a callback
// with the new snapshot image.
- (void)updateSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates and returns a new snapshot image with UIKit-based snapshot API.
// This does not update the snapshot storage.
- (UIImage*)generateUIViewSnapshot;

// Requests deletion of the current page snapshot from disk and memory.
- (void)removeSnapshot;

// Sets the delegate to SnapshotGenerator. Generating snapshots before setting a
// delegate will fail. The delegate is not owned by the tab helper.
- (void)setDelegate:(id<SnapshotGeneratorDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_
