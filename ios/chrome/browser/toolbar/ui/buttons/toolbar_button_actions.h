// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_ACTIONS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_ACTIONS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_button_type.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_tab_grid_menu_consumer.h"

@class ToolbarButton;
@protocol ActivityServiceCommands;
@protocol ActorOverlayCommands;
@protocol PopupMenuCommands;
@protocol SceneCommands;
@protocol ToolbarButtonsMutator;

// Wires the taps, menu metrics and haptics, and the tab grid menu of the
// toolbar's control buttons. Shared by every surface showing them, so that they
// behave and record metrics the same everywhere. It holds no state of the
// surfaces: the mutator records the metrics that depend on the visible page,
// and gives it the tab grid menu.
@interface ToolbarButtonActions : NSObject <ToolbarTabGridMenuConsumer>

// Mutator handling the button actions.
@property(nonatomic, weak) id<ToolbarButtonsMutator> mutator;

// Handler for the popup menu commands.
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuHandler;

// Handler for the activity service commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;

// Handler for the scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Handler for the actor overlay commands.
@property(nonatomic, weak) id<ActorOverlayCommands> actorOverlayHandler;

// Adds the actions of a button of `type` to `button`.
- (void)configureButton:(ToolbarButton*)button ofType:(ToolbarButtonType)type;

// Returns a recognizer of the taps on the disabled navigation `buttons`, which
// shows that interactions with the tab are limited while an actor controls it.
// To be added to an ancestor of `buttons`, and only enabled while the active
// tab is actor-controlled.
- (UIGestureRecognizer*)disabledNavigationTapRecognizerForButtons:
    (NSArray<ToolbarButton*>*)buttons;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_ACTIONS_H_
