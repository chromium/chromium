// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_WELCOME_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_WELCOME_SCREEN_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class PasskeyWelcomeScreenCoordinator;

// Delegate for the PasskeyWelcomeScreenCoordinator.
@protocol PasskeyWelcomeScreenCoordinatorDelegate <NSObject>

// Requests the delegate to dismiss the coordinator.
- (void)passkeyWelcomeScreenCoordinatorWantsToBeDismissed:
    (PasskeyWelcomeScreenCoordinator*)coordinator;

@end

// Coordinator for the passkey welcome screen.
@interface PasskeyWelcomeScreenCoordinator : ChromeCoordinator

// Delegate for the coordinator.
@property(nonatomic, weak) id<PasskeyWelcomeScreenCoordinatorDelegate> delegate;

// Designated initializer. `purpose` indicates the purpose for which the passkey
// welcome screen needs to be shown. `completion` is the block to execute when
// the screen is dismissed.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                       purpose:(webauthn::PasskeyWelcomeScreenPurpose)purpose
                    completion:(webauthn::PasskeyWelcomeScreenAction)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Dismisses the view with a `completion` block before stopping the coordinator.
- (void)stopWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_PASSKEY_WELCOME_SCREEN_COORDINATOR_H_
