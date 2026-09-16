// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions.h"

std::optional<web::Permission> WebPermissionFromContentPermission(
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
