// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import <stdint.h>

#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"
#import "ios/chrome/browser/profile/model/profile_ios_impl.h"
#import "ios/chrome/browser/profile_metrics/model/profile_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
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

// Returns whether `name` matches "TestProfile[0-9]+" regex which is the
// pattern used to name test profiles during an experiment run while the
// support for multi-profile was added.
bool IsTestProfile(std::string_view name) {
  constexpr std::string_view kTestProfilePrefix = "TestProfile";
  if (!name.starts_with(kTestProfilePrefix)) {
    return false;
  }

  std::string_view tail = name.substr(kTestProfilePrefix.size());
  if (tail.empty()) {
    return false;
  }

  for (const char c : tail) {
    if (c < '0' || '9' < c) {
      return false;
    }
  }

  return true;
}

// Returns the names of the recently active profiles.
std::set<std::string> GetRecentlyActiveProfiles(PrefService* local_state) {
  std::set<std::string> profiles;
  for (const auto& value : local_state->GetList(prefs::kLastActiveProfiles)) {
    if (value.is_string()) {
      const std::string& name = value.GetString();
      if (!name.empty() && !IsTestProfile(name)) {
        profiles.insert(name);
      }
    }
  }

  std::string last_used = local_state->GetString(prefs::kLastUsedProfile);
  if (!last_used.empty() && !IsTestProfile(last_used)) {
    profiles.insert(last_used);
  }

  return profiles;
}

}  // namespace

BASE_FEATURE(kHideLegacyProfiles,
             "HideLegacyProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Stores information about a single Profile.
class ProfileManagerIOSImpl::ProfileInfo {
 public:
  explicit ProfileInfo(std::unique_ptr<ProfileIOS> profile)
      : profile_(std::move(profile)) {
    DCHECK(profile_);
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

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<ProfileIOS> profile_;
  std::vector<ProfileLoadedCallback> callbacks_;
  bool is_loaded_ = false;
};

void ProfileManagerIOSImpl::ProfileInfo::SetIsLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
  is_loaded_ = true;
}

void ProfileManagerIOSImpl::ProfileInfo::AddCallback(
    ProfileLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
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
}

ProfileManagerIOSImpl::~ProfileManagerIOSImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }
}

void ProfileManagerIOSImpl::AddObserver(ProfileManagerObserverIOS* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  // Notify the observer of any pre-existing Profiles.
  for (auto& [name, profile_info] : profiles_map_) {
    ProfileIOS* profile = profile_info.profile();
    DCHECK(profile);

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

void ProfileManagerIOSImpl::LoadProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> profiles = GetRecentlyActiveProfiles(local_state_);

  // LoadProfiles() must load at least one profile, so if there is no
  // recently active Profile, create one with a default name.
  if (profiles.empty()) {
    profiles.insert(kIOSChromeInitialProfile);
  }

  // Take care of the legacy profiles.
  if (base::FeatureList::IsEnabled(kHideLegacyProfiles)) {
    HideLegacyProfiles(profiles);
  } else {
    RestoreLegacyProfiles(profiles);
  }

  // Record the number of legacy profiles.
  base::UmaHistogramCounts100(
      "Profile.LegacyProfilesCount",
      static_cast<int>(
          std::min(size_t{100},
                   local_state_->GetDict(prefs::kLegacyProfileMap).size())));

  for (const std::string& name : profiles) {
    ProfileIOS* profile = CreateProfile(name);
    DCHECK(profile != nullptr);
  }
}

ProfileIOS* ProfileManagerIOSImpl::GetProfileWithName(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the profile is already loaded, just return it.
  auto iter = profiles_map_.find(name);
  if (iter != profiles_map_.end()) {
    ProfileInfo& profile_info = iter->second;
    if (profile_info.is_loaded()) {
      DCHECK(profile_info.profile());
      return profile_info.profile();
    }
  }

  return nullptr;
}

std::vector<ProfileIOS*> ProfileManagerIOSImpl::GetLoadedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ProfileIOS*> loaded_profiles;
  for (const auto& [name, profile_info] : profiles_map_) {
    if (profile_info.is_loaded()) {
      DCHECK(profile_info.profile());
      loaded_profiles.push_back(profile_info.profile());
    }
  }
  return loaded_profiles;
}

