// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace safe_browsing {
enum class WarningAction;
}

@protocol PasswordProtectionCoordinatorDelegate;

// Presents and stops the Password Protection feature.
@interface PasswordProtectionCoordinator : ChromeCoordinator

@property id<PasswordProtectionCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               warningText:(NSString*)warningText
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Presents the password protection UI. `completion` should be called when the
// UI is dismissed with the user's `action`.
- (void)startWithCompletion:(void (^)(safe_browsing::WarningAction))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_PROTECTION_COORDINATOR_H_
