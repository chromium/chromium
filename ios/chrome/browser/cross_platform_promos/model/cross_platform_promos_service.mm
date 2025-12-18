// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/json/values_util.h"
#import "base/time/time.h"
#import "components/desktop_to_mobile_promos/features.h"
#import "components/desktop_to_mobile_promos/pref_names.h"
#import "components/desktop_to_mobile_promos/promos_types.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
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
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kCrossPlatformPromosActiveDays);
  registry->RegisterTimePref(prefs::kCrossPlatformPromosIOS16thActiveDay,
                             base::Time());
  registry->RegisterDictionaryPref(
      prefs::kIOSPromoReminder,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

CrossPlatformPromosService::CrossPlatformPromosService(ProfileIOS* profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIOSPromoReminder,
      base::BindRepeating(&CrossPlatformPromosService::MaybeShowPromo,
                          weak_factory_.GetWeakPtr()));
}

CrossPlatformPromosService::~CrossPlatformPromosService() = default;
void CrossPlatformPromosService::OnApplicationWillEnterForeground() {
  if (IsMobilePromoOnDesktopRecordActiveDaysEnabled()) {
    Update16thActiveDay();
  }
  if (MobilePromoOnDesktopEnabled()) {
    MaybeShowPromo();
  }
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

  const base::Value::Dict& promo_reminder =
      profile_->GetPrefs()->GetDict(prefs::kIOSPromoReminder);
  std::optional<int> promo_type =
      promo_reminder.FindInt(prefs::kIOSPromoReminderPromoType);

  if (!promo_type) {
    return;
  }

  // Do not show promo if the target device guid does not match the current
  // device guid.
  const std::string* promo_device_guid =
      promo_reminder.FindString(prefs::kIOSPromoReminderDeviceGUID);
  if (!promo_device_guid) {
    return;
  }

  const syncer::DeviceInfo* local_device_info =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_)
          ->GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  if (!local_device_info || local_device_info->guid() != *promo_device_guid) {
    return;
  }

  switch (static_cast<desktop_to_mobile_promos::PromoType>(*promo_type)) {
    case desktop_to_mobile_promos::PromoType::kLens:
      ShowLensPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kEnhancedBrowsing:
      ShowESBPromo(browser);
      break;
    case desktop_to_mobile_promos::PromoType::kPassword:
      ShowCPEPromo(browser);
      break;
    default:
      // If the promo type is unknown, clear the pref to avoid re-triggering.
      profile_->GetPrefs()->ClearPref(prefs::kIOSPromoReminder);
      return;
  }

  // Clear the promo reminder pref after showing the promo.
  profile_->GetPrefs()->ClearPref(prefs::kIOSPromoReminder);
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
