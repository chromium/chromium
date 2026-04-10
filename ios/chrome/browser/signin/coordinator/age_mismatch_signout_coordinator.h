// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_COORDINATOR_AGE_MISMATCH_SIGNOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SIGNIN_COORDINATOR_AGE_MISMATCH_SIGNOUT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/signin/ui/age_mismatch_prompt_mode.h"

@protocol SystemIdentity;
@class AgeMismatchSignoutCoordinator;

// Delegate for AgeMismatchSignoutCoordinator.
@protocol AgeMismatchSignoutCoordinatorDelegate <NSObject>

// Called when the coordinator has completed its flow and should be dismissed.
- (void)ageMismatchSignoutCoordinatorWantsToBeStopped:
    (AgeMismatchSignoutCoordinator*)coordinator;

// Called when the user wants to sign in. The view controller is not dismissed
// when this method is called.
- (void)ageMismatchSignoutCoordinatorWantsToSignIn:
    (AgeMismatchSignoutCoordinator*)coordinator;

@end

// Coordinator for the Age Mismatch Prompt.
@interface AgeMismatchSignoutCoordinator : ChromeCoordinator

// Initializes the coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(id<SystemIdentity>)identity
                                      mode:(AgeMismatchPromptMode)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The delegate for this coordinator.
@property(nonatomic, weak) id<AgeMismatchSignoutCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_COORDINATOR_AGE_MISMATCH_SIGNOUT_COORDINATOR_H_
