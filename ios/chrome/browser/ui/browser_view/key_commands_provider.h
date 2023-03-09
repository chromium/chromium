// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"

@protocol BookmarksCommands;
class Browser;

// Handles the keyboard commands registration and handling for the
// BrowserViewController.
@interface KeyCommandsProvider : UIResponder <KeyCommandActions>

// Key command actions are converted to Chrome commands and sent to these
// handlers.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, FindInPageCommands>
        dispatcher;
@property(nonatomic, weak) id<BookmarksCommands> bookmarksCommandsHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Adds the receiver in the chain between the view controller and its original
// next responder.
- (void)respondBetweenViewController:(UIViewController*)viewController
                        andResponder:(UIResponder*)nextResponder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
