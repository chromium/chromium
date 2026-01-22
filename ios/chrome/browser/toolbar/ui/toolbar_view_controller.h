// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"

@protocol ActivityServiceCommands;
@protocol BrowserCoordinatorCommands;
@class LayoutGuideCenter;
@protocol PopupMenuCommands;
@protocol SceneCommands;
@class ToolbarButtonFactory;
@protocol ToolbarHeightDelegate;
@protocol ToolbarMutator;

// View controller for the toolbar.
@interface ToolbarViewController : UIViewController <ToolbarConsumer>

// Handler for the browser coordinator commands.
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;

// Handler for the popup menu commands.
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuHandler;

// Handler for the activity service commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;

// Handler for the scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Mutator to handle toolbar actions.
@property(nonatomic, weak) id<ToolbarMutator> mutator;

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// The height of the toolbar.
@property(nonatomic, readonly) CGFloat toolbarHeight;

// The height delegate.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Layout Guide Center.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether this toolbar is currently visible or not.
@property(nonatomic, assign) BOOL visible;

// The location bar in this toolbar.
@property(nonatomic, strong) UIViewController* locationBarViewController;

// Triggers the animation for the slide in of the toolbar.
- (void)triggerToolbarSlideInAnimation;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
