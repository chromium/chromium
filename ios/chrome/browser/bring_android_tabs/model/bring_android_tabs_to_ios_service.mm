// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"

#import <numeric>
#import <string>

#import "base/containers/contains.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "components/sync/service/sync_prefs.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/bring_android_tabs/model/metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ui/base/device_form_factor.h"

namespace {

using ::bring_android_tabs::kPromptAttemptStatusHistogramName;
using ::bring_android_tabs::PromptAttemptStatus;

// The length of time from now in which the tabs will be brought over.
const base::TimeDelta kTimeRangeOfTabsImported = base::Days(14);

// Maximum number of tabs that should be imported.
const size_t kMaxNumberOfTabs = 20;

// Logs `status` on UMA.
void RecordPromptAttemptStatus(PromptAttemptStatus status) {
  base::UmaHistogramEnumeration(kPromptAttemptStatusHistogramName, status);
}

// Returns true if the user is eligible for the Bring Android Tabs prompt. Logs
// attempt status metric on UMA if the user is NOT eligible.
bool UserEligibleForAndroidSwitcherPrompt() {
  if (IsFirstRunRecent(base::Days(7))) {
    return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  }
  return false;
}

// Returns true if the user is segmented as an Android switcher, either by
// the segmentation platform or by using the forced device switcher flag.
bool UserIsAndroidSwitcher(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  bool device_switcher_forced =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel;
  if (device_switcher_forced) {
    return true;
  }

  segmentation_platform::ClassificationResult result =
      dispatcher->GetCachedClassificationResult();
  return result.status == segmentation_platform::PredictionStatus::kSucceeded &&
         result.ordered_labels[0] ==
             segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel &&
         !base::Contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
}

}  // namespace

BringAndroidTabsToIOSService::BringAndroidTabsToIOSService(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
    syncer::SyncService* sync_service,
    sync_sessions::SessionSyncService* session_sync_service,
    PrefService* profile_prefs)
    : device_switcher_result_dispatcher_(dispatcher),
      sync_service_(sync_service),
      session_sync_service_(session_sync_service),
      profile_prefs_(profile_prefs) {
  DCHECK(device_switcher_result_dispatcher_);
  DCHECK(sync_service_);
  DCHECK(session_sync_service_);
  DCHECK(profile_prefs_);
}

BringAndroidTabsToIOSService::~BringAndroidTabsToIOSService() {}

void BringAndroidTabsToIOSService::LoadTabs() {
  load_tabs_invoked_ = true;
  // Early returns for users who should NOT be enrolled in the feature
  // experiment. This includes current iPad users and those who aren't recent
  // Android switchers.
  if (!UserEligibleForAndroidSwitcherPrompt() ||
      !UserIsAndroidSwitcher(device_switcher_result_dispatcher_)) {
    return;
  }

  // In case the user is previously eligible for the prompt but not
  // anymore, clear the tabs so that future calls to `GetNumberOfAndroidTabs()`
  // will return 0 and the caller won't show the prompt.
  if (PromptShownAndShouldNotShowAgain()) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kPromptShownAndDismissed);
    synced_sessions_.reset();
    position_of_tabs_in_synced_sessions_.clear();
    return;
  }

  // Load the tabs if they aren't loaded.
  if (position_of_tabs_in_synced_sessions_.empty()) {
    PromptAttemptStatus status = LoadSyncedSessionsAndComputeTabPositions();
    RecordPromptAttemptStatus(status);
  }
}

size_t BringAndroidTabsToIOSService::GetNumberOfAndroidTabs() const {
  CHECK(load_tabs_invoked_);
  size_t tab_count = position_of_tabs_in_synced_sessions_.size();
  CHECK_LE(tab_count, kMaxNumberOfTabs);
  return tab_count;
}

synced_sessions::DistantTab* BringAndroidTabsToIOSService::GetTabAtIndex(
    size_t index) const {
  CHECK_LT(index, position_of_tabs_in_synced_sessions_.size());
  std::tuple<size_t, size_t> indices =
      position_of_tabs_in_synced_sessions_[index];
  size_t session_idx = std::get<0>(indices);
  size_t tab_idx = std::get<1>(indices);
  return synced_sessions_->GetSession(session_idx)->tabs[tab_idx].get();
}

