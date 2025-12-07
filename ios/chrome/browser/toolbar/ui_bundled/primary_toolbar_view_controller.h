// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/toolbar_animatee.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_positioner.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_consumer.h"

@protocol BannerPromoViewDelegate;
@protocol PrimaryToolbarViewControllerDelegate;
@class TabGroupIndicatorView;
@protocol ToolbarHeightDelegate;
@class ViewRevealingVerticalPanHandler;

// ViewController for the primary toobar part of the adaptive toolbar. The one
// at the top of the screen.
@interface PrimaryToolbarViewController
    : AdaptiveToolbarViewController <SharingPositioner,
                                     KeyCommandActions,
                                     PrimaryToolbarConsumer,
                                     ToolbarAnimatee>

@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate> delegate;

/// Delegate to inform about toolbar height changes.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Whether the omnibox should be hidden on NTP.
@property(nonatomic, assign) BOOL shouldHideOmniboxOnNTP;

// Pan gesture handler for the toolbar.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// Delegate for banner promo interactions.
@property(nonatomic, weak) id<BannerPromoViewDelegate> bannerPromoDelegate;

// Whether the toolbar's location bar is currently expanded.
@property(nonatomic, readonly) BOOL locationBarIsExpanded;

// Sets the tabgroupIndicatorView.
- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
