// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

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

namespace {

// This class observes a passkey model which is not ready yet with the sole
// purpose of re-triggering the passkey migration when the passkey model becomes
// ready.
class CredentialProviderMigratorPasskeyModelObserver
    : public webauthn::PasskeyModel::Observer {
 public:
  CredentialProviderMigratorPasskeyModelObserver(
      CredentialProviderMigratorAppAgent* agent,
      webauthn::PasskeyModel* passkey_model)
      : agent_(agent), passkey_model_(passkey_model) {
    passkey_model_->AddObserver(this);
  }

  bool isObserving(webauthn::PasskeyModel* passkey_model) const {
    return passkey_model == passkey_model_;
  }

 private:
  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override {}
  void OnPasskeyModelShuttingDown() override {}
  void OnPasskeyModelIsReady(bool is_ready) override {
    // When the initial credential migration was attempted, the passkey model
    // wasn't ready. Now that it is ready, call that function again so that the
    // passkeys get properly migrated.
    [agent_ credentialMigrationForPasskeyModel:passkey_model_];
  }

  // The agent which can trigger the passkey migration.
  __weak CredentialProviderMigratorAppAgent* agent_;

  // The passkey model being observed.
  webauthn::PasskeyModel* passkey_model_;
};

}  // namespace

@interface CredentialProviderMigratorAppAgent ()

// Keep track of the migration status of each browser state.
@property(nonatomic, strong) NSMutableSet<NSString*>* migratingTracker;

@end

@implementation CredentialProviderMigratorAppAgent {
  std::vector<std::unique_ptr<CredentialProviderMigratorPasskeyModelObserver>>
      _passkeyModelObservers;
}

// Migrate the password when Chrome comes to foreground.
- (void)appDidEnterForeground {
  [self credentialMigrationForPasskeyModel:nullptr];
}

#pragma mark - Private

// Performs the credential migration only for the specified passkey model.
// If passkey_model is nil, the migration is performed for all passkey models.
- (void)credentialMigrationForPasskeyModel:
    (webauthn::PasskeyModel*)passkeyModel {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

  const std::vector<ChromeBrowserState*> loadedProfiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  if (!self.migratingTracker) {
    self.migratingTracker =
        [NSMutableSet setWithCapacity:loadedProfiles.size()];
  }

  for (ChromeBrowserState* browserState : loadedProfiles) {
    NSString* browserStatePathString =
        [NSString stringWithCString:browserState->GetStatePath()
                                        .BaseName()
                                        .MaybeAsASCII()
                                        .c_str()
                           encoding:NSASCIIStringEncoding];
    // Do nothing if the migration for a browser state already started.
    if ([self.migratingTracker containsObject:browserStatePathString]) {
      continue;
    }

    webauthn::PasskeyModel* passkeyStore =
        IOSPasskeyModelFactory::GetForBrowserState(browserState);
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
            browserState->GetPrefs(),
            SyncServiceFactory::GetForBrowserState(browserState));
    scoped_refptr<password_manager::PasswordStoreInterface> storeToSave =
        defaultStore == password_manager::PasswordForm::Store::kAccountStore
            ? IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
                  browserState, ServiceAccessType::IMPLICIT_ACCESS)
            : IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
                  browserState, ServiceAccessType::IMPLICIT_ACCESS);
    CredentialProviderMigrator* migrator =
        [[CredentialProviderMigrator alloc] initWithUserDefaults:userDefaults
                                                             key:key
                                                   passwordStore:storeToSave
                                                    passkeyStore:passkeyStore];
    [self.migratingTracker addObject:browserStatePathString];
    __weak __typeof__(self) weakSelf = self;
    [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
      DCHECK(success) << error.localizedDescription;
      if (weakSelf) {
        [weakSelf.migratingTracker removeObject:browserStatePathString];
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
    if (passkeyModelObserver->isObserving(passkeyModel)) {
      return YES;
    }
  }
  return NO;
}

// Adds an observer for the provided passkey model.
- (void)addObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  _passkeyModelObservers.push_back(
      std::make_unique<CredentialProviderMigratorPasskeyModelObserver>(
          self, passkeyModel));
}

// Removes an observer for the provided passkey model.
- (void)removeObserverForPasskeyModel:(webauthn::PasskeyModel*)passkeyModel {
  auto itEnd = _passkeyModelObservers.end();
  for (auto it = _passkeyModelObservers.begin(); it != itEnd; ++it) {
    if ((*it)->isObserving(passkeyModel)) {
      // Remove the observer both from the passkey model and from this object.
      passkeyModel->RemoveObserver(it->get());
      _passkeyModelObservers.erase(it);
      return;
    }
  }
}

@end
