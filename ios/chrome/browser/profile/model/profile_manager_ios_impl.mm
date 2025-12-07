// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import <stdint.h>

#import <utility>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/uuid.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"
#import "ios/chrome/browser/profile/model/profile_deleter_ios.h"
#import "ios/chrome/browser/profile/model/profile_ios_impl.h"
#import "ios/chrome/browser/profile_metrics/model/profile_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

namespace {

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty()) {
    running_size += iter.GetInfo().GetSize();
  }
  return running_size;
}

// Simple task to log the size of the profile at `path`.
void RecordProfileSizeTask(const base::FilePath& path) {
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.ExtensionSize", size_MB);
}

}  // namespace

// Stores information about a single Profile.
class ProfileManagerIOSImpl::ProfileInfo {
 public:
  explicit ProfileInfo(std::unique_ptr<ProfileIOS> profile)
      : profile_(std::move(profile)) {
    CHECK(profile_);
  }

  ProfileInfo(ProfileInfo&&) = default;
  ProfileInfo& operator=(ProfileInfo&&) = default;

  ~ProfileInfo() = default;

  ProfileIOS* profile() const { return profile_.get(); }

  bool is_loaded() const { return is_loaded_; }

  void SetIsLoaded();

  void AddCallback(ProfileLoadedCallback callback);

  std::vector<ProfileLoadedCallback> TakeCallbacks() {
    return std::exchange(callbacks_, {});
  }

  // Increment the keep alive counter and return its value.
  uint32_t IncrementKeepAliveCounter() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_LT(keep_alive_counter_, std::numeric_limits<uint32_t>::max());
    return ++keep_alive_counter_;
  }

  // Decrement the keep alive counter and return its value.
  uint32_t DecrementKeepAliveCounter() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_GT(keep_alive_counter_, 0u);
    return --keep_alive_counter_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<ProfileIOS> profile_;
  std::vector<ProfileLoadedCallback> callbacks_;
  uint32_t keep_alive_counter_ = 0;
  bool is_loaded_ = false;
};

void ProfileManagerIOSImpl::ProfileInfo::SetIsLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_loaded_);
  is_loaded_ = true;
}

void ProfileManagerIOSImpl::ProfileInfo::AddCallback(
    ProfileLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_loaded_);
  if (!callback.is_null()) {
    callbacks_.push_back(std::move(callback));
  }
}

ProfileManagerIOSImpl::ProfileManagerIOSImpl(PrefService* local_state,
                                             const base::FilePath& data_dir)
    : local_state_(local_state),
      profile_data_dir_(data_dir),
      profile_attributes_storage_(local_state) {
  CHECK(local_state_);
  CHECK(!profile_data_dir_.empty());

  profile_attributes_storage_.EnsurePersonalProfileExists();
}

ProfileManagerIOSImpl::~ProfileManagerIOSImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(will_be_destroyed_);
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }
}

void ProfileManagerIOSImpl::PrepareForDestruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  will_be_destroyed_ = true;

  // Drops the ScopedProfileKeepAliveIOS for all profiles that are still
  // loading before notifying the observers. Then check that there are
  // no profiles still kept alive.
  loading_profiles_map_.clear();

  for (auto& observer : observers_) {
    observer.OnProfileManagerWillBeDestroyed(this);
  }

  CHECK(profiles_map_.empty());
}

void ProfileManagerIOSImpl::AddObserver(ProfileManagerObserverIOS* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  // Notify the observer of any pre-existing Profiles.
  for (auto& [name, profile_info] : profiles_map_) {
    if (IsProfileMarkedForDeletion(name)) {
      continue;
    }

    ProfileIOS* profile = profile_info.profile();
    CHECK(profile);

    observer->OnProfileCreated(this, profile);
    if (profile_info.is_loaded()) {
      observer->OnProfileLoaded(this, profile);
    }
  }
}

