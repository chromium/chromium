// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/ui/assistant_container_presenter.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"

@protocol SceneViewControllerDelegate;
@class LayoutGuideCenter;
@protocol BWGCommands;
@class AssistantContainerViewController;

// A view controller that can act as the `rootViewController` for a scene's
// window.
@interface SceneViewController
    : UIViewController <FullscreenUIElement, AssistantContainerPresenter>

// A view to contain the TabGrid and BVC.
@property(nonatomic, readonly) UIView* appContainer;
// This view controller's LayoutGuideCenter.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// Delegate for this view controller.
@property(nonatomic, weak) id<SceneViewControllerDelegate> delegate;

// Sets the app bar.
- (void)setAppBar:(UIViewController*)appBar;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_H_
