// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the Site Settings cell in Settings.
inline constexpr NSString* const kSettingsSiteSettingsCellId =
    @"kSettingsSiteSettingsCellId";

// Accessibility identifier for the Site Settings table view.
inline constexpr NSString* const kSiteSettingsTableViewId =
    @"kSiteSettingsTableViewId";

// Accessibility identifier for the Microphone cell.
inline constexpr NSString* const kSiteSettingsMicrophoneCellId =
    @"kSiteSettingsMicrophoneCellId";

// Accessibility identifier for the Camera cell.
inline constexpr NSString* const kSiteSettingsCameraCellId =
    @"kSiteSettingsCameraCellId";

// Accessibility identifier for the Location cell.
inline constexpr NSString* const kSiteSettingsLocationCellId =
    @"kSiteSettingsLocationCellId";

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_
