// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_PRIVATE_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"

@protocol BrowserViewVisibilityAudience;
enum class BrowserViewVisibilityState;

// This is a private category that is intended to only be imported in
// browser_coordinator.mm.
@interface BrowserViewController (Private)

// Audience that gets notified of the visibility of the browser view.
@property(nonatomic, weak) id<BrowserViewVisibilityAudience>
    browserViewVisibilityAudience;

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

// The visibility state of the browser view. Value will be set to `kVisible` on
// viewDidAppear and to `kNotInViewHierarchy` on viewWillDisappear.
@property(nonatomic, readonly) BrowserViewVisibilityState visibilityState;

// Height of the header view.
@property(nonatomic, readonly) CGFloat headerHeight;

// Does an animation from `originPoint` when opening a background tab, then
// calls `completion`.
- (void)animateOpenBackgroundTabFromOriginPoint:(CGPoint)originPoint
                                     completion:(void (^)())completion;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_PRIVATE_H_
