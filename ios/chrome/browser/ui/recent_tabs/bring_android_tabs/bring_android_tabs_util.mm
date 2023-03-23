// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/bring_android_tabs/bring_android_tabs_util.h"

#import <string>

#import "base/containers/contains.h"
#import "base/files/file.h"
#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/field_trial_register.h"
#import "components/segmentation_platform/public/result.h"
#import "components/sync/base/sync_prefs.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/recent_tabs/bring_android_tabs/bring_android_tabs_metrics.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

bool prompt_shown_current_session = false;
bool prompt_interacted = false;

// Returns true if the user is segmented as an Android switcher, either by
// the segmentation platform or by using the forced device switcher flag.
bool UserIsAndroidSwitcher(ChromeBrowserState* browser_state) {
  bool device_switcher_forced =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel;
  if (device_switcher_forced) {
    return true;
  }

  segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetDispatcherForBrowserState(browser_state);

  segmentation_platform::ClassificationResult result =
      dispatcher->GetCachedClassificationResult();
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kSegmentationIncomplete);
    return false;
  }

  if (base::Contains(
          result.ordered_labels,
          segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel)) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kNotAndroidSwitcher);
    return false;
  }

  if (result.ordered_labels[0] ==
      segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kSyncDisabled);
  }

  if (result.ordered_labels[0] !=
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kNotAndroidSwitcher);
    return false;
  }
  return true;
}

// Returns true if the user is eligible for the Bring Android Tabs prompt.
bool UserEligibleForAndroidSwitcherPrompt(ChromeBrowserState* browser_state) {
  absl::optional<base::Time> first_run_time = GetFirstRunTime();
  bool first_run_over_7_days_ago =
      first_run_time.has_value() &&
      (base::Time::Now() - first_run_time.value()) > base::Days(7);
  if (!ShouldPresentFirstRunExperience() && first_run_over_7_days_ago) {
    return false;
  }

  if (GetBringYourOwnTabsPromptType() ==
      BringYourOwnTabsPromptType::kDisabled) {
    return false;
  }

  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }

  bool prompt_shown = browser_state->GetPrefs()->GetBoolean(
      prefs::kIosBringAndroidTabsPromptDisplayed);
  if (prompt_interacted || (prompt_shown && !prompt_shown_current_session)) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kPromptShownAndDismissed);
    return false;
  }

  return UserIsAndroidSwitcher(browser_state);
}

// Returns the set of DistantTabs for an android switcher.
synced_sessions::DistantTabVector AndroidSwitcherTabs(
    ChromeBrowserState* browser_state) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  bool tab_sync_disabled =
      !sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kTabs);
  if (tab_sync_disabled) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kTabSyncDisabled);
    return synced_sessions::DistantTabVector();
  }

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForBrowserState(browser_state);
  auto synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service);
  synced_sessions::DistantTabVector android_tabs;

  for (size_t s = 0; s < synced_sessions->GetSessionCount(); s++) {
    const synced_sessions::DistantSession* session =
        synced_sessions->GetSession(s);
    for (auto const& tab : session->tabs) {
      std::unique_ptr<synced_sessions::DistantTab> tab_copy =
          std::make_unique<synced_sessions::DistantTab>();
      tab_copy->session_tag = tab->session_tag;
      tab_copy->tab_id = tab->tab_id;
      tab_copy->title = tab->title;
      tab_copy->virtual_url = tab->virtual_url;
      android_tabs.push_back(std::move(tab_copy));
    }
  }

  return android_tabs;
}

}  // namespace

synced_sessions::DistantTabVector PromptTabsForAndroidSwitcher(
    ChromeBrowserState* browser_state) {
  if (!UserEligibleForAndroidSwitcherPrompt(browser_state)) {
    return synced_sessions::DistantTabVector();
  }
  synced_sessions::DistantTabVector tabs = AndroidSwitcherTabs(browser_state);
  if (tabs.empty()) {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kNoActiveTabs);
  } else {
    base::UmaHistogramEnumeration(
        bring_android_tabs::kPromptAttemptStatusHistogramName,
        bring_android_tabs::PromptAttemptStatus::kSuccess);
  }
  return tabs;
}

void OnBringAndroidTabsPromptDisplayed(PrefService* user_prefs) {
  user_prefs->SetBoolean(prefs::kIosBringAndroidTabsPromptDisplayed, true);
  prompt_shown_current_session = true;
}

void OnUserInteractWithPrompt() {
  prompt_interacted = true;
}
