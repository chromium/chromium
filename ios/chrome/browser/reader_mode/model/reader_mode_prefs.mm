// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_prefs.h"

#import "base/json/values_util.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace reader_mode_prefs {

const char kReaderModeRecentlyUsedTimestampsPref[] =
    "reader_mode.recently_used_timestamps";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kReaderModeRecentlyUsedTimestampsPref);
}

bool IsReaderModeRecentlyUsed(const PrefService& prefs) {
  const base::Time now = base::Time::Now();
  int use_in_num_days = 0;

  const base::TimeDelta num_days =
      base::Days(ReaderModeDefaultBrowserNumDaysCriteria());
  const base::Value::List& recently_used_timestamps =
      prefs.GetList(kReaderModeRecentlyUsedTimestampsPref);
  for (const base::Value& timestamp : recently_used_timestamps) {
    base::Time switch_time =
        base::ValueToTime(timestamp).value_or(base::Time());
    if (now - switch_time <= num_days) {
      ++use_in_num_days;
    }
  }
  return use_in_num_days >= ReaderModeDefaultBrowserActiveDaysCriteria();
}

}  // namespace reader_mode_prefs
