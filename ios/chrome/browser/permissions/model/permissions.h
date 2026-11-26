// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_

#import <optional>

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/web/public/permissions/permissions.h"

// Enum for permission types requiring prompt dialogs in Chrome iOS.
enum class ContentPermission {
  kCamera,
  kMicrophone,
  kGeolocation,
};

// Maps `ContentPermission` to `ContentSettingsType`.
ContentSettingsType ContentPermissionToContentSettingsType(
    ContentPermission permission);

// Maps `web::Permission` to `ContentPermission`.
ContentPermission WebPermissionToContentPermission(
    web::Permission web_permission);

// Maps `ContentPermission` to `web::Permission`, or `std::nullopt` if none
// exists (e.g. Geolocation).
std::optional<web::Permission> ContentPermissionToWebPermission(
    ContentPermission permission);

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_
