// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class SnapshotGenerator;
@class SnapshotOverlay;

namespace web {
class WebState;
}

// Protocol for the SnapshotGenerator's delegate.
@protocol SnapshotGeneratorDelegate

// Returns whether it is possible to capture a snapshot for |webState|.
- (BOOL)canTakeSnapshotForWebState:(web::WebState*)webState;

// Returns the edge insets to use to crop the snapshot for |webState| during
// generation. If the snapshot should not be cropped, then UIEdgeInsetsZero
// can be returned.
- (UIEdgeInsets)snapshotEdgeInsetsForWebState:(web::WebState*)webState;

// Returns the list of SnapshotOverlays that should be rendered over the
// page when generating the snapshot for |webState|. If no overlays should
// be rendered, the list may be nil or empty.
- (NSArray<SnapshotOverlay*>*)snapshotOverlaysForWebState:
    (web::WebState*)webState;

// Invoked before capturing a snapshot for |webState|. The delegate can remove
// subviews from the hierarchy or take other actions to ensure the snapshot
// is correclty captured.
- (void)willUpdateSnapshotForWebState:(web::WebState*)webState;

// Invoked after capturing a snapshot for |webState|. The delegate can insert
// subviews that were removed during -willUpdateSnapshotForWebState: or take
// other actions necessary after a snapshot has been captured.
- (void)didUpdateSnapshotForWebState:(web::WebState*)webState
                           withImage:(UIImage*)snapshot;
// Returns the base view to be snapshotted.
- (UIView*)viewForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_DELEGATE_H_
