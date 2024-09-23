// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/toolbar_animatee.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

@protocol PrimaryToolbarViewControllerDelegate;
@class TabGroupIndicatorView;
@class ViewRevealingVerticalPanHandler;

// ViewController for the primary toobar part of the adaptive toolbar. The one
// at the top of the screen.
@interface PrimaryToolbarViewController
    : AdaptiveToolbarViewController <SharingPositioner,
                                     KeyCommandActions,
                                     ToolbarAnimatee>

@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate> delegate;

// Whether the omnibox should be hidden on NTP.
@property(nonatomic, assign) BOOL shouldHideOmniboxOnNTP;

// Pan gesture handler for the toolbar.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// Sets the tabgroupIndicatorView.
- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
