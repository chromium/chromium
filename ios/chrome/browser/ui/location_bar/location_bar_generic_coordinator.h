// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_GENERIC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_GENERIC_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"

class ChromeBrowserState;
class WebStateList;
@class CommandDispatcher;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol EditViewAnimatee;
@protocol LocationBarAnimatee;
@protocol OmniboxPopupPresenterDelegate;
@protocol ToolbarCoordinatorDelegate;

@protocol LocationBarGenericCoordinator <NSObject,
                                         LocationBarURLLoader,
                                         OmniboxCommands>

// Command dispatcher.
@property(nonatomic, strong) CommandDispatcher* commandDispatcher;
// View containing the omnibox.
@property(nonatomic, strong, readonly) UIView* view;
// Weak reference to ChromeBrowserState;
@property(nonatomic, assign) ChromeBrowserState* browserState;
// The dispatcher for this view controller.
@property(nonatomic, weak) CommandDispatcher* dispatcher;
// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;
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

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_GENERIC_COORDINATOR_H_
