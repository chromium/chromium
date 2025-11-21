// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_consumer.h"

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
// Whether the location bar is currently focused. This is used to prevent
// updating the location bar container height when the multiline omnibox is
// enabled, as it's already handled by the toolbar height delegate.
@property(nonatomic, assign) BOOL locationBarFocused;
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

// The view containing the location bar.
- (UIView*)locationBarContainer;

// Returns the tab grid button.
- (ToolbarButton*)tabGridButton;

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
// Highlights the tab grid button if `highlight` is YES, resets to original
// color if NO.
- (void)IPHHighlightTabGridButton:(BOOL)highlight;
// Sets the height of the location bar. Used when the omnibox is multiline.
- (void)setLocationBarHeight:(CGFloat)height;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_H_
