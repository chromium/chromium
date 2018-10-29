// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@class SnapshotOverlay;
@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page.
@interface SnapshotGenerator : NSObject

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
               snapshotSessionId:(NSString*)snapshotSessionId
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the size the snapshot for the current page would have if it
// was regenerated. If capturing the snapshot is not possible, returns
// CGSizeZero.
- (CGSize)snapshotSize;

// If |snapshotCoalescingEnabled| is YES snapshots of the web page are
// coalesced until this method is called with |snapshotCoalescingEnabled| set to
// NO. When snapshot coalescing is enabled, mutiple calls to generate a snapshot
// with the same parameters may be coalesced.
- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled;

// Gets a color snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated. If the snapshot cannot be generated, the
// |callback| will be called with nil.
- (void)retrieveSnapshot:(void (^)(UIImage*))callback;

// Gets a grey snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated. If the snapshot cannot be generated, the
// |callback| will be called with nil.
- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback;

// Invalidates the cached snapshot for the current page, generates and caches
// a new snapshot. Returns the snapshot with or without the overlaid views
// (e.g. infobar), and either of the visible frame or of the full screen.
- (UIImage*)updateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Invalidates the cached snapshot for the current page, generates and caches
// a new snapshot. Calls |completion| with a snapshot with overlaid views (e.g.
// infobar) of the visible frame. This method should only be called if the web
// state has a valid web view.
- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates a new snapshot for the current page including optional infobars.
// Returns the snapshot with or without the overlaid views (e.g. infobar), and
// either of the visible frame or of the full screen.
- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                        visibleFrameOnly:(BOOL)visibleFrameOnly;

// Requests deletion of the current page snapshot from disk and memory.
- (void)removeSnapshot;

// Returns an image to use as replacement of a missing snapshot.
+ (UIImage*)defaultSnapshotImage;

// The SnapshotGenerator delegate.
@property(nonatomic, weak) id<SnapshotGeneratorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
