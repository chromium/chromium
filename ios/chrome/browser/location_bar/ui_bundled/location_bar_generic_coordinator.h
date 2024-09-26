// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_GENERIC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_GENERIC_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"

class WebStateList;
@class CommandDispatcher;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol EditViewAnimatee;
@protocol LocationBarAnimatee;
@protocol OmniboxPopupPresenterDelegate;
@protocol OmniboxFocusDelegate;

@protocol LocationBarGenericCoordinator <NSObject,
                                         LocationBarURLLoader,
                                         OmniboxCommands>

// Command dispatcher.
@property(nonatomic, strong) CommandDispatcher* commandDispatcher;
// View containing the omnibox.
@property(nonatomic, strong, readonly) UIView* view;
// Weak reference to ProfileIOS;
@property(nonatomic, assign) ProfileIOS* profile;
// The dispatcher for this view controller.
@property(nonatomic, weak) CommandDispatcher* dispatcher;
// Delegate for this coordinator.
// TODO(crbug.com/41363340): Change this.
@property(nonatomic, weak) id<OmniboxFocusDelegate> delegate;
// The web state list this ToolbarCoordinator is handling.
@property(nonatomic, assign) WebStateList* webStateList;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    popupPresenterDelegate;

// Start this coordinator.
- (void)start;
// Stop this coordinator.
- (void)stop;

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

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_GENERIC_COORDINATOR_H_