bool ProfileManagerIOSImpl::LoadProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ProfileWithNameExists(name)) {
    // Must not create the ProfileIOS if it does not already exist, so fail.
    if (!initialized_callback.is_null()) {
      std::move(initialized_callback).Run(nullptr);
    }
    return false;
  }

  return CreateProfileAsync(name, std::move(initialized_callback),
                            std::move(created_callback));
}

bool ProfileManagerIOSImpl::CreateProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateProfileWithMode(name, CreationMode::kAsynchronous,
                               std::move(initialized_callback),
                               std::move(created_callback));
}

ProfileIOS* ProfileManagerIOSImpl::LoadProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ProfileWithNameExists(name)) {
    // Must not create the ProfileIOS if it does not already exist, so fail.
    return nullptr;
  }

  return CreateProfile(name);
}

ProfileIOS* ProfileManagerIOSImpl::CreateProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CreateProfileWithMode(name, CreationMode::kSynchronous,
                             /* initialized_callback */ {},
                             /* created_callback */ {})) {
    return nullptr;
  }

  auto iter = profiles_map_.find(name);
  DCHECK(iter != profiles_map_.end());

  DCHECK(iter->second.is_loaded());
  return iter->second.profile();
}

ProfileAttributesStorageIOS*
ProfileManagerIOSImpl::GetProfileAttributesStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &profile_attributes_storage_;
}

void ProfileManagerIOSImpl::OnProfileCreationStarted(
    ProfileIOS* profile,
    CreationMode creation_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile);

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
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());

  // If the Profile is loaded synchronously the method is called as part of the
  // constructor and before the ProfileInfo insertion in the map. The method
  // will be called again after the insertion.
  auto iter = profiles_map_.find(profile->GetProfileName());
  if (iter == profiles_map_.end()) {
    DCHECK(creation_mode == CreationMode::kSynchronous);
    return;
  }

  DCHECK(iter != profiles_map_.end());
  auto callbacks = iter->second.TakeCallbacks();

  if (success) {
    DoFinalInit(profile);
    iter->second.SetIsLoaded();
  } else {
    if (is_new_profile) {
      // TODO(crbug.com/335630301): Mark the data for removal and prevent the
      // creation of a profile with the same name until the data has been
      // deleted.
      const std::string& name = profile->GetProfileName();
      profile_attributes_storage_.RemoveProfile(name);
      DCHECK(!ProfileWithNameExists(name));
    }

    profile = nullptr;
    profiles_map_.erase(iter);
  }

  // Invoke the callbacks, if the load failed, `profile` will be null.
  for (auto& callback : callbacks) {
    std::move(callback).Run(profile);
  }

  // Notify the observers after invoking the callbacks in case of success.
  if (success) {
    DCHECK(profile);
    for (auto& observer : observers_) {
      observer.OnProfileLoaded(this, profile);
    }
  }
}

bool ProfileManagerIOSImpl::ProfileWithNameExists(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.HasProfileWithName(name);
}

bool ProfileManagerIOSImpl::CanCreateProfileWithName(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cannot create a profile with the same name as a legacy profile.
  if (local_state_->GetDict(prefs::kLegacyProfileMap).Find(name)) {
    return false;
  }

  // TODO(crbug.com/335630301): check whether there is a Profile with that name
  // whose deletion is pending, and return false if this is the case (to avoid
  // recovering its state).
  return true;
}

