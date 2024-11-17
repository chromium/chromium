// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"

@protocol AdaptiveToolbarMenusProvider;
@class AdaptiveToolbarViewController;
@protocol AdaptiveToolbarViewControllerDelegate;
@protocol BrowserCommands;
@class LayoutGuideCenter;
@protocol OmniboxCommands;
@protocol PopupMenuCommands;
@class ToolbarButton;
@class ToolbarButtonFactory;

// ViewController for the adaptive toolbar. This ViewController is the super
// class of the different implementation (primary or secondary).
// This class and its subclasses are constraining some named layout guides to
// their buttons. All of those constraints are dropped upon size class changes
// and rotations. Any view constrained to a layout guide is expected to be
// dismissed on such events. For example, the tools menu is closed upon
// rotation.
@interface AdaptiveToolbarViewController
    : UIViewController <FullscreenUIElement,
                        PopupMenuUIUpdating,
                        ToolbarConsumer>

// Button factory.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
// Layout Guide Center.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// View controller for the location bar containing the omnibox. Nil when the
// toolbar doesn't have the omnibox.
@property(nonatomic, weak) UIViewController* locationBarViewController;
// Omnibox commands handler for the ViewController.
@property(nonatomic, weak) id<OmniboxCommands> omniboxCommandsHandler;
// Popup menu commands handler for the ViewController.
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuCommandsHandler;

// Provider for the context menus.
@property(nonatomic, weak) id<AdaptiveToolbarMenusProvider> menuProvider;
// Delegate for events in `AdaptiveToolbarViewController`.
@property(nonatomic, weak) id<AdaptiveToolbarViewControllerDelegate>
    adaptiveDelegate;

// Returns the tools menu button.
- (ToolbarButton*)toolsMenuButton;

// Whether the toolbar has the omnibox.
- (BOOL)hasOmnibox;
// Updates the view so a snapshot can be taken. It needs to be adapted,
// depending on if it is a snapshot displayed `onNonIncognitoNTP` or not.
- (void)updateForSideSwipeSnapshot:(BOOL)onNonIncognitoNTP;
// Resets the view after taking a snapshot for a side swipe.
- (void)resetAfterSideSwipeSnapshot;
// Sets the toolbar location bar alpha and vertical offset based on `progress`.
- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress;
// Triggers the slide-in animation for the toolbar with direction determined
// from `fromBelow`.
- (void)triggerToolbarSlideInAnimationFromBelow:(BOOL)fromBelow;
// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
