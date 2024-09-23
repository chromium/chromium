// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol OverlayContainerViewControllerDelegate;

// View controller used to show overlay UI.
@interface OverlayContainerViewController : UIViewController
@property(nonatomic, weak) id<OverlayContainerViewControllerDelegate> delegate;
@end

// Delegate protocol for the container view.
@protocol OverlayContainerViewControllerDelegate <NSObject>

// Called when `containerViewController`'s view moves to a new window. Overlay
// presentation should not be attempted until the container is added to
// a window.
- (void)containerViewController:
            (OverlayContainerViewController*)containerViewController
                didMoveToWindow:(UIWindow*)window;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
