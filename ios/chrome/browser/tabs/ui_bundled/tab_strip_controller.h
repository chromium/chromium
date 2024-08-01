// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_constants.h"

@protocol TabStripPresentation;
@class ViewRevealingVerticalPanHandler;
class Browser;
@class LayoutGuideCenter;

// Controller class for the tabstrip.  Manages displaying tabs and keeping the
// display in sync with a Browser's WebStateList.  This controller is only
// instantiated on tablet.  The tab strip view itself is a subclass of
// UIScrollView, which manages scroll offsets and scroll animations.
@interface TabStripController : NSObject

@property(nonatomic, assign) BOOL highlightsSelectedTab;
@property(nonatomic, readonly, strong) UIView* view;

// The duration to wait before starting tab strip animations. Used to
// synchronize animations.
@property(nonatomic, assign) NSTimeInterval animationWaitDuration;

// Used to check if the tabstrip is visible before starting an animation.
@property(nonatomic, weak) id<TabStripPresentation> presentationProvider;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                     style:(TabStripStyle)style
                         layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Hides or shows the tab strip.
- (void)hideTabStrip:(BOOL)hidden;

// Preprare the receiver for destruction, disconnecting from all services.
// It is an error for the receiver to dealloc without this having been called
// first.
- (void)disconnect;

// Notifies of a forced resizing layout of the tab strip.
- (void)tabStripSizeDidChange;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_CONTROLLER_H_
