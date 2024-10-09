// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_provider_creation_notifier.h"
#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"

@interface CredentialProviderMigratorAppAgent () <PasskeyModelObserverDelegate>

// Keep track of the migration status of each profile.
@property(nonatomic, strong) NSMutableSet<NSString*>* migratingTracker;

@property(nonatomic, strong)
    CredentialProviderCreationNotifier* credentialProviderCreationNotifier;

@end

@implementation CredentialProviderMigratorAppAgent {
  std::vector<std::unique_ptr<PasskeyModelObserverBridge>>
      _passkeyModelObservers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    __weak __typeof__(self) weakSelf = self;
    self.credentialProviderCreationNotifier =
        [[CredentialProviderCreationNotifier alloc] initWithBlock:^() {
          [weakSelf credentialMigrationForPasskeyModel:nullptr];
        }];
  }
  return self;
}

// Migrate the password when Chrome comes to foreground.
- (void)appDidEnterForeground {
  [self credentialMigrationForPasskeyModel:nullptr];
}

#pragma mark - PasskeyModelObserverDelegate

- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel {
  [self credentialMigrationForPasskeyModel:passkeyModel];
}

#pragma mark - Private

// Performs the credential migration only for the specified passkey model.
// If passkey_model is nil, the migration is performed for all passkey models.
- (void)credentialMigrationForPasskeyModel:
    (webauthn::PasskeyModel*)passkeyModel {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  if (!self.migratingTracker) {
    self.migratingTracker =
        [NSMutableSet setWithCapacity:loadedProfiles.size()];
  }

  for (ProfileIOS* profile : loadedProfiles) {
    NSString* profilePathString =
        [NSString stringWithCString:profile->GetStatePath()
                                        .BaseName()
                                        .MaybeAsASCII()
                                        .c_str()
                           encoding:NSASCIIStringEncoding];
    // Do nothing if the migration for a profile already started.
    if ([self.migratingTracker containsObject:profilePathString]) {
      continue;
    }

    webauthn::PasskeyModel* passkeyStore =
        IOSPasskeyModelFactory::GetForProfile(profile);
    // If the migration is happening as a result of a passkey model becoming
    // ready, only perform the migration for that specific passkey model.
    if (passkeyModel && passkeyStore != passkeyModel) {
      continue;
    }

    if (!passkeyModel && passkeyStore && !passkeyStore->IsReady()) {
      // If the passkey model isn't ready, delay the migration of passkeys until
      // it is ready.
      if (![self isObservingPasskeyModel:passkeyStore]) {
        [self addObserverForPasskeyModel:passkeyStore];
      }
      // The passkeyStore is set to nullptr here so that the observer just added
      // above won't be removed in the migration's completion block below.
      passkeyStore = nullptr;
    }

    password_manager::PasswordForm::Store defaultStore =
        password_manager::features_util::GetDefaultPasswordStore(
            profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
    scoped_refptr<password_manager::PasswordStoreInterface> storeToSave =
        defaultStore == password_manager::PasswordForm::Store::kAccountStore
            ? IOSChromeAccountPasswordStoreFactory::GetForProfile(
                  profile, ServiceAccessType::IMPLICIT_ACCESS)
            : IOSChromeProfilePasswordStoreFactory::GetForProfile(
                  profile, ServiceAccessType::IMPLICIT_ACCESS);
    CredentialProviderMigrator* migrator =
        [[CredentialProviderMigrator alloc] initWithUserDefaults:userDefaults
                                                             key:key
                                                   passwordStore:storeToSave
                                                    passkeyStore:passkeyStore];
    [self.migratingTracker addObject:profilePathString];
    __weak __typeof__(self) weakSelf = self;
    [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
      DCHECK(success) << error.localizedDescription;
      if (weakSelf) {
        [weakSelf.migratingTracker removeObject:profilePathString];
        if (passkeyStore) {
          [weakSelf removeObserverForPasskeyModel:passkeyStore];
        }
      }
    }];
  }
}

// Returns whether we already own an observer for the provided passkey model.
- (BOOL)isObservingPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  for (const auto& passkeyModelObserver : _passkeyModelObservers) {
    if (passkeyModelObserver->IsObserving(passkeyModel)) {
      return YES;
    }
  }
  return NO;
}

// Adds an observer for the provided passkey model.
- (void)addObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  _passkeyModelObservers.emplace_back(
      std::make_unique<PasskeyModelObserverBridge>(self, passkeyModel));
}

// Removes an observer for the provided passkey model.
- (void)removeObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  auto itEnd = _passkeyModelObservers.end();
  for (auto it = _passkeyModelObservers.begin(); it != itEnd; ++it) {
    if ((*it)->IsObserving(passkeyModel)) {
      // Remove the observer both from the passkey model and from this object.
      passkeyModel->RemoveObserver(it->get());
      _passkeyModelObservers.erase(it);
      return;
    }
  }
}

@end
