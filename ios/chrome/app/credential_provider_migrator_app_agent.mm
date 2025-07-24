// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

#import <map>

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_provider_creation_notifier.h"
#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"

@interface CredentialProviderMigratorAppAgent () <PasskeyModelObserverDelegate>

// Invoked when -startMigrationWithCompletion: completes for a given profile.
// The `profile` pointer may be null if the profile has been destroyed before
// the callback is invoked.
- (void)migrationCompleteForProfile:(ProfileIOS*)profile
                        profileName:(const std::string&)profileName;

@end

namespace {

// Helper function that call -migrationCompleteForProfile:... while allowing
// to use base::BindOnce(...) which is safer as is avoid capturing implicitly
// pointer to C++ objects.
void MigrationCompleteForProfile(
    __weak CredentialProviderMigratorAppAgent* app_agent,
    base::WeakPtr<ProfileIOS> weak_profile,
    const std::string& profile_name,
    BOOL success,
    NSError* error) {
  DCHECK(success) << error.localizedDescription;
  [app_agent migrationCompleteForProfile:weak_profile.get()
                             profileName:profile_name];
}

}  // namespace

@implementation CredentialProviderMigratorAppAgent {
  CredentialProviderCreationNotifier* _credentialProviderCreationNotifier;

  // Maps of PasskeyModel to the registered observer.
  std::map<webauthn::PasskeyModel*,
           std::unique_ptr<webauthn::PasskeyModel::Observer>>
      _passkeyModelObservers;

  // Maps profile name to the CredentialProviderMigrator responsible for the
  // profile's migration.
  std::map<std::string, CredentialProviderMigrator*, std::less<>> _migratorMap;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    __weak __typeof__(self) weakSelf = self;
    _credentialProviderCreationNotifier =
        [[CredentialProviderCreationNotifier alloc] initWithBlock:^() {
          [weakSelf migrateCredentialForAllPasskeyModels];
        }];
  }
  return self;
}

#pragma mark - SceneObservingAppAgent

// Migrate the password when Chrome comes to foreground.
- (void)appDidEnterForeground {
  [self migrateCredentialForAllPasskeyModels];
}

#pragma mark - AppStateObserver

// Called when a new ProfileState is connected.
- (void)appState:(AppState*)appState
    profileStateConnected:(ProfileState*)profileState {
  [self updateMultiProfileSetting];
}

// Called when a ProfileState is disconnected.
- (void)appState:(AppState*)appState
    profileStateDisconnected:(ProfileState*)profileState {
  [self updateMultiProfileSetting];
}

#pragma mark - PasskeyModelObserverDelegate

- (void)passKeyModelShuttingDown:(webauthn::PasskeyModel*)passkeyModel {
  [self removeObserverForPasskeyModel:passkeyModel];
}

- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel {
  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  const auto iter =
      std::ranges::find_if(loadedProfiles, [passkeyModel](ProfileIOS* profile) {
        return IOSPasskeyModelFactory::GetForProfile(profile) == passkeyModel;
      });

  if (iter != loadedProfiles.end()) {
    NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
    NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

    [self migrateCredentialForProfile:*iter
                         passKeyModel:passkeyModel
                                  key:key
                         userDefaults:userDefaults];
  }
}

#pragma mark - Private

// Returns whether multiple profiles have at least one scene connected.
- (BOOL)isMultiProfile {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    return NO;
  }

  // Check if we have more than 1 connected profile.
  NSUInteger profileWithScenes = 0;
  for (ProfileState* profileState in self.appState.profileStates) {
    if ([profileState.connectedScenes count] != 0) {
      profileWithScenes++;
    }
  }

  return profileWithScenes > 1;
}

// Updates the CPE's multi profile setting.
- (void)updateMultiProfileSetting {
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:[self isMultiProfile]]
         forKey:AppGroupUserDefaultsCredentialProviderMultiProfileSetting()];
}

// Sets whether the passkey updates are allowed to show an infobar to the user.
// This should normally only happen during the credential migration.
- (void)allowInfobarForProfile:(ProfileIOS*)profile allowed:(BOOL)allowed {
  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kAll)) {
    if (auto* agent = CredentialProviderBrowserAgent::FromBrowser(browser)) {
      agent->SetInfobarAllowed(allowed);
    }
  }
}

// Migrates the credential for all passkey models.
- (void)migrateCredentialForAllPasskeyModels {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  for (ProfileIOS* profile : loadedProfiles) {
    webauthn::PasskeyModel* passkeyModel =
        IOSPasskeyModelFactory::GetForProfile(profile);

    [self migrateCredentialForProfile:profile
                         passKeyModel:passkeyModel
                                  key:key
                         userDefaults:userDefaults];
  }
}

// Migrate the credential for the given profile and model.
- (void)migrateCredentialForProfile:(ProfileIOS*)profile
                       passKeyModel:(webauthn::PasskeyModel*)passkeyModel
                                key:(NSString*)key
                       userDefaults:(NSUserDefaults*)userDefaults {
  CHECK(profile);
  // Do nothing if the migration for the profile already started.
  if (base::Contains(_migratorMap, profile->GetProfileName())) {
    return;
  }

  // If the passkey model isn't ready, delay the migration of passkeys until
  // it is ready.
  if (passkeyModel && !passkeyModel->IsReady()) {
    if (![self isObservingPasskeyModel:passkeyModel]) {
      [self addObserverForPasskeyModel:passkeyModel];
    }
    return;
  }

  password_manager::PasswordForm::Store defaultStore =
      password_manager::features_util::IsAccountStorageEnabled(
          SyncServiceFactory::GetForProfile(profile))
          ? password_manager::PasswordForm::Store::kAccountStore
          : password_manager::PasswordForm::Store::kProfileStore;
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
                                                  passkeyStore:passkeyModel];
  _migratorMap.insert(std::make_pair(profile->GetProfileName(), migrator));
  [self allowInfobarForProfile:profile allowed:YES];

  __weak __typeof__(self) weakSelf = self;
  [migrator startMigrationWithCompletion:base::CallbackToBlock(base::BindOnce(
                                             &MigrationCompleteForProfile,
                                             weakSelf, profile->AsWeakPtr(),
                                             profile->GetProfileName()))];
}

- (void)migrationCompleteForProfile:(ProfileIOS*)profile
                        profileName:(const std::string&)profileName {
  auto iter = _migratorMap.find(profileName);
  CHECK(iter != _migratorMap.end());
  _migratorMap.erase(iter);
  if (!profile) {
    return;
  }

  webauthn::PasskeyModel* passkeyModel =
      IOSPasskeyModelFactory::GetForProfile(profile);
  if ([self isObservingPasskeyModel:passkeyModel]) {
    [self removeObserverForPasskeyModel:passkeyModel];
  }

  [self allowInfobarForProfile:profile allowed:NO];
}

// Returns whether we already own an observer for the provided passkey model.
- (BOOL)isObservingPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  return base::Contains(_passkeyModelObservers, passkeyModel);
}

// Adds an observer for the provided passkey model.
- (void)addObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  CHECK(![self isObservingPasskeyModel:passkeyModel]);
  _passkeyModelObservers.insert(std::make_pair(
      passkeyModel,
      std::make_unique<PasskeyModelObserverBridge>(self, passkeyModel)));
}

// Removes an observer for the provided passkey model.
- (void)removeObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  CHECK([self isObservingPasskeyModel:passkeyModel]);
  _passkeyModelObservers.erase(passkeyModel);
}

@end
