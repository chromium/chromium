// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MODE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MODE_H_

#import <Foundation/Foundation.h>

// Configures Google services settings UI according to the context when it is
// used.
typedef NS_ENUM(NSInteger, GoogleServicesSettingsMode) {
  // Show the Google services settings without being able to log-out.
  GoogleServicesSettingsModeAdvancedSigninSettings,
  // Show the regular Google services settings.
  GoogleServicesSettingsModeSettings,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_MODE_H_
