// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_state_provider.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"

@protocol BrowserCoordinatorCommands;
@protocol EditViewAnimatee;
@protocol FakeboxButtonsSnapshotProvider;
@protocol LocationBarAnimatee;
@class LocationBarCoordinator;
@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocusDelegate;
@protocol ToolbarOmniboxConsumer;

// Delegate for height change.
@protocol LocationBarCoordinatorHeightDelegate <NSObject>

// Location bar in edit state required `height` changed.
- (void)locationBarCoordinator:(LocationBarCoordinator*)coordinator
      didChangeEditStateHeight:(CGFloat)height;

@end

// Location bar coordinator.
@interface LocationBarCoordinator : ChromeCoordinator <LocationBarURLLoader,
                                                       OmniboxCommands,
                                                       OmniboxStateProvider>

// View controller containing the omnibox.
@property(nonatomic, strong, readonly)
    UIViewController* locationBarViewController;
// Delegate for this coordinator.
// TODO(crbug.com/41363340): Change this.
@property(nonatomic, weak) id<OmniboxFocusDelegate> delegate;
// Delegate for height changes.
@property(nonatomic, weak) id<LocationBarCoordinatorHeightDelegate>
    heightDelegate;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Indicates whether the popup has results to show or not.
- (BOOL)omniboxPopupHasAutocompleteResults;

// Indicates if the omnibox currently displays a popup with suggestions.
- (BOOL)showingOmniboxPopup;

// Indicates when the omnibox is the first responder.
- (BOOL)isOmniboxFirstResponder;

// Returns the location bar animatee.
- (id<LocationBarAnimatee>)locationBarAnimatee;

// Returns the edit view animatee.
- (id<EditViewAnimatee>)editViewAnimatee;

// Target to forward omnibox-related scribble events to.
- (UIResponder<UITextInput>*)omniboxScribbleForwardingTarget;

// Returns the toolbar omnibox consumer.
- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer;

// Sets an object to provide a snapshot of the fakebox buttons to be used during
// focus and defocus transitions.
- (void)setFakeboxButtonsSnapshotProvider:
    (id<FakeboxButtonsSnapshotProvider>)provider;

// Sets whether Lens overlay is currently visible.
- (void)setLensOverlayVisible:(BOOL)lensOverlayVisible;

// Sets command dispatcher for page action menu entry point.
- (void)setPageActionMenuEntryPointDispatcher;

// Creates a visual copy of the location bar steady view.
- (UIView*)locationBarSteadyViewVisualCopy;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_
