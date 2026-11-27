// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator_app_agent.h"

#import <algorithm>
#import <map>
#import <string>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"
#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_provider_migration_notifier.h"

namespace {

// Maximum age since a passkey's `creation_time` for migration to present a
// confirmation infobar, suppressing stale passkeys created in other apps during
// earlier sessions.
constexpr base::TimeDelta kRecentlyAddedDelay = base::Seconds(5);

// Attempts to present the passkey creation infobar on `scene`'s active browser.
// Returns true if the infobar was added.
bool TryShowPasskeyCreationInfobarInScene(
    SceneState* scene,
    const std::string& account_email,
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  // `scene` is nil when no `foregroundActiveScene` exists, and `browser` or
  // `SettingsCommands` may not yet be wired if `scene` is mid-connection.
  Browser* browser = scene.browserProviderInterface.currentBrowser;
  if (!browser) {
    return false;
  }
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  if (![dispatcher dispatchingForProtocol:@protocol(SettingsCommands)]) {
    return false;
  }
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  return IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      browser->GetWebStateList()->GetActiveWebState(), account_email, passkey,
      settings_handler);
}

}  // namespace

@implementation CredentialProviderMigratorAppAgent {
  CredentialProviderMigrationNotifier* _credentialProviderMigrationNotifier;

  // Maps profile name to the CredentialProviderMigrator responsible for the
  // profile's migration.
  std::map<std::string, CredentialProviderMigrator*, std::less<>> _migratorMap;
}

#pragma mark - SceneObservingAppAgent

- (void)appDidEnterForeground {
  // Migrate credentials when Chrome enters foreground.
  [self migrateCredentialsForAllProfiles];
  [self createCredentialProviderMigrationNotifierIfNeeded];
}

- (void)appDidEnterBackground {
  // Stop any queued passes or deferred `PasskeyModel` waits while Chrome is in
  // the background so Chrome neither races with the Credential Provider
  // Extension on App Group `NSUserDefaults` nor drops the foreground infobar.
  std::vector<CredentialProviderMigrator*> activeMigrators;
  activeMigrators.reserve(_migratorMap.size());
  for (const auto& [profileName, migrator] : _migratorMap) {
    activeMigrators.push_back(migrator);
  }
  for (CredentialProviderMigrator* migrator : activeMigrators) {
    [migrator cancelPendingMigration];
  }
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [super appState:appState didTransitionFromInitStage:previousInitStage];

  // Check if the app is now fully initialized.
  if (appState.initStage == AppInitStage::kFinal) {
    [self migrateCredentialsForAllProfiles];
    [self createCredentialProviderMigrationNotifierIfNeeded];
  }
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

#pragma mark - Private

// Returns whether the app is foregrounded and fully initialized.
- (bool)canMigrate {
  return self.appState.foregroundScenes.count > 0 &&
         self.appState.initStage == AppInitStage::kFinal;
}

// Creates the CredentialProviderMigrationNotifier if the app is ready.
- (void)createCredentialProviderMigrationNotifierIfNeeded {
  if (!_credentialProviderMigrationNotifier && [self canMigrate]) {
    __weak __typeof__(self) weakSelf = self;
    _credentialProviderMigrationNotifier =
        [[CredentialProviderMigrationNotifier alloc] initWithBlock:^() {
          [weakSelf migrateCredentialsForAllProfiles];
        }];
  }
}

// Returns whether multiple profiles have at least one scene connected.
- (BOOL)isMultiProfile {
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

// Presents the passkey creation infobar on at most one foreground scene
// belonging to `profile`, preferring the foreground-active scene.
- (void)showPasskeyCreationInfobarForProfile:(ProfileIOS*)profile
                                     passkey:(const sync_pb::
                                                  WebauthnCredentialSpecifics&)
                                                 passkey {
  // The primary account may have signed out while async migration persistence
  // was in flight.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identityManager) {
    return;
  }

  CoreAccountInfo account =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.email.empty()) {
    return;
  }

  for (ProfileState* profileState in [self.appState profileStates]) {
    if (profileState.profile != profile) {
      continue;
    }

    SceneState* activeScene = profileState.foregroundActiveScene;
    if (TryShowPasskeyCreationInfobarInScene(activeScene, account.email,
                                             passkey)) {
      return;
    }

    for (SceneState* scene in profileState.foregroundScenes) {
      if (scene == activeScene) {
        continue;
      }
      if (TryShowPasskeyCreationInfobarInScene(scene, account.email, passkey)) {
        return;
      }
    }
    return;
  }
}

