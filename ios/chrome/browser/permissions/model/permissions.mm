// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions.h"

#import <optional>

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/web/public/permissions/permissions.h"

ContentSettingsType ContentPermissionToContentSettingsType(
    ContentPermission permission) {
  switch (permission) {
    case ContentPermission::kCamera:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case ContentPermission::kMicrophone:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case ContentPermission::kGeolocation:
      return ContentSettingsType::GEOLOCATION;
  }
}

ContentPermission WebPermissionToContentPermission(
    web::Permission web_permission) {
  switch (web_permission) {
    case web::PermissionCamera:
      return ContentPermission::kCamera;
    case web::PermissionMicrophone:
      return ContentPermission::kMicrophone;
  }
}

std::optional<web::Permission> ContentPermissionToWebPermission(
    ContentPermission permission) {
  switch (permission) {
    case ContentPermission::kCamera:
      return web::PermissionCamera;
    case ContentPermission::kMicrophone:
      return web::PermissionMicrophone;
    case ContentPermission::kGeolocation:
      return std::nullopt;
  }
}
