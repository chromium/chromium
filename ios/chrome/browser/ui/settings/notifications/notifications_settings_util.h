// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_UTIL_H_

#import <Foundation/Foundation.h>
#import <string>

class PrefService;
enum class PushNotificationClientId;

namespace notifications_settings {

// Reflects the state a push notification client's notification permission set
// holds when aggregated across all modes of transmission (i.e mobile, email,
// etc). A client's permission state is considered enabled only if every
// transmission mode has permissions enabled and vice versa for a disabled
// permission state. Otherwise, a client's permission state is considered
// indeterminant.
enum class ClientPermissionState { ENABLED, DISABLED, INDETERMINANT };

// Returns whether push notification permissions are enabled/disabled across all
// chrome push notification clients.
ClientPermissionState GetNotificationPermissionState(const std::string& gaia_id,
                                                     PrefService* pref_service);

// Returns whether the push notification permission statuses aggregated across
// all modes of transmission (i.e mobile, email, etc) are enabled or disabled
// for a client.
ClientPermissionState GetClientPermissionState(
    PushNotificationClientId client_id,
    const std::string& gaia_id,
    PrefService* pref_service);

// Returns whether the push notification client's, `client_id`,
// permission status for mobile notifications is enabled or disabled for the
// current user.
BOOL GetMobileNotificationPermissionStatusForClient(
    PushNotificationClientId clientID,
    const std::string& gaia_id);

}  // namespace notifications_settings

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_UTIL_H_
