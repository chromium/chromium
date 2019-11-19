// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"

@protocol KeyCommandsPlumbing <NSObject>

#pragma mark Query information

// Whether the current profile is off-the-record.
- (BOOL)isOffTheRecord;

// Whether the Find in Page is available on current page. For example it's not
// supported on NTP and other native content pages.
- (BOOL)isFindInPageAvailable;

// Returns the current number of tabs.
- (NSUInteger)tabsCount;

// Whether navigation to the previous page is available.
- (BOOL)canGoBack;

// Whether navigation to the next page is available.
- (BOOL)canGoForward;

#pragma mark Call for action

// Called to put the tab at index in focus.
- (void)focusTabAtIndex:(NSUInteger)index;

// Called to focus the next tab.
- (void)focusNextTab;

// Called to focus the previous tab.
- (void)focusPreviousTab;

// Called to reopen the last closed tab.
- (void)reopenClosedTab;

@end

// Handles the keyboard commands registration and handling for the
// BrowserViewController.
@interface KeyCommandsProvider : NSObject

- (NSArray*)
    keyCommandsForConsumer:(id<KeyCommandsPlumbing>)consumer
        baseViewController:(UIViewController*)baseViewController
                dispatcher:
                    (id<ApplicationCommands, BrowserCommands, OmniboxFocuser>)
                        dispatcher
               editingText:(BOOL)editingText;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_KEY_COMMANDS_PROVIDER_H_
