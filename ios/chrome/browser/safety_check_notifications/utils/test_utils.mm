// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/utils/test_utils.h"

#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/testing_application_context.h"

void UpdateSafetyCheckNotificationsPermission(BOOL enable) {
  PrefService* pref_service = GetApplicationContext()->GetLocalState();

  ScopedDictPrefUpdate update(pref_service,
                              prefs::kAppLevelPushNotificationPermissions);

  update->Set(kSafetyCheckNotificationKey, enable);
}