void ProfileManagerIOSImpl::RemoveObserver(
    ProfileManagerObserverIOS* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

ProfileIOS* ProfileManagerIOSImpl::GetProfileWithName(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do not give access to profiles marked for deletion.
  if (IsProfileMarkedForDeletion(name)) {
    return nullptr;
  }
  // If the profile is already loaded, just return it.
  auto iter = profiles_map_.find(name);
  if (iter != profiles_map_.end()) {
    ProfileInfo& profile_info = iter->second;
    if (profile_info.is_loaded()) {
      CHECK(profile_info.profile());
      return profile_info.profile();
    }
  }

  return nullptr;
}

std::vector<ProfileIOS*> ProfileManagerIOSImpl::GetLoadedProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ProfileIOS*> loaded_profiles;
  for (const auto& [name, profile_info] : profiles_map_) {
    if (IsProfileMarkedForDeletion(name)) {
      continue;
    }

    if (profile_info.is_loaded()) {
      CHECK(profile_info.profile());
      loaded_profiles.push_back(profile_info.profile());
    }
  }
  return loaded_profiles;
}

bool ProfileManagerIOSImpl::HasProfileWithName(std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.HasProfileWithName(name);
}

bool ProfileManagerIOSImpl::CanCreateProfileWithName(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.CanCreateProfileWithName(name);
}

std::string ProfileManagerIOSImpl::ReserveNewProfileName() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.ReserveNewProfileName();
}

bool ProfileManagerIOSImpl::CanDeleteProfileWithName(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.CanDeleteProfileWithName(name);
}

bool ProfileManagerIOSImpl::LoadProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateOrLoadProfile(name, LoadOrCreatePolicy::kLoadOnly,
                             std::move(initialized_callback),
                             std::move(created_callback));
}

bool ProfileManagerIOSImpl::CreateProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateOrLoadProfile(name, LoadOrCreatePolicy::kCreateIfNecessary,
                             std::move(initialized_callback),
                             std::move(created_callback));
}

void ProfileManagerIOSImpl::MarkProfileForDeletion(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(CanDeleteProfileWithName(name));

  // Remove the profile from the ProfileAttributesStorageIOS to prevent
  // people iterating over all profiles from seeing it anymore.
  profile_attributes_storage_.MarkProfileForDeletion(name);

  // If the profile is not loaded, nor loading, return.
  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    return;
  }

  // If the profile is still loading, pretend that the loading failed
  // by calling the ProfileLoadedCallbacks with nullptr.
  ProfileInfo& info = iter->second;
  if (!info.is_loaded()) {
    for (auto& callback : info.TakeCallbacks()) {
      std::move(callback).Run(CreateScopedProfileKeepAlive(nullptr));
    }
  } else {
    ProfileIOS* profile = info.profile();
    for (auto& observer : observers_) {
      observer.OnProfileMarkedForPermanentDeletion(this, profile);
    }
  }
}

bool ProfileManagerIOSImpl::IsProfileMarkedForDeletion(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.IsProfileMarkedForDeletion(name);
}

void ProfileManagerIOSImpl::PurgeProfilesMarkedForDeletion(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the list of profiles marked for deletion, and ignore those that
  // are already loaded or loading (their data will be deleted when they
  // are unloaded).
  std::set<std::string> profiles =
      profile_attributes_storage_.GetProfilesMarkedForDeletion();
  for (const auto& [key, _] : profiles_map_) {
    profiles.erase(key);
  }

  // If there are no profiles to delete, the operation is complete. Post
  // the callback on the current sequence to ensure it is always invoked
  // asynchronously.
  if (profiles.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  auto closure = base::BarrierClosure(profiles.size(), std::move(callback));
  for (const std::string& name : profiles) {
    profile_deleter_.DeleteProfile(
        name, profile_data_dir_,
        base::BindOnce(&ProfileManagerIOSImpl::OnProfileDeletionComplete,
                       weak_ptr_factory_.GetWeakPtr(), closure, name));
  }
}

ProfileAttributesStorageIOS*
ProfileManagerIOSImpl::GetProfileAttributesStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &profile_attributes_storage_;
}

base::FilePath ProfileManagerIOSImpl::GetProfilePath(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_attributes_storage_.HasProfileWithName(name));
  return profile_data_dir_.Append(name);
}

void ProfileManagerIOSImpl::OnProfileCreationStarted(
    ProfileIOS* profile,
    CreationMode creation_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(creation_mode, CreationMode::kAsynchronous);
  CHECK(profile);

  for (auto& observer : observers_) {
    observer.OnProfileCreated(this, profile);
  }
}

