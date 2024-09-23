// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_coordinating.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_height_delegate.h"

@class BubblePresenter;
@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocusDelegate;
@protocol SharingPositioner;

/// Coordinator above primary and secondary toolbars. It does not have a
/// view controller. This object is also an interface between multiple toolbars
/// and the objects which want to interact with them without having to know to
/// which one specifically send the call.
@interface ToolbarCoordinator
    : ChromeCoordinator <FakeboxFocuser,
                         PopupMenuUIUpdating,
                         SideSwipeToolbarSnapshotProviding,
                         ToolbarCoordinating>

/// Delegate for focusing omnibox in `locationBarCoordinator`.
@property(nonatomic, weak) id<OmniboxFocusDelegate> omniboxFocusDelegate;
/// Delegate for presenting the popup in `locationBarCoordinator`.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;
/// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;
// Bubble presenter for displaying IPH bubbles relating to the toolbars.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

/// Initializes this coordinator with its `browser` and a nil base view
/// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
/// Initializes this coordinator with its `browser` and a nil base view
/// controller.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

/// Returns `primaryToolbarViewController`.
- (UIViewController*)primaryToolbarViewController;
/// Returns `secondaryToolbarViewController`.
- (UIViewController*)secondaryToolbarViewController;

/// Returns the sharing positioner for the current toolbar configuration.
- (id<SharingPositioner>)sharingPositioner;

/// Updates the toolbar's appearance.
/// TODO(crbug.com/40842406): Remove this once toolbar coordinator owns focus
/// orchestrator.
- (void)updateToolbar;

/// YES when a prerendered webstate is being inserted into a webStateList.
- (BOOL)isLoadingPrerenderer;

#pragma mark Omnibox and LocationBar

/// Coordinates the location bar focusing/defocusing. For example, initiates
/// transition to the expanded location bar state of the view controller.
- (void)transitionToLocationBarFocusedState:(BOOL)focused
                                 completion:(ProceduralBlock)completion;
/// Whether the omnibox is currently the first responder.
- (BOOL)isOmniboxFirstResponder;
/// Whether the omnibox popup is currently presented.
- (BOOL)showingOmniboxPopup;

#pragma mark ToolbarHeightProviding

/// The minimum height of the primary toolbar.
- (CGFloat)collapsedPrimaryToolbarHeight;
/// The maximum height of the primary toolbar.
- (CGFloat)expandedPrimaryToolbarHeight;
/// The minimum height of the secondary toolbar.
- (CGFloat)collapsedSecondaryToolbarHeight;
/// The maximum height of the secondary toolbar.
- (CGFloat)expandedSecondaryToolbarHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_H_
