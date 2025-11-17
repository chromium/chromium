// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/json/values_util.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_presenter.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"

using base::TimeToValue;
using base::ValueToTime;

namespace {

// Time after which days will be pruned from the set of active days.
const base::TimeDelta kStorageExpiry = base::Days(28);

}  // namespace

// static
void CrossPlatformPromosService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kCrossPlatformPromosActiveDays);
  registry->RegisterTimePref(prefs::kCrossPlatformPromosIOS16thActiveDay,
                             base::Time());
}

CrossPlatformPromosService::CrossPlatformPromosService(ProfileIOS* profile)
    : profile_(profile) {
  // TODO(crbug.com/460783437): Observe synced pref to see if promo should be
  // shown.
}

CrossPlatformPromosService::~CrossPlatformPromosService() = default;

void CrossPlatformPromosService::OnApplicationWillEnterForeground() {
  Update16thActiveDay();
  MaybeShowPromo();
}

void CrossPlatformPromosService::ShowLensPromo(Browser* browser) {
  TipsNotificationPresenter::Present(browser->AsWeakPtr(),
                                     TipsNotificationType::kLens);
}

void CrossPlatformPromosService::ShowESBPromo(Browser* browser) {
  TipsNotificationPresenter::Present(
      browser->AsWeakPtr(), TipsNotificationType::kEnhancedSafeBrowsing);
}

void CrossPlatformPromosService::ShowCPEPromo(Browser* browser) {
  TipsNotificationPresenter::Present(browser->AsWeakPtr(),
                                     TipsNotificationType::kCPE);
}

void CrossPlatformPromosService::MaybeShowPromo() {
  Browser* browser = GetActiveBrowser();
  if (!browser) {
    return;
  }
  // TODO(crbug.com/460783437): Check synced pref to see if promo should be
  // shown.
}

Browser* CrossPlatformPromosService::GetActiveBrowser() {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  if (browser_list) {
    std::set<Browser*> browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
    if (!browsers.empty()) {
      return *browsers.begin();
    }
  }
  return nullptr;
}

void CrossPlatformPromosService::Update16thActiveDay() {
  if (RecordActiveDay()) {
    base::Time day = FindActiveDay(16);
    profile_->GetPrefs()->SetTime(prefs::kCrossPlatformPromosIOS16thActiveDay,
                                  day);
  }
}

bool CrossPlatformPromosService::RecordActiveDay(base::Time day) {
  day = day.LocalMidnight();
  ScopedListPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCrossPlatformPromosActiveDays);
  base::Value::List& active_days = update.Get();

  // Return early if the given day is the most recent day in the list.
  int size = active_days.size();
  if (size > 0 && ValueToTime(active_days[size - 1]) == day) {
    return false;
  }

  // Prune active days older than 28 days and check for duplicates.
  base::Time cutoff = base::Time::Now().LocalMidnight() - kStorageExpiry;
  active_days.EraseIf([&](const base::Value& value) {
    std::optional<base::Time> stored_time = ValueToTime(value);
    if (!stored_time) {
      return true;  // Prune invalid entries.
    }
    return stored_time < cutoff || stored_time == day;
  });

  // Add the new day.
  active_days.Append(TimeToValue(day));
  return true;
}

base::Time CrossPlatformPromosService::FindActiveDay(size_t count) {
  if (count == 0) {
    return base::Time();
  }

  const base::Value::List& active_days =
      profile_->GetPrefs()->GetList(prefs::kCrossPlatformPromosActiveDays);

  if (active_days.size() < count) {
    return base::Time();
  }

  const base::Value& value = active_days[active_days.size() - count];
  return ValueToTime(value).value_or(base::Time());
}
