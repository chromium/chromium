// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class LearnMoreCoordinator;

// Delegate for LearnMoreCoordinator, to receive events when the view
// controller is dismissed.
@protocol LearnMoreCoordinatorDelegate <NSObject>
// Called when the LearnMoreCoordinator is dismissed.
- (void)removeLearnMoreCoordinator:(LearnMoreCoordinator*)coordinator;

@end

// Coordinator for the LearnMore dialog.
@interface LearnMoreCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<LearnMoreCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_COORDINATOR_H_
