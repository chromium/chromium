// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/enterprise/managed_profile_creation/ui/browsing_data_migration_view_controller.h"
#import "ios/chrome/browser/authentication/enterprise/managed_profile_creation/ui/managed_profile_creation_view_controller.h"

class ChromeAccountManagerService;
class GaiaId;

@protocol ManagedProfileCreationConsumer;
@class ManagedProfileCreationMediator;

namespace signin {
enum class ManagedAccountSigninMode;
}  // namespace signin

@protocol ManagedProfileCreationMediatorDelegate <NSObject>

// Called when the Managed Profile Creation must be stopped.
// This should be very rare. It can occurs either if the profile was removed
// from the device (this could be done from another application), or if the user
// signed-in into an account (this could be done by using a second view, with a
// managed profile, and switching to a personal profile).
- (void)managedProfileCreationMediatorWantsToBeStopped:
    (ManagedProfileCreationMediator*)mediator;

@end

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator that handles the sign-in operation.
@interface ManagedProfileCreationMediator
    : NSObject <BrowsingDataMigrationViewControllerMutator,
                ManagedProfileCreationDataSource>

// Consumer for this mediator.
@property(nonatomic, weak) id<ManagedProfileCreationConsumer> consumer;

@property(nonatomic, weak) id<ManagedProfileCreationMediatorDelegate> delegate;

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
                       mode:(signin::ManagedAccountSigninMode)mode
                     gaiaID:(const GaiaId&)gaiaID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_MEDIATOR_H_
