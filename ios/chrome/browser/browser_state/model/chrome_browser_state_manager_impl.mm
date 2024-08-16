// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"

#import <stdint.h>

#import <utility>

#import "base/check.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/model/chrome_browser_state_impl.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/browser_state_metrics/model/browser_state_metrics.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"
#import "ios/chrome/browser/push_notification/model/push_notification_browser_state_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/unified_consent/model/unified_consent_service_factory.h"

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

// Simple task to log the size of the browser state at `path`.
void BrowserStateSizeTask(const base::FilePath& path) {
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);
}

}  // namespace

// Stores information about a single BrowserState.
class ChromeBrowserStateManagerImpl::BrowserStateInfo {
 public:
  explicit BrowserStateInfo(std::unique_ptr<ChromeBrowserState> browser_state)
      : browser_state_(std::move(browser_state)) {
    DCHECK(browser_state_);
  }

  BrowserStateInfo(BrowserStateInfo&&) = default;
  BrowserStateInfo& operator=(BrowserStateInfo&&) = default;

  ~BrowserStateInfo() = default;

  ChromeBrowserState* browser_state() const { return browser_state_.get(); }

  bool is_loaded() const { return is_loaded_; }

  void SetIsLoaded();

  void AddCallback(ChromeBrowserStateLoadedCallback callback);

  std::vector<ChromeBrowserStateLoadedCallback> TakeCallbacks() {
    return std::exchange(callbacks_, {});
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::vector<ChromeBrowserStateLoadedCallback> callbacks_;
  bool is_loaded_ = false;
};

void ChromeBrowserStateManagerImpl::BrowserStateInfo::SetIsLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
  is_loaded_ = true;
}

void ChromeBrowserStateManagerImpl::BrowserStateInfo::AddCallback(
    ChromeBrowserStateLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
  if (!callback.is_null()) {
    callbacks_.push_back(std::move(callback));
  }
}

ChromeBrowserStateManagerImpl::ChromeBrowserStateManagerImpl(
    PrefService* local_state,
    const base::FilePath& data_dir)
    : local_state_(local_state), data_dir_(data_dir) {
  CHECK(local_state_);
  CHECK(!data_dir_.empty());
}

ChromeBrowserStateManagerImpl::~ChromeBrowserStateManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateManagerDestroyed(this);
  }
}

void ChromeBrowserStateManagerImpl::AddObserver(
    ChromeBrowserStateManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  // Notify the observer of any pre-existing ChromeBrowserStates.
  for (auto& [name, info] : browser_states_) {
    ChromeBrowserState* browser_state = info.browser_state();
    DCHECK(browser_state);

    observer->OnChromeBrowserStateCreated(this, browser_state);
    if (info.is_loaded()) {
      observer->OnChromeBrowserStateLoaded(this, browser_state);
    }
  }
}

void ChromeBrowserStateManagerImpl::RemoveObserver(
    ChromeBrowserStateManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ChromeBrowserStateManagerImpl::LoadBrowserStates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::List& last_active_browser_states =
      local_state_->GetList(prefs::kBrowserStatesLastActive);

  std::set<std::string> last_active_browser_states_set;
  for (const base::Value& browser_state_id : last_active_browser_states) {
    if (browser_state_id.is_string()) {
      last_active_browser_states_set.insert(browser_state_id.GetString());
    }
  }

  // Ensure that the last used BrowserState is loaded (since client code
  // does not expect GetLastUsedBrowserStateDeprecatedDoNotUse() to return
  // null).
  //
  // See https://crbug.com/345478758 for exemple of crashes happening when
  // the last used BrowserState is not loaded.
  last_active_browser_states_set.insert(GetLastUsedBrowserStateName());

  // Create and load test profiles if experiment enabling Switch Profile
  // developer UI is enabled.
  std::optional<int> load_test_profiles =
      experimental_flags::DisplaySwitchProfile();
  if (load_test_profiles.has_value()) {
    for (int i = 0; i < load_test_profiles; i++) {
      last_active_browser_states_set.insert("TestProfile" +
                                            base::NumberToString(i + 1));
    }
  }

  for (const std::string& browser_state_name : last_active_browser_states_set) {
    ChromeBrowserState* browser_state = CreateBrowserState(browser_state_name);
    DCHECK(browser_state != nullptr);
  }
}

ChromeBrowserState*
ChromeBrowserStateManagerImpl::GetLastUsedBrowserStateDeprecatedDoNotUse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeBrowserState* browser_state =
      GetBrowserStateByName(GetLastUsedBrowserStateName());
  CHECK(browser_state);
  return browser_state;
}

ChromeBrowserState* ChromeBrowserStateManagerImpl::GetBrowserStateByName(
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the browser state is already loaded, just return it.
  auto iter = browser_states_.find(name);
  if (iter != browser_states_.end()) {
    BrowserStateInfo& info = iter->second;
    if (info.is_loaded()) {
      DCHECK(info.browser_state());
      return info.browser_state();
    }
  }

  return nullptr;
}