// Returns the primary identity's GAIA ID for `profile`, or `nil` if signed out.
- (NSString*)primaryGaiaIDForProfile:(ProfileIOS*)profile {
  // `ProfileManagerIOS::GetLoadedProfiles()` only returns regular profiles, so
  // `AuthenticationServiceFactory` always has an instance.
  CHECK(!profile->IsOffTheRecord());
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> identity = authService->GetPrimaryIdentity();
  return identity ? identity.gaiaId.ToNSString() : nil;
}

// Migrates credentials for all loaded profiles.
- (void)migrateCredentialsForAllProfiles {
  // Only attempt to start migrations while the app is foregrounded and fully
  // initialized.
  if (![self canMigrate]) {
    return;
  }

  const std::vector<ProfileIOS*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  for (ProfileIOS* profile : loadedProfiles) {
    auto iter = _migratorMap.find(profile->GetProfileName());
    if (iter != _migratorMap.end()) {
      NSString* activeGaiaID = iter->second.gaiaID;
      NSString* currentGaiaID = [self primaryGaiaIDForProfile:profile];
      BOOL sameGaia =
          (!activeGaiaID && !currentGaiaID) ||
          (activeGaiaID && [activeGaiaID isEqualToString:currentGaiaID]);
      if (sameGaia) {
        [iter->second checkForNewCredentials];
        continue;
      }
      [iter->second cancelPendingMigration];
      if (_migratorMap.contains(profile->GetProfileName())) {
        // An in-flight temporal-store write is still finishing; skip this
        // trigger rather than run two migrators for the same profile.
        continue;
      }
    }
    [self migrateProfile:profile];
  }
}

// Migrates a specific profile.
- (void)migrateProfile:(ProfileIOS*)profile {
  CHECK(profile);

  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

  password_manager::PasswordForm::Store defaultStore =
      password_manager::features_util::IsAccountStorageActive(
          SyncServiceFactory::GetForProfile(profile))
          ? password_manager::PasswordForm::Store::kAccountStore
          : password_manager::PasswordForm::Store::kProfileStore;
  scoped_refptr<password_manager::PasswordStoreInterface> storeToSave =
      defaultStore == password_manager::PasswordForm::Store::kAccountStore
          ? IOSChromeAccountPasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS)
          : IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS);

  NSString* gaiaID = [self primaryGaiaIDForProfile:profile];
  webauthn::PasskeyModel* passkeyModel =
      IOSPasskeyModelFactory::GetForProfile(profile);

  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:userDefaults
                                                           key:key
                                                          gaia:gaiaID
                                                 passwordStore:storeToSave
                                                  passkeyStore:passkeyModel];
  _migratorMap.insert(std::make_pair(profile->GetProfileName(), migrator));

  __weak CredentialProviderMigratorAppAgent* weakSelf = self;
  [migrator startMigrationWithCompletion:
                base::BindOnce(
                    [](CredentialProviderMigratorAppAgent* appAgent,
                       base::WeakPtr<ProfileIOS> weakProfile,
                       const std::string& profileName,
                       CredentialMigrationResult result) {
                      CHECK(result.has_value(), base::NotFatalUntil::M164)
                          << base::SysNSStringToUTF8(
                                 result.error().localizedDescription);
                      [appAgent migrationCompleteForProfile:weakProfile.get()
                                                profileName:profileName
                                                     result:std::move(result)];
                    },
                    weakSelf, profile->AsWeakPtr(), profile->GetProfileName())];
}

// Invoked when `-startMigrationWithCompletion:` completes for a given profile.
// `profile` may be null if the profile was unloaded before completion ran.
- (void)migrationCompleteForProfile:(ProfileIOS*)profile
                        profileName:(const std::string&)profileName
                             result:(CredentialMigrationResult)result {
  auto iter = _migratorMap.find(profileName);
  CHECK(iter != _migratorMap.end());
  _migratorMap.erase(iter);
  if (!profile) {
    return;
  }

  if (result.has_value() && !result->empty()) {
    const auto newestPasskeyIter = std::ranges::max_element(
        *result, {}, &sync_pb::WebauthnCredentialSpecifics::creation_time);
    const base::TimeDelta age =
        base::Time::Now() - base::Time::FromMillisecondsSinceUnixEpoch(
                                newestPasskeyIter->creation_time());
    if (!age.is_negative() && age < kRecentlyAddedDelay) {
      [self showPasskeyCreationInfobarForProfile:profile
                                         passkey:*newestPasskeyIter];
    }
  }
}

@end
