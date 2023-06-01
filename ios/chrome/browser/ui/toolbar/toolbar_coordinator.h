// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_coordinating.h"

@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocusDelegate;
@class PrimaryToolbarCoordinator;
@class PrimaryToolbarViewController;
@protocol SharingPositioner;
@class SecondaryToolbarCoordinator;
@class SecondaryToolbarViewController;
@protocol ViewRevealingAnimatee;
@class ViewRevealingVerticalPanHandler;

/// Coordinator above primary and secondary toolbars. It does not have a
/// view controller. This object is also an interface between multiple toolbars
/// and the objects which want to interact with them without having to know to
/// which one specifically send the call.
@interface ToolbarCoordinator
    : ChromeCoordinator <PopupMenuUIUpdating, ToolbarCoordinating>

/// Delegate for focusing omnibox in `locationBarCoordinator`.
@property(nonatomic, weak) id<OmniboxFocusDelegate> omniboxFocusDelegate;
/// Delegate for presenting the popup in `locationBarCoordinator`.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;

/// Initializes this coordinator with its `browser` and a nil base view
/// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

/// Returns `primaryToolbarCoordinator`.
- (PrimaryToolbarCoordinator*)primaryToolbarCoordinator;
/// Returns `secondaryToolbarCoordinator`.
- (SecondaryToolbarCoordinator*)secondaryToolbarCoordinator;

/// Returns `primaryToolbarViewController`.
- (UIViewController*)primaryToolbarViewController;
/// Returns `secondaryToolbarViewController`.
- (UIViewController*)secondaryToolbarViewController;

/// Returns the sharing positioner for the current toolbar configuration.
- (id<SharingPositioner>)sharingPositioner;

#pragma mark ViewRevealing

/// A reference to the view controller that implements the view revealing
/// vertical pan handler delegate methods.
- (id<ViewRevealingAnimatee>)viewRevealingAnimatee;
/// Sets the pan gesture handler for the view controller that implements the
/// view revealing.
- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
