// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"

@protocol BookmarksCommands;
class Browser;

// Handles the keyboard commands registration and handling for the
// BrowserViewController.
@interface KeyCommandsProvider : NSObject

@property(nonatomic, weak) UIViewController* baseViewController;
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              BrowserCoordinatorCommands,
                              FindInPageCommands>
    dispatcher;

@property(nonatomic, weak) id<BookmarksCommands> bookmarksCommandsHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;

// Set this flag to YES when the key shortcut bound to Escape key that dismisses
// modals should be enabled.
@property(nonatomic, assign) BOOL canDismissModals;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (NSArray*)keyCommandsWithEditingText:(BOOL)editingText;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
