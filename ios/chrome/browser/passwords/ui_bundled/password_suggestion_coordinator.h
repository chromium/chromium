// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Delegate for the coordinator.
@protocol PasswordSuggestionCoordinatorDelegate

// Signals the parent coordinator, BrowserCoordinator, to stop
// PasswordSuggestionCoordinator.
- (void)closePasswordSuggestion;

@end

// Presents the password suggestion feature. The content is presented in a
// half-page sheet (or full-page sheet for devices running iOS 14 or earlier),
// where the suggested password, current user email, and accept/deny action
// buttons are displayed.
@interface PasswordSuggestionCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                        passwordSuggestion:(NSString*)passwordSuggestion
                           decisionHandler:
                               (void (^)(BOOL accept))decisionHandler
                                 proactive:(BOOL)proactive
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<PasswordSuggestionCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_COORDINATOR_H_
