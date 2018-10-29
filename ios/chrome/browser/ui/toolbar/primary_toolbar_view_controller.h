// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/orchestrator/toolbar_animatee.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

@protocol PrimaryToolbarViewControllerDelegate;

// ViewController for the primary toobar part of the adaptive toolbar. It is the
// part always displayed and containing the location bar.
@interface PrimaryToolbarViewController
    : AdaptiveToolbarViewController<ActivityServicePositioner,
                                    FullscreenUIElement,
                                    ToolbarAnimatee>

@property(nonatomic, weak) id<PrimaryToolbarViewControllerDelegate> delegate;

// Sets the location bar view, containing the omnibox.
- (void)setLocationBarView:(UIView*)locationBarView;

// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