bool ProfileManagerIOSImpl::CreateProfileWithMode(
    std::string_view name,
    CreationMode creation_mode,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = false;
  bool existing = ProfileWithNameExists(name);

  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    if (!CanCreateProfileWithName(name)) {
      if (!initialized_callback.is_null()) {
        std::move(initialized_callback).Run(nullptr);
      }
      return false;
    }

    if (!existing) {
      profile_attributes_storage_.AddProfile(name);
      DCHECK(ProfileWithNameExists(name));
    }

    std::tie(iter, inserted) = profiles_map_.insert(std::make_pair(
        std::string(name),
        ProfileInfo(ProfileIOS::CreateProfile(profile_data_dir_.Append(name),
                                              name, creation_mode, this))));

    DCHECK(inserted);
  }

  DCHECK(iter != profiles_map_.end());
  ProfileInfo& profile_info = iter->second;
  DCHECK(profile_info.profile());

  if (!created_callback.is_null()) {
    std::move(created_callback).Run(profile_info.profile());
  }

  if (!initialized_callback.is_null()) {
    if (inserted || !profile_info.is_loaded()) {
      profile_info.AddCallback(std::move(initialized_callback));
    } else {
      std::move(initialized_callback).Run(profile_info.profile());
    }
  }

  // If asked to load synchronously but an asynchronous load was already in
  // progress, pretend the load failed, as we cannot return an unitialized
  // Profile, nor can we wait for the asynchronous initialisation to complete.
  if (creation_mode == CreationMode::kSynchronous) {
    if (!inserted && !profile_info.is_loaded()) {
      return false;
    }
  }

  // If the Profile was just created, and the creation mode is synchronous then
  // OnProfileCreationFinished() will have been called during the construction
  // of the ProfileInfo. Thus it is necessary to call the method again here.
  if (inserted && creation_mode == CreationMode::kSynchronous) {
    OnProfileCreationFinished(profile_info.profile(),
                              CreationMode::kAsynchronous, !existing,
                              /* success */ true);
  }

  return true;
}

void ProfileManagerIOSImpl::DoFinalInit(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoFinalInitForServices(profile);

  // Log the profile size after a reasonable startup delay.
  DCHECK(!profile->IsOffTheRecord());
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
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
  ListFamilyMembersServiceFactory::GetForProfile(profile)->Init();
}

void ProfileManagerIOSImpl::HideLegacyProfiles(
    const std::set<std::string>& profiles) {
  CHECK(base::FeatureList::IsEnabled(kHideLegacyProfiles));
  if (local_state_->GetBoolean(prefs::kLegacyProfileHidden)) {
    return;
  }

  base::Value::Dict legacy_profiles;

  const size_t count = profile_attributes_storage_.GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    const size_t index = count - i - 1;  // iterate backwards
    ProfileAttributesIOS attr =
        profile_attributes_storage_.GetAttributesForProfileAtIndex(index);

    const std::string name = attr.GetProfileName();
    if (!base::Contains(profiles, name)) {
      legacy_profiles.Set(name, std::move(attr).GetStorage());
      profile_attributes_storage_.RemoveProfile(name);
    }
  }

  local_state_->SetBoolean(prefs::kLegacyProfileHidden, true);
  local_state_->SetDict(prefs::kLegacyProfileMap, std::move(legacy_profiles));
}

void ProfileManagerIOSImpl::RestoreLegacyProfiles(
    const std::set<std::string>& profiles) {
  CHECK(!base::FeatureList::IsEnabled(kHideLegacyProfiles));
  if (!local_state_->GetBoolean(prefs::kLegacyProfileHidden)) {
    return;
  }

  const base::Value::Dict& legacy_profiles =
      local_state_->GetDict(prefs::kLegacyProfileMap);

  for (const auto [key, value] : legacy_profiles) {
    DCHECK(!base::Contains(profiles, key));
    DCHECK(value.is_dict());

    profile_attributes_storage_.AddProfile(key);
    profile_attributes_storage_.UpdateAttributesForProfileWithName(
        key, base::BindOnce(
                 [](const base::Value::Dict* dict, ProfileAttributesIOS attr) {
                   return ProfileAttributesIOS(attr.GetProfileName(), dict);
                 },
                 &value.GetDict()));
  }

  local_state_->ClearPref(prefs::kLegacyProfileHidden);
  local_state_->ClearPref(prefs::kLegacyProfileMap);
}
