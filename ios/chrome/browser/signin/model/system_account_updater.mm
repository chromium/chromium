// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_account_updater.h"

#import "base/check_is_test.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/model/model_swift.h"  // nogncheck
#endif

namespace {

// Updates all widget timelines with the updated data.
void ReloadAllTimelines() {
  if (IsWidgetsForMultiprofileEnabled()) {
#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
    [WidgetTimelinesUpdater reloadAllTimelines];
#endif
  }
}

UIImage* ResizedAvatar(UIImage* image) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  // Resize the avatar image.
  CGSize new_size = CGSizeMake(32.0, 32.0);
  if (!CGSizeEqualToSize(image.size, new_size)) {
    image = ResizeImage(image, new_size, ProjectionMode::kAspectFit);
  }
  return image;
}

// Save avatar info to disk.
void StoreAvatarToDisk(NSURL* identity_file, UIImage* avatar) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  CHECK(avatar, base::NotFatalUntil::M141);
  UIImage* resized_avatar = ResizedAvatar(avatar);
  NSData* png_data = UIImagePNGRepresentation(resized_avatar);
  if (png_data) {
    [png_data writeToURL:identity_file atomically:YES];
  }
}

// Remove legacy avatar data from disk.
void RemoveAvatarDataFromDisk(NSDictionary* avatars) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSURL* avatars_folder = app_group::WidgetsAvatarFolder();
  if (!avatars_folder) {
    return;
  }
  NSFileManager* manager = [NSFileManager defaultManager];
  NSArray<NSURL*>* contents =
      [manager contentsOfDirectoryAtURL:avatars_folder
             includingPropertiesForKeys:nil
                                options:NSDirectoryEnumerationSkipsHiddenFiles
                                  error:nil];

  for (NSURL* url in contents) {
    if ([url.pathExtension.lowercaseString isEqualToString:@"png"]) {
      NSString* file_name =
          [[url lastPathComponent] stringByDeletingPathExtension];
      if (avatars[file_name] == nil) {
        [manager removeItemAtURL:url error:nil];
      }
    }
  }
}

// Remove a single avatar file from disk.
void RemoveSingleAvatarFromDisk(NSURL* url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  [NSFileManager.defaultManager removeItemAtURL:url error:nil];
}

// Save avatars info to disk.
void UpdateAvatars(NSDictionary* avatars) {
  for (NSString* gaia in avatars) {
    NSString* file_name = [NSString stringWithFormat:@"%@.png", gaia];
    NSURL* identity_file = [app_group::WidgetsAvatarFolder()
        URLByAppendingPathComponent:file_name];
    UIImage* avatar = avatars[gaia];
    StoreAvatarToDisk(identity_file, avatar);
  }
  // Check if disk cleanup in WidgetsAvatarFolder folder is needed.
  RemoveAvatarDataFromDisk(avatars);
}

}  // namespace

SystemAccountUpdater::SystemAccountUpdater(
    SystemIdentityManager* system_identity_manager)
    : system_identity_manager_(system_identity_manager) {
  system_identity_manager_observation_.Observe(system_identity_manager_);
  HandleMigrationIfNeeded();
}

SystemAccountUpdater::~SystemAccountUpdater() = default;

void SystemAccountUpdater::OnIdentityListChanged() {
  UpdateLoadedAccounts();
}

void SystemAccountUpdater::OnIdentityUpdated(id<SystemIdentity> identity) {
  UIImage* avatar =
      system_identity_manager_->GetCachedAvatarForIdentity(identity);

  NSString* file_name = [NSString stringWithFormat:@"%@.png", identity.gaiaID];
  NSURL* identity_file =
      [app_group::WidgetsAvatarFolder() URLByAppendingPathComponent:file_name];

  if (!avatar) {
    // No avatar available, remove any existing avatar file for this identity
    // Note: If this task is skipped, the avatar file will remain on disk
    // temporarily but will be cleaned up automatically during the next UpdateLoadedAccounts()
    // call via RemoveAvatarDataFromDisk().
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&RemoveSingleAvatarFromDisk, identity_file),
        base::BindOnce(&ReloadAllTimelines));
    return;
  }

  // Update the identity avatar info on disk and update widgets.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&StoreAvatarToDisk, identity_file, avatar),
      base::BindOnce(&ReloadAllTimelines));
}

