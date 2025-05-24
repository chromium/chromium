// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/browsing_data_migration_view_controller.h"

class ChromeAccountManagerService;

@protocol ManagedProfileCreationConsumer;

@protocol ManagedProfileCreationMediatorDelegate <NSObject>

// Called when the identity is removed from the device while the dialog is
// opened.
- (void)identityRemovedFromDevice;

@end

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator that handles the sign-in operation.
@interface ManagedProfileCreationMediator
    : NSObject <BrowsingDataMigrationViewControllerMutator>

// Consumer for this mediator.
@property(nonatomic, weak) id<ManagedProfileCreationConsumer> consumer;

@property(nonatomic, weak) id<ManagedProfileCreationMediatorDelegate> delegate;

// If `browsingDataSeparate` is `YES`, the managed account gets signed in to
// a new empty work profile.
// If `browsingDataSeparate` is `NO`, the account gets signed in to the
// current profile. This involves converting the current profile into a work
// profile.
@property(nonatomic, assign) BOOL browsingDataSeparate;

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                    accountManagerService:
                        (ChromeAccountManagerService*)accountManagerService
                skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
               mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
    browsingDataMigrationDisabledByPolicy:
        (BOOL)browsingDataMigrationDisabledByPolicy
                                   gaiaID:(NSString*)gaiaID
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_
