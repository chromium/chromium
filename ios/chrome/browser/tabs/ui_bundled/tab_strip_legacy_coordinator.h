// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_LEGACY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_highlighting.h"

@protocol TabStripPresentation;

// A legacy coordinator that presents the public interface for the tablet tab
// strip feature.
@interface TabStripLegacyCoordinator : ChromeCoordinator<TabStripHighlighting>

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The base view controller for this coordinator. This is required because the
// TabStripLegacyCoordinator is instantiated before the BrowserViewController.
@property(nonatomic, weak, readwrite) UIViewController* baseViewController;

// Provides methods for presenting the tab strip and checking the visibility
// of the tab strip in the containing object.
@property(nonatomic, weak) id<TabStripPresentation> presentationProvider;

// The duration to wait before starting tab strip animations. Used to
// synchronize animations.
@property(nonatomic, assign) NSTimeInterval animationWaitDuration;

// Hides or shows the TabStrip.
- (void)hideTabStrip:(BOOL)hidden;

// Force resizing layout of the tab strip.
- (void)tabStripSizeDidChange;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_LEGACY_COORDINATOR_H_
