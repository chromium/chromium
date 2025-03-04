// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_widget_updater.h"

#import "base/task/single_thread_task_runner.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/widget_kit/model/model_swift.h"  // nogncheck
#endif

AccountWidgetUpdater::AccountWidgetUpdater(
    SystemIdentityManager* system_identity_manager)
    : system_identity_manager_(system_identity_manager) {
  system_identity_manager_observation_.Observe(system_identity_manager_);
  HandleMigrationIfNeeded();
}

AccountWidgetUpdater::~AccountWidgetUpdater() = default;

void AccountWidgetUpdater::OnIdentityListChanged() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AccountWidgetUpdater::UpdateLoadedAccounts,
                                weak_ptr_factory_.GetWeakPtr()));
}

void AccountWidgetUpdater::OnIdentityUpdated(id<SystemIdentity> identity) {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSMutableDictionary* accounts =
      [[shared_defaults objectForKey:app_group::kAccountsOnDevice] mutableCopy];
  if (!accounts) {
    accounts = [[NSMutableDictionary alloc] init];
  }

  StoreIdentityDataInDict(accounts, identity);

  [shared_defaults setObject:accounts forKey:app_group::kAccountsOnDevice];
}

void AccountWidgetUpdater::OnIdentityRefreshTokenUpdated(
    id<SystemIdentity> identity) {}

void AccountWidgetUpdater::OnIdentityAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {}

SystemIdentityManager::IteratorResult
AccountWidgetUpdater::StoreIdentityDataInDict(NSMutableDictionary* dictionary,
                                              id<SystemIdentity> identity) {
  NSMutableDictionary* account = [[NSMutableDictionary alloc] init];
  [account setObject:identity.userEmail forKey:app_group::kEmail];
  // Add the account to the dictionary of accounts.
  [dictionary setObject:account forKey:identity.gaiaID];

  // Save avatar info to disk.
  NSString* file_name = [NSString stringWithFormat:@"%@.png", identity.gaiaID];
  NSURL* identity_file =
      [app_group::WidgetsAvatarFolder() URLByAppendingPathComponent:file_name];

  if (UIImage* image =
          system_identity_manager_->GetCachedAvatarForIdentity(identity)) {
    NSData* png_data = UIImagePNGRepresentation(image);
    [png_data writeToURL:identity_file atomically:YES];
  } else {
    [[NSFileManager defaultManager] removeItemAtURL:identity_file error:nil];
  }

  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

void AccountWidgetUpdater::UpdateLoadedAccounts() {
  NSMutableDictionary* accounts = [[NSMutableDictionary alloc] init];

  // base::Unretained(...) is safe because the callback is
  // called synchronously from IterateOverIdentities(...).
  system_identity_manager_->IterateOverIdentities(
      base::BindRepeating(&AccountWidgetUpdater::StoreIdentityDataInDict,
                          base::Unretained(this), accounts));

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  [shared_defaults setObject:accounts forKey:app_group::kAccountsOnDevice];

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
  [WidgetTimelinesUpdater reloadAllTimelines];
#endif
}

void AccountWidgetUpdater::HandleMigrationIfNeeded() {
#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  bool migration_performed =
      local_state->GetBoolean(prefs::kMigrateWidgetsPrefs);
  // Don't migrate prefs again if migration was already performed.
  if (migration_performed) {
    return;
  }
  local_state->SetBoolean(prefs::kMigrateWidgetsPrefs, true);
  UpdateLoadedAccounts();
#endif
}