void ProfileManagerIOSImpl::OnProfileCreationFinished(
    ProfileIOS* profile,
    CreationMode creation_mode,
    bool is_new_profile,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(creation_mode, CreationMode::kAsynchronous);
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  const std::string& name = profile->GetProfileName();
  auto iter = profiles_map_.find(name);
  CHECK(iter != profiles_map_.end());
  ProfileInfo& profile_info = iter->second;
  auto callbacks = profile_info.TakeCallbacks();

  // Update the ProfileAttributesStorageIOS before notifying the observers
  // and callbacks of the success or failure of the operation.
  if (is_new_profile) {
    if (success) {
      profile_attributes_storage_.UpdateAttributesForProfileWithName(
          name, base::BindOnce([](ProfileAttributesIOS& attrs) {
            attrs.ClearIsNewProfile();
          }));
    } else {
      MarkProfileForDeletion(name);
    }
  }

  // If the profile is marked for deletion, consider the load as failed
  // even in case of success.
  if (success && !IsProfileMarkedForDeletion(name)) {
    DoFinalInit(profile);
    iter->second.SetIsLoaded();
  } else {
    profile = nullptr;
  }

  // Invoke the callbacks, if the load failed, `profile` will be null.
  for (auto& callback : callbacks) {
    std::move(callback).Run(CreateScopedProfileKeepAlive(&profile_info));
  }

  // Notify the observers after invoking the callbacks in case of success.
  if (success) {
    CHECK(profile);
    for (auto& observer : observers_) {
      observer.OnProfileLoaded(this, profile);
    }
  }

  // The profile is fully loaded, so drop the ScopedProfileKeepAliveIOS
  // owned by this instance. If no other code keeps the profile alive,
  // it will be unloaded at this point.
  CHECK(base::Contains(loading_profiles_map_, name));
  loading_profiles_map_.erase(name);
}

bool ProfileManagerIOSImpl::CreateOrLoadProfile(
    std::string_view name,
    LoadOrCreatePolicy policy,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Profile creation is forbiden for profiles that have been marked for
  // deletion. Fail even if the profile is already loaded, to avoid new
  // usages after the deletion.
  if (will_be_destroyed_ || IsProfileMarkedForDeletion(name)) {
    if (!initialized_callback.is_null()) {
      std::move(initialized_callback)
          .Run(CreateScopedProfileKeepAlive(nullptr));
    }
    return false;
  }

  // Need to track whether the profile is already reserved, and/or loaded.
  bool inserted = false;
  bool existing = HasProfileWithName(name);

  // As the name may have been registered with ProfileAttributesStorageIOS,
  // a profile is considered as a new profile if the storage does not know
  // about it, or if the IsNewProfile() flag is still set. The flag will be
  // cleared the first time the profile is successfully loaded.
  bool is_new_profile = !existing;
  if (existing) {
    const ProfileAttributesIOS attrs =
        profile_attributes_storage_.GetAttributesForProfileWithName(name);
    is_new_profile = attrs.IsNewProfile();
  }

  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    if (is_new_profile) {
      // The profile has never be loaded and needs to be created. Check
      // whether creating the profile is allowed. This is controlled by
      // `policy` and `CanCreateProfileWithName(...)`.
      const bool creation_allowed =
          (policy == LoadOrCreatePolicy::kCreateIfNecessary) &&
          (existing || CanCreateProfileWithName(name));

      if (!creation_allowed) {
        if (!initialized_callback.is_null()) {
          std::move(initialized_callback)
              .Run(CreateScopedProfileKeepAlive(nullptr));
        }
        return false;
      }
    }

    if (!existing) {
      profile_attributes_storage_.AddProfile(name);
      CHECK(HasProfileWithName(name));
    }

    std::tie(iter, inserted) = profiles_map_.emplace(
        name, ProfileIOS::CreateProfile(profile_data_dir_.Append(name), name,
                                        CreationMode::kAsynchronous, this));

    CHECK(inserted);
  }

  CHECK(iter != profiles_map_.end());
  ProfileInfo& profile_info = iter->second;
  CHECK(profile_info.profile());

  // Ensure the profile is kept alive until it is fully loaded or
  // the current instance is destroyed.
  if (inserted) {
    CHECK(!base::Contains(loading_profiles_map_, name));
    loading_profiles_map_.emplace(name,
                                  CreateScopedProfileKeepAlive(&profile_info));
  }

  if (!created_callback.is_null()) {
    std::move(created_callback)
        .Run(CreateScopedProfileKeepAlive(&profile_info));
  }

  if (!initialized_callback.is_null()) {
    if (inserted || !profile_info.is_loaded()) {
      profile_info.AddCallback(std::move(initialized_callback));
    } else {
      std::move(initialized_callback)
          .Run(CreateScopedProfileKeepAlive(&profile_info));
    }
  }

  return true;
}

