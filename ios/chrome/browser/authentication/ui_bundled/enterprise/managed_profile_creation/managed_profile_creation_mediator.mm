// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface ManagedProfileCreationMediator () {
  BOOL _canShowBrowsingDataMigration;
  BOOL _browsingDataMigrationDisabledByPolicy;
}
@end

@implementation ManagedProfileCreationMediator

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
               mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
    browsingDataMigrationDisabledByPolicy:
        (BOOL)browsingDataMigrationDisabledByPolicy {
  self = [super init];
  if (self) {
    // We can merge if either
    // * we are at FRE,
    // * separate profiles are not supported, or
    // * the user is signed-in.
    _canShowBrowsingDataMigration =
        !skipBrowsingDataMigration &&
        AreSeparateProfilesForManagedAccountsEnabled() &&
        !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
    _keepBrowsingDataSeparate = !mergeBrowsingDataByDefault;
    _browsingDataMigrationDisabledByPolicy =
        browsingDataMigrationDisabledByPolicy;
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
  _consumer.browsingDataMigrationDisabledByPolicy =
      _browsingDataMigrationDisabledByPolicy;
  [_consumer setKeepBrowsingDataSeparate:self.keepBrowsingDataSeparate];
}

#pragma mark - BrowsingDataMigrationViewControllerDelegate

- (void)updateShouldKeepBrowsingDataSeparate:(BOOL)keepBrowsingDataSeparate {
  self.keepBrowsingDataSeparate = keepBrowsingDataSeparate;
  [self.consumer setKeepBrowsingDataSeparate:self.keepBrowsingDataSeparate];
}

@end
