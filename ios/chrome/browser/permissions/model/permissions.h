// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/web/public/permissions/permissions.h"

// Enum for permission types requiring prompt dialogs in Chrome iOS.
enum class ContentPermission {
  kCamera,
  kMicrophone,
  kGeolocation,
};

// Returns the corresponding `web::Permission`, or `std::nullopt` if none
// exists.
std::optional<web::Permission> WebPermissionFromContentPermission(
    ContentPermission permission);

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_H_
