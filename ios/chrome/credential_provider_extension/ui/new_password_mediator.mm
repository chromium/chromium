// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/credential_provider_extension/password_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPasswordMediator ()

// The service identifier the password is being created for,
@property(nonatomic, strong) ASCredentialServiceIdentifier* serviceIdentifier;

@end

@implementation NewPasswordMediator

- (instancetype)initWithServiceIdentifier:
    (ASCredentialServiceIdentifier*)serviceIdentifier {
  self = [super init];
  if (self) {
    _serviceIdentifier = serviceIdentifier;
  }
  return self;
}

#pragma mark - NewCredentialHandler

- (ArchivableCredential*)createNewCredentialWithUsername:(NSString*)username
                                                password:(NSString*)password {
  NSString* uuid = [[NSUUID UUID] UUIDString];
  if (!StorePasswordInKeychain(password, uuid)) {
    return nil;
  }

  NSURL* url = [NSURL URLWithString:self.serviceIdentifier.identifier];
  NSString* recordIdentifier =
      [NSString stringWithFormat:@"CPE|%@|%@|%@", url.host, username,
                                 self.serviceIdentifier.identifier];
  NSString* validationIdentifier =
      AppGroupUserDefaultsCredentialProviderUserID();

  return [[ArchivableCredential alloc]
           initWithFavicon:nil
        keychainIdentifier:uuid
                      rank:1
          recordIdentifier:recordIdentifier
         serviceIdentifier:self.serviceIdentifier.identifier
               serviceName:url.host
                      user:username
      validationIdentifier:validationIdentifier];
}

- (void)saveNewCredential:(ArchivableCredential*)credential
               completion:(void (^)(NSError* error))completion {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:app_group::GetGroupUserDefaults()
                       key:key];

  [store addCredential:credential];
  [store saveDataWithCompletion:completion];
}

@end
