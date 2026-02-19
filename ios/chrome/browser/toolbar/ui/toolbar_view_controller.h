// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
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
@interface ToolbarViewController
    : UIViewController <FullscreenUIElement, ToolbarConsumer>

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

// The height delegate.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Layout Guide Center.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether this toolbar is currently visible or not.
@property(nonatomic, assign) BOOL visible;

// The location bar in this toolbar.
@property(nonatomic, strong) UIViewController* locationBarViewController;

// Initializer for the toolbar, in `incognito` or not.
- (instancetype)initInIncognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Shows/Hides the location bar.
- (void)setLocationBarHidden:(BOOL)hidden;

// Returns a copy of the location bar container, with its frame in the same
// coordinates as the real in window coordinates.
- (UIView*)locationBarContainerCopy;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
