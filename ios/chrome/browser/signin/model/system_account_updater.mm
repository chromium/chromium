// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_account_updater.h"

#import "base/barrier_callback.h"
#import "base/check_deref.h"
#import "base/task/bind_post_task.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
#import "base/check_is_test.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/widget_kit/model/model_swift.h"  // nogncheck
#endif

namespace {

// Updates all widget timelines with the updated data.
#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
void ReloadAllTimelines() {
  [WidgetTimelinesUpdater reloadAllTimelines];
}
#endif

// Stores information about a SystemIdentity.
class SystemIdentityInfo {
 public:
  SystemIdentityInfo(id<SystemIdentity> identity, UIImage* cached_avatar)
      : gaia_id_(identity.gaiaId),
        full_name_(identity.userFullName ?: @""),
        user_email_(identity.userEmail),
        cached_avatar_(cached_avatar) {}

  ~SystemIdentityInfo() = default;

  const GaiaId& gaia_id() const { return gaia_id_; }
  NSString* full_name() const { return full_name_; }
  NSString* user_email() const { return user_email_; }
  UIImage* cached_avatar() const { return cached_avatar_; }

  NSDictionary* AsDictionary() const {
    NSMutableDictionary* dictionary = [[NSMutableDictionary alloc] init];
    [dictionary setObject:user_email_ forKey:app_group::kEmail];
    [dictionary setObject:full_name_ forKey:app_group::kFullName];
    return dictionary;
  }

 private:
  GaiaId gaia_id_;
  NSString* full_name_;
  NSString* user_email_;
  UIImage* cached_avatar_;
};

// Stores information about a SystemIdentity in a serializable format.
class SystemIdentityInfoData {
 public:
  explicit SystemIdentityInfoData(const SystemIdentityInfo& info)
      : gaia_id_(info.gaia_id()), avatar_data_(nil) {
    UIImage* image = info.cached_avatar();
    if (image) {
      const CGSize size = CGSizeMake(32.0, 32.0);
      if (!CGSizeEqualToSize(image.size, size)) {
        image = ResizeImage(image, size, ProjectionMode::kAspectFit);
      }
      avatar_data_ = UIImagePNGRepresentation(image);
    }
  }

  ~SystemIdentityInfoData() = default;

  const GaiaId& gaia_id() const { return gaia_id_; }
  NSData* avatar_data() const { return avatar_data_; }
  NSURL* avatar_path(NSURL* directory) const {
    return [directory
        URLByAppendingPathComponent:[gaia_id_.ToNSString()
                                        stringByAppendingPathExtension:@"png"]];
  }

 private:
  GaiaId gaia_id_;
  NSData* avatar_data_;
};

// Converts a SystemIdentityInfo into SystemIdentityInfoData.
SystemIdentityInfoData ConvertSystemIdentityInfo(SystemIdentityInfo info) {
  return SystemIdentityInfoData(info);
}

// A list of SystemIdentityInfo.
using SystemIdentityInfoList = std::vector<SystemIdentityInfo>;
using SystemIdentityInfoDataList = std::vector<SystemIdentityInfoData>;

// Used as iterator for SystemIdentityManager.
class SystemIdentityInfoCollector {
 public:
  using IteratorResult = SystemIdentityManager::IteratorResult;

  SystemIdentityInfoCollector(SystemIdentityInfoList& list,
                              SystemIdentityManager& manager)
      : list_(list), manager_(manager) {}

  ~SystemIdentityInfoCollector() = default;

  IteratorResult AddIdentity(id<SystemIdentity> identity) {
    list_->push_back(SystemIdentityInfo(
        identity, manager_->GetCachedAvatarForIdentity(identity)));
    return IteratorResult::kContinueIteration;
  }

