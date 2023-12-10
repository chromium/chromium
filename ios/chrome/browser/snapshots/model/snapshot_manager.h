// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_MANAGER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_MANAGER_H_

#import <UIKit/UIKit.h>

class SnapshotID;
@class SnapshotStorage;
@class SnapshotGenerator;
@protocol SnapshotGeneratorDelegate;

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page. This lives on the UI thread.
@interface SnapshotManager : NSObject

// Strong reference to the snapshot generator which is used to generate
// snapshots.
@property(nonatomic, readonly) SnapshotGenerator* snapshotGenerator;

// Weak reference to the snapshot storage which is used to store and retrieve
// snapshots. SnapshotStorage is owned by SnapshotBrowserAgent.
@property(nonatomic, weak) SnapshotStorage* snapshotStorage;

// The snapshot ID.
@property(nonatomic, readonly) SnapshotID snapshotID;

// Designated initializer.
- (instancetype)initWithGenerator:(SnapshotGenerator*)generator
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

// Asynchronously generates a new snapshot with WebKit-based snapshot API,
// updates the snapshot storage, and runs a callback with the new snapshot
// image. It is an error to call this method if the web state is showing
// anything other (e.g., native content) than a web view.
- (void)updateWKWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates a new snapshot with UIKit-based snapshot API, updates the snapshot
// storage, and runs a callback with the new snapshot image.
- (void)updateUIViewSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates and returns a new snapshot image with UIKit-based snapshot API.
// This does not update the snapshot storage.
- (UIImage*)generateUIViewSnapshot;

// Hints that the snapshot will likely be saved to disk when the application is
// backgrounded.  The snapshot is then saved in memory, so it does not need to
// be read off disk.
- (void)willBeSavedGreyWhenBackgrounding;

// Writes a grey copy of the snapshot to disk, but if and only if a color
// version of the snapshot already exists in memory or on disk.
- (void)saveGreyInBackground;

// Requests deletion of the current page snapshot from disk and memory.
- (void)removeSnapshot;

// Sets the delegate to SnapshotGenerator. Generating snapshots before setting a
// delegate will fail. The delegate is not owned by the tab helper.
- (void)setDelegate:(id<SnapshotGeneratorDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_MANAGER_H_