std::vector<ChromeBrowserState*>
ChromeBrowserStateManagerImpl::GetLoadedBrowserStates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ChromeBrowserState*> loaded_browser_states;
  for (const auto& pair : browser_states_) {
    const BrowserStateInfo& info = pair.second;
    if (info.is_loaded()) {
      DCHECK(info.browser_state());
      loaded_browser_states.push_back(info.browser_state());
    }
  }
  return loaded_browser_states;
}

bool ChromeBrowserStateManagerImpl::LoadBrowserStateAsync(
    std::string_view name,
    ChromeBrowserStateLoadedCallback initialized_callback,
    ChromeBrowserStateLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!BrowserStateWithNameExists(name)) {
    // Must not create the ChromeBrowserState if it does not already
    // exist, so fail.
    if (!initialized_callback.is_null()) {
      std::move(initialized_callback).Run(nullptr);
    }
    return false;
  }

  return CreateBrowserStateAsync(name, std::move(initialized_callback),
                                 std::move(created_callback));
}

bool ChromeBrowserStateManagerImpl::CreateBrowserStateAsync(
    std::string_view name,
    ChromeBrowserStateLoadedCallback initialized_callback,
    ChromeBrowserStateLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateBrowserStateWithMode(name, CreationMode::kAsynchronous,
                                    std::move(initialized_callback),
                                    std::move(created_callback));
}

ChromeBrowserState* ChromeBrowserStateManagerImpl::LoadBrowserState(
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!BrowserStateWithNameExists(name)) {
    // Must not create the ChromeBrowserState if it does not already
    // exist, so fail.
    return nullptr;
  }

  return CreateBrowserState(name);
}

ChromeBrowserState* ChromeBrowserStateManagerImpl::CreateBrowserState(
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CreateBrowserStateWithMode(name, CreationMode::kSynchronous,
                                  /* initialized_callback */ {},
                                  /* created_callback */ {})) {
    return nullptr;
  }

  auto iter = browser_states_.find(name);
  DCHECK(iter != browser_states_.end());

  DCHECK(iter->second.is_loaded());
  return iter->second.browser_state();
}

BrowserStateInfoCache*
ChromeBrowserStateManagerImpl::GetBrowserStateInfoCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!browser_state_info_cache_) {
    browser_state_info_cache_ =
        std::make_unique<BrowserStateInfoCache>(local_state_.get());
  }
  return browser_state_info_cache_.get();
}

void ChromeBrowserStateManagerImpl::OnChromeBrowserStateCreationStarted(
    ChromeBrowserState* browser_state,
    CreationMode creation_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_state);

  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateCreated(this, browser_state);
  }
}

void ChromeBrowserStateManagerImpl::OnChromeBrowserStateCreationFinished(
    ChromeBrowserState* browser_state,
    CreationMode creation_mode,
    bool is_new_browser_state,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());

  // If the BrowserState is loaded synchronously the method is called as
  // part of the constructor and before the BrowserStateInfo insertion
  // in the map. The method will be called again after the insertion.
  auto iter = browser_states_.find(browser_state->GetBrowserStateName());
  if (iter == browser_states_.end()) {
    DCHECK(creation_mode == CreationMode::kSynchronous);
    return;
  }

  DCHECK(iter != browser_states_.end());
  auto callbacks = iter->second.TakeCallbacks();

  if (success) {
    DoFinalInit(browser_state);
    iter->second.SetIsLoaded();
  } else {
    browser_state = nullptr;
    browser_states_.erase(iter);
  }

  // Invoke the callbacks, if the load failed, `browser_state` will be null.
  for (auto& callback : callbacks) {
    std::move(callback).Run(browser_state);
  }

  // Notify the observers after invoking the callbacks.
  for (auto& observer : observers_) {
    observer.OnChromeBrowserStateLoaded(this, browser_state);
  }
}

std::string ChromeBrowserStateManagerImpl::GetLastUsedBrowserStateName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string last_used_browser_state_name =
      local_state_->GetString(prefs::kBrowserStateLastUsed);
  if (last_used_browser_state_name.empty()) {
    last_used_browser_state_name = kIOSChromeInitialBrowserState;
  }
  CHECK(!last_used_browser_state_name.empty());
  return last_used_browser_state_name;
}

bool ChromeBrowserStateManagerImpl::BrowserStateWithNameExists(
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetBrowserStateInfoCache()->GetIndexOfBrowserStateWithName(name) !=
         std::string::npos;
}

bool ChromeBrowserStateManagerImpl::CanCreateBrowserStateWithName(
    std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/335630301): check whether there is a ChromeBrowserState
  // with that name whose deletion is pending, and return false if this is
  // the case (to avoid recovering its state).
  return true;
}

