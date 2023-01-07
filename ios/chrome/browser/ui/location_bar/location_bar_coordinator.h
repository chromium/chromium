// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"

@protocol BrowserCoordinatorCommands;
@protocol EditViewAnimatee;
@protocol LocationBarAnimatee;
@protocol OmniboxPopupPresenterDelegate;
@protocol ToolbarCoordinatorDelegate;

// Location bar coordinator.
@interface LocationBarCoordinator
    : ChromeCoordinator <LocationBarURLLoader, OmniboxCommands>

// View controller containing the omnibox.
@property(nonatomic, strong, readonly)
    UIViewController* locationBarViewController;
// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
