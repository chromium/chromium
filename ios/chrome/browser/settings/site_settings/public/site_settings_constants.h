// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings_types.h"

// Category types supported by Site Settings.
enum class SiteSettingsCategory {
  kMicrophone,
  kCamera,
  kLocation,
};

// Converts a SiteSettingsCategory to the corresponding ContentSettingsType.
ContentSettingsType ContentSettingsTypeFromSiteSettingsCategory(
    SiteSettingsCategory category);

// Converts a ContentSettingsType to the corresponding SiteSettingsCategory.
SiteSettingsCategory SiteSettingsCategoryFromContentSettingsType(
    ContentSettingsType type);

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

// Accessibility identifier for the category detail table view.
inline constexpr NSString* const kSiteSettingsCategoryDetailTableViewId =
    @"kSiteSettingsCategoryDetailTableViewId";

// Accessibility identifier for the category detail search bar.
inline constexpr NSString* const kSiteSettingsCategoryDetailSearchBarId =
    @"kSiteSettingsCategoryDetailSearchBarId";

// Accessibility identifier for the category detail Ask option cell.
inline constexpr NSString* const kSiteSettingsCategoryDetailAskCellId =
    @"kSiteSettingsCategoryDetailAskCellId";

// Accessibility identifier for the category detail Block option cell.
inline constexpr NSString* const kSiteSettingsCategoryDetailBlockCellId =
    @"kSiteSettingsCategoryDetailBlockCellId";

// Accessibility identifier for the category detail search scrim view.
inline constexpr NSString* const kSiteSettingsCategoryDetailScrimViewId =
    @"kSiteSettingsCategoryDetailScrimViewId";

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_PUBLIC_SITE_SETTINGS_CONSTANTS_H_
