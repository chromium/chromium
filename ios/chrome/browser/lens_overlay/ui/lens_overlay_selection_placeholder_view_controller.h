// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_PLACEHOLDER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_PLACEHOLDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_snapshot_consumer.h"

@protocol LensOverlaySelectionDelegate;

/// Placeholder view controller for the lens selection UI.
@interface LensOverlaySelectionPlaceholderViewController
    : UIViewController <LensOverlaySnapshotConsumer>

@property(weak, nonatomic) id<LensOverlaySelectionDelegate> delegate;

/// Cancels any previous requests, then sends the image to lens server.
- (void)startFullImageRequestWithImage:(UIImage*)image;
/// Cancels any previous requests.
- (void)cancelOngoingRequests;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_PLACEHOLDER_VIEW_CONTROLLER_H_