void ProfileManagerIOSImpl::DoFinalInit(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoFinalInitForServices(profile);

  // Log the profile size after a reasonable startup delay.
  CHECK(!profile->IsOffTheRecord());
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RecordProfileSizeTask, profile->GetStatePath()),
      base::Seconds(112));

  LogNumberOfProfiles(this);
}

void ProfileManagerIOSImpl::DoFinalInitForServices(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IdentityManagerFactory::GetForProfile(profile)->OnNetworkInitialized();

  // Those services needs to be explicitly initialized and can't simply be
  // marked as created with the profile as 1. they depend on initialisation
  // performed in ProfileIOSImpl (thus can't work with TestProfileIOS), and
  // 2. code do not expect them to be null (thus tests cannot be configured
  // to have a null instance).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  ListFamilyMembersServiceFactory::GetForProfile(profile)->Init();
}

void ProfileManagerIOSImpl::OnProfileDeletionComplete(
    base::OnceClosure closure,
    const std::string& profile_name,
    ProfileDeleterIOS::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == ProfileDeleterIOS::Result::kSuccess) {
    profile_attributes_storage_.ProfileDeletionComplete(profile_name);
  }

  std::move(closure).Run();
}

ScopedProfileKeepAliveIOS ProfileManagerIOSImpl::CreateScopedProfileKeepAlive(
    ProfileInfo* info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  //  If the info is null or the current instance is being destroyed, then
  // the ScopedProfileKeepAlive is a no-op.
  if (!info || will_be_destroyed_) {
    return ScopedProfileKeepAliveIOS(CreatePassKey(), nullptr, {});
  }

  ProfileIOS* profile = info->profile();
  CHECK(info->profile());

  info->IncrementKeepAliveCounter();
  return ScopedProfileKeepAliveIOS(
      CreatePassKey(), profile,
      base::BindOnce(&ProfileManagerIOSImpl::MaybeUnloadProfile,
                     weak_ptr_factory_.GetWeakPtr(),
                     profile->GetProfileName()));
}

void ProfileManagerIOSImpl::MaybeUnloadProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = profiles_map_.find(name);
  CHECK(iter != profiles_map_.end());

  // If there are other ScopedProfileKeepAliveIOS referencing this profile,
  // then return as there is nothing more to do.
  if (iter->second.DecrementKeepAliveCounter() > 0u) {
    return;
  }

  // Extract the profile from the storage before notifying the observers.
  auto node = profiles_map_.extract(iter);
  ProfileInfo& info = node.mapped();

  // If the profile is still loading, pretend that the loading failed
  // by calling the ProfileLoadedCallbacks with nullptr.
  if (!info.is_loaded()) {
    for (auto& callback : info.TakeCallbacks()) {
      std::move(callback).Run(CreateScopedProfileKeepAlive(nullptr));
    }
  } else {
    // If profile is loaded, notify all observers that it is unloaded.
    ProfileIOS* profile = info.profile();
    for (auto& observer : observers_) {
      observer.OnProfileUnloaded(this, profile);
    }
  }

  // If the profile has been marked for deletion, then try to delete it
  // after notifying all observers that it has been unloaded.
  if (IsProfileMarkedForDeletion(node.key())) {
    profile_deleter_.DeleteProfile(
        node.key(), profile_data_dir_,
        base::BindOnce(&ProfileManagerIOSImpl::OnProfileDeletionComplete,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing(),
                       node.key()));
  }
}
