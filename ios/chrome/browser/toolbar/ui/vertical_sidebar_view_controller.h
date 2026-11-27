// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_VERTICAL_SIDEBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_VERTICAL_SIDEBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_consumer.h"

@class LayoutGuideCenter;
@class ToolbarButtonActions;
@class ToolbarButtonFactory;

// Shows the toolbar's control buttons in the system vertical bar column (iPhone
// Duo folded), the same way in both IAs: back and forward on top, then new tab
// and tab grid, and the tools menu at the bottom. It only expects
// `ToolbarButtonsConsumer` updates while active, as activating it loads its
// view.
@interface VerticalSidebarViewController
    : UIViewController <PopupMenuUIUpdating, ToolbarButtonsConsumer>

// The view, created on first access.
@property(nonatomic, strong) VerticalSidebarView* view;

// Whether the sidebar is shown. Becoming active, its buttons take the named
// layout guides of the toolbar's back, forward, tab grid and tools menu
// buttons, so the toolbars must have hidden theirs first. Becoming inactive,
// they release the ones they still have. Loads the view.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Initializes with the factory of the buttons, their `actions`, and the
// `layoutGuideCenter` of their named layout guides.
- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)buttonFactory
                              actions:(ToolbarButtonActions*)actions
                    layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_VERTICAL_SIDEBAR_VIEW_CONTROLLER_H_
