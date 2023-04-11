// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@class SnapshotCache;
@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page.
@interface SnapshotGenerator : NSObject

// Weak reference to the snapshot cache which is used to store and retrieve
// snapshots for the WebState associated with this SnapshotGenerator.
@property(nonatomic, weak) SnapshotCache* snapshotCache;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
              snapshotIdentifier:(NSString*)snapshotIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Gets a color snapshot for the current page, calling `callback` once it has
// been retrieved. Invokes `callback` with nil if a snapshot does not exist.
- (void)retrieveSnapshot:(void (^)(UIImage*))callback;

// Gets a grey snapshot for the current page, calling `callback` once it has
// been retrieved or regenerated. If the snapshot cannot be generated, the
// `callback` will be called with nil.
- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback;

// Generates a new snapshot, updates the snapshot cache, and returns the new
// snapshot image.
- (UIImage*)updateSnapshot;

// Asynchronously generates a new snapshot, updates the snapshot cache, and runs
// `callback` with the new snapshot image. It is an error to call this method if
// the web state is showing anything other (e.g., native content) than a web
// view.
- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates a new snapshot and returns the new snapshot image. This does not
// update the snapshot cache. If `shouldAddOverlay` is YES, overlays (e.g.,
// infobars, the download manager, and sad tab view) are also captured in the
// snapshot image.
- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay;

// Hint that the snapshot will likely be saved to disk when the application is
// backgrounded.  The snapshot is then saved in memory, so it does not need to
// be read off disk.
- (void)willBeSavedGreyWhenBackgrounding;

// Write a grey copy of the snapshot to disk, but if and only if a color
// version of the snapshot already exists in memory or on disk.
- (void)saveGreyInBackground;

// Requests deletion of the current page snapshot from disk and memory.
- (void)removeSnapshot;

// The SnapshotGenerator delegate.
@property(nonatomic, weak) id<SnapshotGeneratorDelegate> delegate;

// The snapshot identifier.
@property(nonatomic, readonly) NSString* snapshotIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
