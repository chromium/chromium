// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ManagedProfileCreationCoordinator;
@class ManagedProfileCreationMediator;

@protocol ManagedProfileCreationCoordinatorDelegate <NSObject>

- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                didAccept:(BOOL)didAccept
                 keepBrowsingDataSeparate:(BOOL)keepBrowsingDataSeparate;

@end

// Coordinator to present managed profile creation.
@interface ManagedProfileCreationCoordinator : ChromeCoordinator

// Creates a coordinator for a ManagedProfileCreationViewController shown
// in `viewController`. UIViewController::presentViewController will be used
// to show the ViewController created and owned by
// ManagedProfileCreationCoordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser
                 skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<ManagedProfileCreationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_
