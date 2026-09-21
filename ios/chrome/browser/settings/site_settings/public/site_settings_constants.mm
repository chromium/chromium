// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"

#import "base/notreached.h"

ContentSettingsType ContentSettingsTypeFromSiteSettingsCategory(
    SiteSettingsCategory category) {
  switch (category) {
    case SiteSettingsCategory::kMicrophone:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case SiteSettingsCategory::kCamera:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case SiteSettingsCategory::kLocation:
      return ContentSettingsType::GEOLOCATION;
  }
  NOTREACHED();
}

SiteSettingsCategory SiteSettingsCategoryFromContentSettingsType(
    ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      return SiteSettingsCategory::kMicrophone;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return SiteSettingsCategory::kCamera;
    case ContentSettingsType::GEOLOCATION:
      return SiteSettingsCategory::kLocation;
    default:
      NOTREACHED();
  }
}
