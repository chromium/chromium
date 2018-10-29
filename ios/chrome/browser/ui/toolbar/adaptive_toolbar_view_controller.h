// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_type.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol PopupMenuLongPressDelegate;
@class ToolbarButtonFactory;
@class ToolbarToolsMenuButton;

// ViewController for the adaptive toolbar. This ViewController is the super
// class of the different implementation (primary or secondary).
// This class and its subclasses are constraining some named layout guides to
// their buttons. All of those constraints are dropped upon size class changes
// and rotations. Any view constrained to a layout guide is expected to be
// dismissed on such events. For example, the tools menu is closed upon
// rotation.
@interface AdaptiveToolbarViewController
    : UIViewController<PopupMenuUIUpdating,
                       ToolbarConsumer,
                       NewTabPageControllerDelegate>

// Button factory.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
// Dispatcher for the ViewController.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;
// Delegate for the long press gesture recognizer triggering popup menu.
@property(nonatomic, weak) id<PopupMenuLongPressDelegate> longPressDelegate;

// Returns the tools menu button.
- (ToolbarToolsMenuButton*)toolsMenuButton;

// Updates the view so a snapshot can be taken. It needs to be adapted,
// depending on if it is a snapshot displayed |onNTP| or not.
- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP;
// Resets the view after taking a snapshot for a side swipe.
- (void)resetAfterSideSwipeSnapshot;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
