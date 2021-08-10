// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_util.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_ui_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPasswordMediator ()

// The service identifier the password is being created for,
@property(nonatomic, strong) ASCredentialServiceIdentifier* serviceIdentifier;

// The NSUserDefaults new credentials should be stored to.
@property(nonatomic, strong) NSUserDefaults* userDefaults;

@end

@implementation NewPasswordMediator

- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                   serviceIdentifier:
                       (ASCredentialServiceIdentifier*)serviceIdentifier {
  self = [super init];
  if (self) {
    _userDefaults = userDefaults;
    _serviceIdentifier = serviceIdentifier;
  }
  return self;
}

#pragma mark - NewCredentialHandler

- (void)saveCredentialWithUsername:(NSString*)username
                          password:(NSString*)password
                     shouldReplace:(BOOL)shouldReplace {
  if (!shouldReplace && [self credentialExistsForUsername:username]) {
    [self.uiHandler alertUserCredentialExists];
    return;
  }

  ArchivableCredential* credential =
      [self createNewCredentialWithUsername:username password:password];

  if (!credential) {
    [self.uiHandler alertSavePasswordFailed];
    return;
  }

  [self
      saveNewCredential:credential
             completion:^(NSError* error) {
               if (error) {
                 UpdateUMACountForKey(
                     app_group::kCredentialExtensionSaveCredentialFailureCount);
                 [self.uiHandler alertSavePasswordFailed];
                 return;
               }
               [self userSelectedCredential:credential];
             }];
}

#pragma mark - Private

// Checks whether a credential already exists with the given username.
- (BOOL)credentialExistsForUsername:(NSString*)username {
  NSURL* url = [NSURL URLWithString:self.serviceIdentifier.identifier];
  NSString* recordIdentifier = RecordIdentifierForData(url, username);

  return [self.existingCredentials
      credentialWithRecordIdentifier:recordIdentifier];
}

// Creates a new credential but doesn't add it to any stores.
- (ArchivableCredential*)createNewCredentialWithUsername:(NSString*)username
                                                password:(NSString*)password {
  NSURL* url = [NSURL URLWithString:self.serviceIdentifier.identifier];
  NSString* recordIdentifier = RecordIdentifierForData(url, username);

  NSString* uuid = [[NSUUID UUID] UUIDString];
  if (!StorePasswordInKeychain(password, uuid)) {
    return nil;
  }
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

// Saves the given credential to disk and calls |completion| once the operation
// is finished.
- (void)saveNewCredential:(ArchivableCredential*)credential
               completion:(void (^)(NSError* error))completion {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:self.userDefaults
                       key:key];

  if ([store credentialWithRecordIdentifier:credential.recordIdentifier]) {
    [store updateCredential:credential];
  } else {
    [store addCredential:credential];
  }

  [store saveDataWithCompletion:completion];
}

// Alerts the host app that the user selected a credential.
- (void)userSelectedCredential:(id<Credential>)credential {
  NSString* password =
      PasswordWithKeychainIdentifier(credential.keychainIdentifier);
  ASPasswordCredential* ASCredential =
      [ASPasswordCredential credentialWithUser:credential.user
                                      password:password];
  [self.context completeRequestWithSelectedCredential:ASCredential
                                    completionHandler:nil];
}

@end
