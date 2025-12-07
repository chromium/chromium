// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ManagedProfileLearnMoreCoordinator;

// Delegate for ManagedProfileLearnMoreCoordinator, to receive events when the
// view controller is dismissed.
@protocol ManagedProfileLearnMoreCoordinatorDelegate <NSObject>
// Called when the ManagedProfileLearnMoreCoordinator is dismissed.
- (void)removeLearnMoreCoordinator:
    (ManagedProfileLearnMoreCoordinator*)coordinator;

@end

// Coordinator for the LearnMore dialog.
@interface ManagedProfileLearnMoreCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ManagedProfileLearnMoreCoordinatorDelegate>
    delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_COORDINATOR_H_
