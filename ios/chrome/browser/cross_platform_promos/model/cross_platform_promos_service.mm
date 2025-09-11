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
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

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

CrossPlatformPromosService::CrossPlatformPromosService(
    PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {}

CrossPlatformPromosService::~CrossPlatformPromosService() = default;

void CrossPlatformPromosService::OnApplicationWillEnterForeground() {
  Update16thActiveDay();
}

void CrossPlatformPromosService::Update16thActiveDay() {
  if (RecordActiveDay()) {
    base::Time day = FindActiveDay(16);
    profile_prefs_->SetTime(prefs::kCrossPlatformPromosIOS16thActiveDay, day);
  }
}

bool CrossPlatformPromosService::RecordActiveDay(base::Time day) {
  day = day.LocalMidnight();
  ScopedListPrefUpdate update(profile_prefs_,
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
      profile_prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);

  if (active_days.size() < count) {
    return base::Time();
  }

  const base::Value& value = active_days[active_days.size() - count];
  return ValueToTime(value).value_or(base::Time());
}
