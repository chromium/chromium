// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"

@interface CredentialProviderMigratorAppAgent ()

// Keep track of the migration status of each browser state.
@property(nonatomic, strong) NSMutableSet<NSString*>* migratingTracker;

@end

@implementation CredentialProviderMigratorAppAgent

// Migrate the password when Chrome comes to foreground.
- (void)appDidEnterForeground {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();

  std::vector<ChromeBrowserState*> loadedBrowserStates =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  if (!self.migratingTracker) {
    self.migratingTracker =
        [NSMutableSet setWithCapacity:loadedBrowserStates.size()];
  }

  for (ChromeBrowserState* browserState : loadedBrowserStates) {
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
                                                   passwordStore:storeToSave];
    [self.migratingTracker addObject:browserStatePathString];
    __weak __typeof__(self) weakSelf = self;
    [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
      DCHECK(success) << error.localizedDescription;
      if (weakSelf) {
        [weakSelf.migratingTracker removeObject:browserStatePathString];
      }
    }];
  }
}

@end
