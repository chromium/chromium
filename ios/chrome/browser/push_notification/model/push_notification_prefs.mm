// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace push_notification_prefs {

const char kSendTabLastOpenTimestamp[] = "ios.sendtab.last_opened_timestamp";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
    registry->RegisterTimePref(kSendTabLastOpenTimestamp, base::Time());
}

}  // namespace push_notification_prefs