bool ChromeBrowserStateManagerImpl::CreateBrowserStateWithMode(
    std::string_view name,
    CreationMode creation_mode,
    ChromeBrowserStateLoadedCallback initialized_callback,
    ChromeBrowserStateLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = false;
  bool existing = BrowserStateWithNameExists(name);

  auto iter = browser_states_.find(name);
  if (iter == browser_states_.end()) {
    if (!CanCreateBrowserStateWithName(name)) {
      if (!initialized_callback.is_null()) {
        std::move(initialized_callback).Run(nullptr);
      }
      return false;
    }

    std::tie(iter, inserted) = browser_states_.insert(std::make_pair(
        std::string(name),
        BrowserStateInfo(ChromeBrowserState::CreateBrowserState(
            data_dir_.Append(name), name, creation_mode, this))));

    DCHECK(inserted);
  }

  DCHECK(iter != browser_states_.end());
  BrowserStateInfo& info = iter->second;
  DCHECK(info.browser_state());

  if (!created_callback.is_null()) {
    std::move(created_callback).Run(info.browser_state());
  }

  if (!initialized_callback.is_null()) {
    if (inserted || !info.is_loaded()) {
      info.AddCallback(std::move(initialized_callback));
    } else {
      std::move(initialized_callback).Run(info.browser_state());
    }
  }

  // If asked to load synchronously but an asynchronous load was already in
  // progress, pretend the load failed, as we cannot return an unitialized
  // ChromeBrowserState, nor can we wait for the asynchronous initialisation
  // to complete.
  if (creation_mode == CreationMode::kSynchronous) {
    if (!inserted && !info.is_loaded()) {
      return false;
    }
  }

  // If the ChromeBrowserState was just created, and the creation mode is
  // synchronous then OnChromeBrowserStateCreationFinished(...) will have
  // been called during the construction of the BrowserStateInfo. Thus it
  // is necessary to call the method again here.
  if (inserted && creation_mode == CreationMode::kSynchronous) {
    OnChromeBrowserStateCreationFinished(info.browser_state(),
                                         CreationMode::kAsynchronous, !existing,
                                         /* success */ true);
  }

  return true;
}

void ChromeBrowserStateManagerImpl::AddBrowserStateToCache(
    ChromeBrowserState* browser_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_state->IsOffTheRecord());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  const CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  BrowserStateInfoCache* cache = GetBrowserStateInfoCache();
  const size_t browser_state_index = cache->GetIndexOfBrowserStateWithName(
      browser_state->GetBrowserStateName());
  if (browser_state_index != std::string::npos) {
    // The BrowserStateInfoCache's info must match the IdentityManager.
    cache->SetAuthInfoOfBrowserStateAtIndex(
        browser_state_index, account_info.gaia, account_info.email);
    return;
  }
  cache->AddBrowserState(browser_state->GetBrowserStateName(),
                         account_info.gaia, account_info.email);
}

void ChromeBrowserStateManagerImpl::DoFinalInit(
    ChromeBrowserState* browser_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoFinalInitForServices(browser_state);
  AddBrowserStateToCache(browser_state);

  // Log the browser state size after a reasonable startup delay.
  DCHECK(!browser_state->IsOffTheRecord());
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BrowserStateSizeTask, browser_state->GetStatePath()),
      base::Seconds(112));

  LogNumberOfBrowserStates(this);
}

void ChromeBrowserStateManagerImpl::DoFinalInitForServices(
    ChromeBrowserState* browser_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ios::AccountConsistencyServiceFactory::GetForBrowserState(browser_state);
  IdentityManagerFactory::GetForBrowserState(browser_state)
      ->OnNetworkInitialized();
  ios::AccountReconcilorFactory::GetForBrowserState(browser_state);
  // Initialization needs to happen after the browser context is available
  // because UnifiedConsentService's dependencies needs the URL context getter.
  UnifiedConsentServiceFactory::GetForBrowserState(browser_state);

  // Initialization needs to happen after the browser context is available
  // because because IOSChromeMetricsServiceAccessor requires browser_state
  // to be registered in the ChromeBrowserStateManager.
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    OptimizationGuideServiceFactory::GetForBrowserState(browser_state)
        ->DoFinalInit(BackgroundDownloadServiceFactory::GetForBrowserState(
            browser_state));
  }
  segmentation_platform::SegmentationPlatformServiceFactory::GetForBrowserState(
      browser_state);

  PushNotificationBrowserStateServiceFactory::GetForBrowserState(browser_state);

  ChildAccountServiceFactory::GetForBrowserState(browser_state)->Init();
  SupervisedUserServiceFactory::GetForBrowserState(browser_state)->Init();
  ListFamilyMembersServiceFactory::GetForBrowserState(browser_state)->Init();

  // The AboutThisSiteService needs to be created at startup in order to
  // register its OptimizationType with OptimizationGuideDecider.
  AboutThisSiteServiceFactory::GetForBrowserState(browser_state);

  PlusAddressServiceFactory::GetForBrowserState(browser_state);
}

