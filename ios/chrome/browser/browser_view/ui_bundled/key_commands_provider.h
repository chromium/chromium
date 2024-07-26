// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_KEY_COMMANDS_PROVIDER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_KEY_COMMANDS_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"

@protocol ApplicationCommands;
@protocol BookmarksCommands;
class Browser;
@protocol BrowserCoordinatorCommands;
@protocol FindInPageCommands;
@protocol OmniboxCommands;
@protocol QuickDeleteCommands;
@protocol SettingsCommands;

// Handles the keyboard commands registration and handling for the
// BrowserViewController.
@interface KeyCommandsProvider : UIResponder <KeyCommandActions>

// Key command actions are converted to Chrome commands and sent to these
// handlers.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;
@property(nonatomic, weak) id<FindInPageCommands> findInPageHandler;
@property(nonatomic, weak) id<BookmarksCommands> bookmarksHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Adds the receiver in the chain between the view controller and its original
// next responder.
- (void)respondBetweenViewController:(UIViewController*)viewController
                        andResponder:(UIResponder*)nextResponder;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_KEY_COMMANDS_PROVIDER_H_
