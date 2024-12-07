// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_consumer.h"

@interface ManagedProfileCreationMediator () {
  BOOL _canShowBrowsingDataMigration;
}
@end

@implementation ManagedProfileCreationMediator

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
              skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration {
  self = [super init];
  if (self) {
    // TODO(crbug.com/382240108): Enable the ProfileDataMigrationSettings policy
    // here.

    // Merging data is not possible at FRE, nor is separate profiles are not
    // supported, nor if the account is signed in.
    _canShowBrowsingDataMigration =
        !skipBrowsingDataMigration &&
        AreSeparateProfilesForManagedAccountsEnabled() &&
        !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }
  return self;
}

- (void)setKeepBrowsingDataSeparate:(BOOL)keepSeparate {
  _keepBrowsingDataSeparate = keepSeparate;
  [self.consumer setKeepBrowsingDataSeparate:keepSeparate];
}

- (void)setConsumer:(id<ManagedProfileCreationConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  _consumer.canShowBrowsingDataMigration = _canShowBrowsingDataMigration;
  [_consumer setKeepBrowsingDataSeparate:self.keepBrowsingDataSeparate];
}

@end
