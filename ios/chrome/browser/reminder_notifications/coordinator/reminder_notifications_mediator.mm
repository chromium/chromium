// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mediator.h"

#import "base/check.h"
#import "base/json/values_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "url/gurl.h"

const char kReminderNotificationsTimeKey[] = "reminder_time";
const char kReminderNotificationsCreationTimeKey[] = "creation_time";

@implementation ReminderNotificationsMediator {
  // The `PrefService` used to store reminder data.
  raw_ptr<PrefService, DanglingUntriaged> _profilePrefs;
}

- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefs {
  CHECK(profilePrefs);

  if ((self = [super init])) {
    _profilePrefs = profilePrefs;
  }

  return self;
}

- (void)disconnect {
  _profilePrefs = nullptr;
}

#pragma mark - ReminderNotificationsMutator

- (void)setReminderForURL:(const GURL&)URL time:(base::Time)time {
  CHECK(_profilePrefs);

  const std::string URLString = URL.spec();

  if (URLString.empty()) {
    // `GURL::spec()` returns an empty string if the `GURL` is empty or invalid.
    // An empty string cannot be used as a reliable key for storing the reminder
    // in Prefs.

    // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
    // case.
    return;
  }

  ScopedDictPrefUpdate update(_profilePrefs, prefs::kReminderNotifications);

  // Create the dictionary value for this specific reminder.
  base::Value::Dict reminderDetails;

  reminderDetails.Set(kReminderNotificationsTimeKey, base::TimeToValue(time));
  reminderDetails.Set(kReminderNotificationsCreationTimeKey,
                      base::TimeToValue(base::Time::Now()));

  update->Set(URLString, std::move(reminderDetails));
}

@end
