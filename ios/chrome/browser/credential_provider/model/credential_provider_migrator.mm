// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

using password_manager::PasswordStoreInterface;

NSErrorDomain const kCredentialProviderMigratorErrorDomain =
    @"kCredentialProviderMigratorErrorDomain";

typedef enum : NSInteger {
  CredentialProviderMigratorErrorAlreadyRunning,
} CredentialProviderMigratorErrors;

@interface CredentialProviderMigrator ()

// Key used to retrieve the temporal storage.
@property(nonatomic, copy) NSString* key;

// User defaults containing the temporal storage.
@property(nonatomic, copy) NSUserDefaults* userDefaults;

// Temporal store containing the passwords created in CPE extension.
@property(nonatomic, strong) UserDefaultsCredentialStore* temporalStore;

// Password manager store, where passwords will be migrated to.
@property(nonatomic, assign) scoped_refptr<PasswordStoreInterface>
    passwordStore;

@end

@implementation CredentialProviderMigrator

- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                                 key:(NSString*)key
                       passwordStore:(scoped_refptr<PasswordStoreInterface>)
                                         passwordStore {
  self = [super init];
  if (self) {
    _key = key;
    _userDefaults = userDefaults;
    _passwordStore = passwordStore;
  }
  return self;
}

- (void)startMigrationWithCompletion:(void (^)(BOOL success,
                                               NSError* error))completion {
  if (self.temporalStore) {
    NSError* error =
        [NSError errorWithDomain:kCredentialProviderMigratorErrorDomain
                            code:CredentialProviderMigratorErrorAlreadyRunning
                        userInfo:nil];
    completion(NO, error);
    return;
  }
  self.temporalStore = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:self.userDefaults
                       key:self.key];
  NSArray<id<Credential>>* credentials = self.temporalStore.credentials.copy;
  for (id<Credential> credential in credentials) {
    password_manager::PasswordForm form =
        PasswordFormFromCredential(credential);
    self.passwordStore->AddLogin(form);
    [self.temporalStore
        removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  }
  __weak __typeof__(self) weakSelf = self;
  [self.temporalStore saveDataWithCompletion:^(NSError* error) {
    DCHECK(!error);
    weakSelf.temporalStore = nil;
    completion(error == nil, error);
  }];
}

@end
