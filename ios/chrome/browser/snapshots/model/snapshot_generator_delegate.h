// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class SnapshotGenerator;

namespace web {
class WebState;
}

// Protocol for the SnapshotGenerator's delegate.
@protocol SnapshotGeneratorDelegate

// Returns whether it is possible to capture a snapshot for `webState`.
- (BOOL)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    canTakeSnapshotForWebState:(web::WebState*)webState;

// Returns the edge insets to use to crop the snapshot for `webState` during
// generation. If the snapshot should not be cropped, then UIEdgeInsetsZero
// can be returned.  The returned insets should be in the coordinate system of
// the view returned by `-baseViewForWebState:`.
- (UIEdgeInsets)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    snapshotEdgeInsetsForWebState:(web::WebState*)webState;

// Returns the list of overlay views that should be rendered over the
// page when generating the snapshot for `webState`. If no overlays should
// be rendered, the list may be nil or empty. The order of views in the array
// will be the z order of their image in the composed snapshot. A view at the
// end of the array will appear in front of a view at the beginning.
- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState;

// Invoked before capturing a snapshot for `webState`. The delegate can remove
// subviews from the hierarchy or take other actions to ensure the snapshot
// is correclty captured.
- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState;

// Returns the base view to be snapshotted.
- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_DELEGATE_H_