 private:
  raw_ref<SystemIdentityInfoList> list_;
  raw_ref<SystemIdentityManager> manager_;
};

// Collects the information about the SystemIdentity available on device.
SystemIdentityInfoList CollectSystemIdentityInfo(
    SystemIdentityManager& manager) {
  SystemIdentityInfoList result;
  SystemIdentityInfoCollector iterator(result, manager);
  // Using base::Unretained(...) is safe since IterateOverIdentities(...)
  // will synchronously call the callback during its execution and will
  // not let it escape.
  manager.IterateOverIdentities(base::BindRepeating(
      &SystemIdentityInfoCollector::AddIdentity, base::Unretained(&iterator)));
  return result;
}

// Writes the avatars' image to disk, deleting obsolete avatars.
void WriteAvatars(const SystemIdentityInfoDataList& list) {
  base::ScopedBlockingCall may_block(FROM_HERE, base::BlockingType::MAY_BLOCK);

  NSURL* avatar_folder = app_group::WidgetsAvatarFolder();
  if (!avatar_folder) {
    return;
  }

  NSFileManager* manager = [NSFileManager defaultManager];
  NSMutableSet<NSURL*>* avatars = [[NSMutableSet alloc] init];
  for (const SystemIdentityInfoData& info : list) {
    NSData* data = info.avatar_data();
    if (data) {
      NSURL* path = info.avatar_path(avatar_folder);
      if ([data writeToURL:path atomically:YES]) {
        [avatars addObject:path];
      }
    }
  }

  const auto options = NSDirectoryEnumerationSkipsSubdirectoryDescendants;
  for (NSURL* url in [manager contentsOfDirectoryAtURL:avatar_folder
                            includingPropertiesForKeys:nil
                                               options:options
                                                 error:nil]) {
    if (![avatars containsObject:url]) {
      [manager removeItemAtURL:url error:nil];
    }
  }
}

// Writes the avatar's image to disk, or delete it if the data is missing.
void WriteAvatar(const SystemIdentityInfoData& info) {
  base::ScopedBlockingCall may_block(FROM_HERE, base::BlockingType::MAY_BLOCK);

  NSURL* avatar_folder = app_group::WidgetsAvatarFolder();
  if (!avatar_folder) {
    return;
  }

  NSURL* path = info.avatar_path(avatar_folder);
  if (NSData* data = info.avatar_data()) {
    [data writeToURL:path atomically:YES];
  } else {
    NSFileManager* manager = [NSFileManager defaultManager];
    [manager removeItemAtURL:path error:nil];
  }
}

// Removes items for unknown accounts from `defaults`.
void RemoveUnknownAccounts(NSUserDefaults* defaults,
                           NSDictionary* accounts,
                           NSString* key) {
  NSDictionary* value = [defaults objectForKey:key];
  if (!value) {
    return;
  }

  NSMutableDictionary* copy = [value mutableCopy];
  for (NSString* gaia_id_string in value) {
    if ([accounts objectForKey:gaia_id_string] ||
        [gaia_id_string isEqualToString:app_group::kDefault] ||
        [gaia_id_string isEqualToString:app_group::kNoAccount]) {
      continue;
    }

    [copy removeObjectForKey:gaia_id_string];
  }
  [defaults setObject:copy forKey:key];
}

}  // namespace

SystemAccountUpdater::SystemAccountUpdater(
    SystemIdentityManager* system_identity_manager)
    : system_identity_manager_(CHECK_DEREF(system_identity_manager)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  system_identity_manager_observation_.Observe(&*system_identity_manager_);
  HandleMigrationIfNeeded();
}

SystemAccountUpdater::~SystemAccountUpdater() = default;

void SystemAccountUpdater::OnIdentityListChanged() {
  UpdateLoadedAccounts();
}

void SystemAccountUpdater::OnIdentityUpdated(id<SystemIdentity> identity) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WriteAvatar,
          ConvertSystemIdentityInfo(SystemIdentityInfo(
              identity, system_identity_manager_->GetCachedAvatarForIdentity(
                            identity)))));
}

#pragma mark - Private

void SystemAccountUpdater::UpdateLoadedAccounts() {
  SystemIdentityInfoList list =
      CollectSystemIdentityInfo(*system_identity_manager_);

  auto callback = base::BarrierCallback<SystemIdentityInfoData>(
      list.size(),
      base::BindPostTask(task_runner_, base::BindOnce(&WriteAvatars)));

  NSMutableDictionary<NSString*, NSDictionary*>* accounts =
      [[NSMutableDictionary alloc] init];
  for (auto& info : list) {
    [accounts setObject:info.AsDictionary() forKey:info.gaia_id().ToNSString()];
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ConvertSystemIdentityInfo, std::move(info))
                       .Then(callback));
  }

  NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
  [defaults setObject:accounts forKey:app_group::kAccountsOnDevice];

  RemoveUnknownAccounts(defaults, accounts,
                        app_group::kSuggestedItemsForMultiprofile);
  RemoveUnknownAccounts(
      defaults, accounts,
      app_group::kSuggestedItemsLastModificationDateForMultiprofile);
}

void SystemAccountUpdater::HandleMigrationIfNeeded() {
  // Perform migration only if the flag is enabled.
#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  if (!local_state) {
    // Skip if there is no local_state. This can happen only in tests.
    CHECK_IS_TEST();
    return;
  }

  bool migration_performed =
      local_state->GetBoolean(prefs::kMigrateWidgetsPrefs);

  if (!migration_performed) {
    // Only migrate prefs if a migration was never performed.
    local_state->SetBoolean(prefs::kMigrateWidgetsPrefs, true);
    UpdateLoadedAccounts();
  } else if (!local_state->GetBoolean(prefs::kWidgetsForMultiProfile) &&
             AreSeparateProfilesForManagedAccountsEnabled()) {
    // Reload timelines if multi-profile was enabled since last build.
    local_state->SetBoolean(prefs::kWidgetsForMultiProfile, true);
    ReloadAllTimelines();
  } else if (local_state->GetBoolean(prefs::kWidgetsForMultiProfile) &&
             !AreSeparateProfilesForManagedAccountsEnabled()) {
    // Reload timelines if multi-profile was disabled since last build.
    local_state->SetBoolean(prefs::kWidgetsForMultiProfile, false);
    ReloadAllTimelines();
  }
#endif
}
