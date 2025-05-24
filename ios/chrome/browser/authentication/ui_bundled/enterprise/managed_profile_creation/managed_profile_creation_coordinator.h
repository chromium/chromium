// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ManagedProfileCreationCoordinator;
@class ManagedProfileCreationMediator;
@protocol SystemIdentity;

@protocol ManagedProfileCreationCoordinatorDelegate <NSObject>

// Called when the user accepted to continue to sign-in with a managed account.
// `accepted` is YES when the user confirmed or NO if the user canceled.
// If `browsingDataSeparate` is `YES`, the managed account gets signed in to
// a new empty work profile. This must only be specified if
// AreSeparateProfilesForManagedAccountsEnabled() is true.
// If `browsingDataSeparate` is `NO`, the account gets signed in to the
// current profile. If AreSeparateProfilesForManagedAccountsEnabled() is true,
// this involves converting the current profile into a work profile.
- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                didAccept:(BOOL)didAccept
                     browsingDataSeparate:(BOOL)browsingDataSeparate;

@end

// Coordinator to present managed profile creation.
@interface ManagedProfileCreationCoordinator : ChromeCoordinator

// Creates a coordinator for a ManagedProfileCreationViewController shown
// in `viewController`. UIViewController::presentViewController will be used
// to show the ViewController created and owned by
// ManagedProfileCreationCoordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                  identity:(id<SystemIdentity>)identity
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser
                 skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
                mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
     browsingDataMigrationDisabledByPolicy:
         (BOOL)browsingDataMigrationDisabledByPolicy NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<ManagedProfileCreationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_COORDINATOR_H_