#pragma mark - Private

// Callback for SystemIdentityManager::IterateOverIdentities().
SystemIdentityManager::IteratorResult SystemAccountUpdater::IdentitiesOnDevice(
    NSMutableDictionary* accounts,
    NSMutableDictionary* avatars,
    id<SystemIdentity> identity) {
  NSMutableDictionary* account = [[NSMutableDictionary alloc] init];
  [account setObject:identity.userEmail forKey:app_group::kEmail];
  [account setObject:identity.userFullName ?: @"" forKey:app_group::kFullName];
  // Add the account to the dictionary of accounts.
  [accounts setObject:account forKey:identity.gaiaID];

  UIImage* avatar =
      system_identity_manager_->GetCachedAvatarForIdentity(identity);
  // Add the avatar info to the dictionary of avatars.
  if (avatar) {
    [avatars setObject:avatar forKey:identity.gaiaID];
  }

  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

void SystemAccountUpdater::UpdateLoadedAccounts() {
  NSMutableDictionary* accounts = [[NSMutableDictionary alloc] init];
  NSMutableDictionary* avatars = [[NSMutableDictionary alloc] init];

  // base::Unretained(...) is safe because the callback is
  // called synchronously from IterateOverIdentities(...).
  system_identity_manager_->IterateOverIdentities(
      base::BindRepeating(&SystemAccountUpdater::IdentitiesOnDevice,
                          base::Unretained(this), accounts, avatars));

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  [shared_defaults setObject:accounts forKey:app_group::kAccountsOnDevice];
  NSDictionary* urls_info =
      [shared_defaults objectForKey:app_group::kSuggestedItemsForMultiprofile];
  NSDictionary* last_modification_dates_info = [shared_defaults
      objectForKey:app_group::
                       kSuggestedItemsLastModificationDateForMultiprofile];

  // An account was removed, 'urls_info' and
  // 'last_modification_dates_info' need to be updated.
  // The +1 is needed because 'urls_info' contains also info about
  // most visited urls when there is no signed-in acoount.
  if (urls_info.count > accounts.count + 1) {
    NSMutableDictionary* updated_urls = [NSMutableDictionary dictionary];
    NSMutableDictionary* updated_dates = [NSMutableDictionary dictionary];

    for (NSString* gaia in urls_info) {
      if (accounts[gaia] || [gaia isEqualToString:app_group::kDefaultAccount]) {
        updated_urls[gaia] = urls_info[gaia];
        updated_dates[gaia] = last_modification_dates_info[gaia];
      }
    }
    [shared_defaults setObject:updated_urls
                        forKey:app_group::kSuggestedItemsForMultiprofile];
    [shared_defaults
        setObject:updated_dates
           forKey:app_group::
                      kSuggestedItemsLastModificationDateForMultiprofile];
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UpdateAvatars, avatars),
      base::BindOnce(&ReloadAllTimelines));
}

void SystemAccountUpdater::HandleMigrationIfNeeded() {
  // Perform migration only if the flag is enabled.
  if (!IsWidgetsForMultiprofileEnabled()) {
    return;
  }
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  if (!local_state) {
    // Skip if there is no local_state. This can happen only in tests.
    CHECK_IS_TEST();
    return;
  }

  bool migration_performed =
      local_state->GetBoolean(prefs::kMigrateWidgetsPrefs);
  // Don't migrate prefs again if migration was already performed.
  if (migration_performed) {
    return;
  }
  local_state->SetBoolean(prefs::kMigrateWidgetsPrefs, true);
  UpdateLoadedAccounts();
}