void BringAndroidTabsToIOSService::OpenTabsAtIndices(
    const std::vector<size_t>& indices,
    UrlLoadingBrowserAgent* url_loader) {
  const int tab_count = static_cast<int>(indices.size());
  const bool in_incognito = false;
  const int maximum_instant_load_tabs =
      GetDefaultNumberOfTabsToLoadSimultaneously();
  for (int i = 0; i < tab_count; i++) {
    const bool instant_load = i < maximum_instant_load_tabs;
    OpenDistantTab(GetTabAtIndex(indices[i]), in_incognito, instant_load,
                   url_loader, UrlLoadStrategy::NORMAL);
  }
}

void BringAndroidTabsToIOSService::OpenAllTabs(
    UrlLoadingBrowserAgent* url_loader) {
  std::vector<size_t> indices(GetNumberOfAndroidTabs());
  std::iota(std::begin(indices), std::end(indices), 0);
  OpenTabsAtIndices(indices, url_loader);
}

void BringAndroidTabsToIOSService::OnBringAndroidTabsPromptDisplayed() {
  profile_prefs_->SetBoolean(prefs::kIosBringAndroidTabsPromptDisplayed, true);
  prompt_shown_current_session_ = true;
}

void BringAndroidTabsToIOSService::OnUserInteractWithBringAndroidTabsPrompt() {
  prompt_interacted_ = true;
}

bool BringAndroidTabsToIOSService::PromptShownAndShouldNotShowAgain() const {
  bool shown_before =
      profile_prefs_->GetBoolean(prefs::kIosBringAndroidTabsPromptDisplayed);
  bool should_not_show_again =
      prompt_interacted_ || (shown_before && !prompt_shown_current_session_);
  return should_not_show_again;
}

PromptAttemptStatus
BringAndroidTabsToIOSService::LoadSyncedSessionsAndComputeTabPositions() {
  bool tab_sync_disabled =
      !sync_service_ ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs);
  if (tab_sync_disabled) {
    return PromptAttemptStatus::kTabSyncDisabled;
  }

  // Synced sessions sorted by recency.
  synced_sessions_ =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  size_t session_count = synced_sessions_->GetSessionCount();
  std::set<std::pair<std::u16string, std::u16string>> tab_titles_and_urls;

  for (size_t session_idx = 0; session_idx < session_count; session_idx++) {
    // Only tabs from an Android phone device within the last
    // `kTimeRangeOfTabsImported` are considered Android tabs.
    const synced_sessions::DistantSession* session =
        synced_sessions_->GetSession(session_idx);
    if (session->form_factor != syncer::DeviceInfo::FormFactor::kPhone ||
        session->modified_time < base::Time::Now() - kTimeRangeOfTabsImported) {
      continue;
    }
    size_t tab_size = session->tabs.size();
    // Tabs are already ordered by recency.
    for (size_t tab_idx = 0; tab_idx < tab_size; tab_idx++) {
      std::tuple<size_t, size_t> indices = {session_idx, tab_idx};

      // Skip tabs with the same title and URL.
      const synced_sessions::DistantTab* tab_candidate =
          synced_sessions_->GetSession(session_idx)->tabs[tab_idx].get();
      std::pair<std::u16string, std::u16string> tab_candidate_key = {
          tab_candidate->title,
          url_formatter::
              FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                  tab_candidate->virtual_url)};
      if (!tab_titles_and_urls.contains(tab_candidate_key)) {
        position_of_tabs_in_synced_sessions_.push_back(indices);
        tab_titles_and_urls.insert(tab_candidate_key);
      }

      if (position_of_tabs_in_synced_sessions_.size() >= kMaxNumberOfTabs) {
        break;
      }
    }
    if (position_of_tabs_in_synced_sessions_.size() >= kMaxNumberOfTabs) {
      break;
    }
  }
  return position_of_tabs_in_synced_sessions_.empty()
             ? PromptAttemptStatus::kNoActiveTabs
             : PromptAttemptStatus::kSuccess;
}
