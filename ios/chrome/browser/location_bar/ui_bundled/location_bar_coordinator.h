// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"

@class BubblePresenter;
@protocol BrowserCoordinatorCommands;
@protocol EditViewAnimatee;
@protocol LocationBarAnimatee;
@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocusDelegate;
@protocol ToolbarOmniboxConsumer;

// Location bar coordinator.
@interface LocationBarCoordinator
    : ChromeCoordinator <LocationBarURLLoader, OmniboxCommands>

// View controller containing the omnibox.
@property(nonatomic, strong, readonly)
    UIViewController* locationBarViewController;
// Delegate for this coordinator.
// TODO(crbug.com/41363340): Change this.
@property(nonatomic, weak) id<OmniboxFocusDelegate> delegate;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;

// Bubble presenter for displaying IPH bubbles relating to the toolbars.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

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

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_COORDINATOR_H_
