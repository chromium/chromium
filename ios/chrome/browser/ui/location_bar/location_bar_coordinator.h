// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"

@class CommandDispatcher;
@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@protocol EditViewAnimatee;
@protocol LocationBarAnimatee;
@protocol OmniboxPopupPresenterDelegate;
@protocol ToolbarCoordinatorDelegate;

// Location bar coordinator.
@interface LocationBarCoordinator
    : NSObject<LocationBarURLLoader, OmniboxFocuser>

// Command dispatcher.
@property(nonatomic, strong) CommandDispatcher* commandDispatcher;
// View controller containing the omnibox.
@property(nonatomic, strong, readonly)
    UIViewController* locationBarViewController;
// The location bar's Browser.
@property(nonatomic, assign) Browser* browser;
// The dispatcher for this view controller.
@property(nonatomic, weak) CommandDispatcher* dispatcher;
// Delegate for this coordinator.
// TODO(crbug.com/799446): Change this.
@property(nonatomic, weak) id<ToolbarCoordinatorDelegate> delegate;

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

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_COORDINATOR_H_
