// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_HISTOGRAMS_H_

#import <Foundation/Foundation.h>

// Enum representing types of users' interactions with Passwords In Other Apps.
// This is used for metrics recording purposes.
typedef NS_ENUM(NSUInteger, PasswordsInOtherAppsActionType) {
  // Open Passwords in Other Apps from Chrome settings.
  PasswordsInOtherAppsActionOpen = 0,
  // Tapping button/link to go to iOS settings from Passwords In Other Apps.
  PasswordsInOtherAppsActionGoToIOSSetting,
  // Password auto fill status change.
  PasswordsInOtherAppsActionAutoFillStatusChange,
  // Dismissing Passwords In Other Apps.
  PasswordsInOtherAppsActionDismiss,
};

// Record the event on UMA with the the user's current password auto fill
// enrollment state.
void RecordEventOnUMA(PasswordsInOtherAppsActionType action);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_HISTOGRAMS_H_
