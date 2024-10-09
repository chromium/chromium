// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

using password_manager::PasswordStoreInterface;

NSErrorDomain const kCredentialProviderMigratorErrorDomain =
    @"kCredentialProviderMigratorErrorDomain";

typedef enum : NSInteger {
  CredentialProviderMigratorErrorAlreadyRunning,
} CredentialProviderMigratorErrors;

@interface CredentialProviderMigrator () {
  // Passkey store.
  raw_ptr<webauthn::PasskeyModel> _passkeyStore;
}

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
                       passwordStore:
                           (scoped_refptr<PasswordStoreInterface>)passwordStore
                        passkeyStore:(webauthn::PasskeyModel*)passkeyStore {
  self = [super init];
  if (self) {
    _key = key;
    _userDefaults = userDefaults;
    _passwordStore = passwordStore;
    _passkeyStore = passkeyStore;
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
    if (credential.isPasskey) {
      // If this happens too early (before the passkey store is ready), the
      // migration will be re-triggered later for that passkey store by
      // CredentialProviderMigratorAppAgent.
      if (!_passkeyStore || !_passkeyStore->IsReady()) {
        continue;
      }

      std::string rpId = base::SysNSStringToUTF8(credential.rpId);
      std::string credentialId(
          static_cast<const char*>(credential.credentialId.bytes),
          credential.credentialId.length);
      std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
          _passkeyStore->GetPasskeyByCredentialId(rpId, credentialId);
      if (credential_specifics) {
        // If the passkey already exists, only update its last used time, and
        // only do so if it's newer.
        if (credential_specifics->last_used_time_windows_epoch_micros() <
            credential.lastUsedTime) {
          _passkeyStore->UpdatePasskeyTimestamp(
              credentialId, base::Time::FromDeltaSinceWindowsEpoch(
                                base::Microseconds(credential.lastUsedTime)));
        }
      } else {
        sync_pb::WebauthnCredentialSpecifics passkey =
            PasskeyFromCredential(credential);
        _passkeyStore->CreatePasskey(passkey);
      }
    } else {
      password_manager::PasswordForm form =
          PasswordFormFromCredential(credential);
      self.passwordStore->AddLogin(form);
    }
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
