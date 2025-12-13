// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_PARAMETERS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_PARAMETERS_H_

#import <UIKit/UIKit.h>

// Config class that contains all the parameters needed to perform a tab grid
// transition animation.
@interface TabGridAnimationParameters : NSObject

// The view that will be animated.
@property(nonatomic, strong, readonly) UIView* animatedView;

// The destination frame of the animated view in window coordinates.
@property(nonatomic, assign, readonly) CGRect destinationFrame;

// The origin frame of the animated view in window coordinates.
@property(nonatomic, assign, readonly) CGRect originFrame;

// A snapshot of the web content for the animated tab.
@property(nonatomic, strong, readonly) UIImage* contentSnapshot;

// The height of the tab's top toolbar (anything above the web content area).
@property(nonatomic, assign, readonly) CGFloat topToolbarHeight;

// The height of the tab's bottom toolbar (anything below the web content area).
@property(nonatomic, assign, readonly) CGFloat bottomToolbarHeight;

// A snapshot of the tab's top toolbar (anything above the web content area).
@property(nonatomic, strong, readonly) UIView* topToolbarSnapshotView;

// A snapshot of the tab's bottom toolbar (anything below the web content area).
@property(nonatomic, strong, readonly) UIView* bottomToolbarSnapshotView;

// The view controller of the currently active grid (regular, incognito, etc.).
@property(nonatomic, weak, readonly) UIViewController* activeGrid;

// The view controller of the pinned tabs.
@property(nonatomic, weak, readonly) UIViewController* pinnedTabs;

// Whether the top toolbar should be scaled during the animation (disabled for
// devices displaying the tab strip).
@property(nonatomic, assign, readonly) BOOL shouldScaleTopToolbar;

// Whether the current tab is in incognito mode.
@property(nonatomic, assign, readonly) BOOL incognito;

// Whether the active cell is from a pinned tab.
@property(nonatomic, assign, readonly) BOOL activeCellPinned;

// Whether the top toolbar is hidden during the animation.
@property(nonatomic, assign, readonly) BOOL topToolbarHidden;

- (instancetype)initWithDestinationFrame:(CGRect)destinationFrame
                             originFrame:(CGRect)originFrame
                              activeGrid:(UIViewController*)activeGrid
                              pinnedTabs:(UIViewController*)pinnedTabs
                        activeCellPinned:(BOOL)activeCellPinned
                            animatedView:(UIView*)animatedView
                         contentSnapshot:(UIImage*)contentSnapshot
                        topToolbarHeight:(CGFloat)topToolbarHeight
                     bottomToolbarHeight:(CGFloat)bottomToolbarHeight
                  topToolbarSnapshotView:(UIView*)topToolbarSnapshotView
               bottomToolbarSnapshotView:(UIView*)bottomToolbarSnapshotView
                   shouldScaleTopToolbar:(BOOL)shouldScaleTopToolbar
                               incognito:(BOOL)incognito
                        topToolbarHidden:(BOOL)topToolbarHidden
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_PARAMETERS_H_
