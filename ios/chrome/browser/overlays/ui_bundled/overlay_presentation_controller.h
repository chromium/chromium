// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol OverlayPresentationControllerObserver;

// Presentation controller used for overlays presented using custom
// UIViewController presentation.
@interface OverlayPresentationController : UIPresentationController

// Whether the presentation controller resizes the presentation container view.
// When set to YES, the overlay presentation context view will be resized to fit
// the presented view so that touches that fall outside of the overlay can be
// forwarded to the underlying browser UI.  Presentation controllers that return
// YES for this property must not lay out their presented views in relation to
// the presenter.  Returns NO by default.
@property(nonatomic, readonly) BOOL resizesPresentationContainer;

// YES if the presented view was resized and therefore the presenting view
// controller needs a new layout pass. Defaults to YES to allow for a layout
// pass the first time through since the presenting view controller needs to
// resize from CGRectZero to the presented view size or vice versa.
@property(nonatomic, assign) BOOL needsLayout;

// Subclasses must notify the superclass when their container views lay out
// their subviews.
- (void)containerViewWillLayoutSubviews NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTROLLER_H_
