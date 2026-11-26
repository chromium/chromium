// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions.h"

#import <optional>

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/web/public/permissions/permissions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using PermissionsTest = PlatformTest;

// Tests that `ContentPermissionToContentSettingsType` maps all permissions.
TEST_F(PermissionsTest, ContentPermissionToContentSettingsType) {
  EXPECT_EQ(ContentPermissionToContentSettingsType(ContentPermission::kCamera),
            ContentSettingsType::MEDIASTREAM_CAMERA);
  EXPECT_EQ(
      ContentPermissionToContentSettingsType(ContentPermission::kMicrophone),
      ContentSettingsType::MEDIASTREAM_MIC);
  EXPECT_EQ(
      ContentPermissionToContentSettingsType(ContentPermission::kGeolocation),
      ContentSettingsType::GEOLOCATION);
}

// Tests that `WebPermissionToContentPermission` correctly converts
// `web::Permission` values into corresponding `ContentPermission`.
TEST_F(PermissionsTest, WebPermissionToContentPermission) {
  EXPECT_EQ(WebPermissionToContentPermission(web::PermissionCamera),
            ContentPermission::kCamera);
  EXPECT_EQ(WebPermissionToContentPermission(web::PermissionMicrophone),
            ContentPermission::kMicrophone);
}

// Tests that `ContentPermissionToWebPermission` correctly converts
// `ContentPermission` values into corresponding `web::Permission` or nullopt.
TEST_F(PermissionsTest, ContentPermissionToWebPermission) {
  EXPECT_EQ(ContentPermissionToWebPermission(ContentPermission::kCamera),
            web::PermissionCamera);
  EXPECT_EQ(ContentPermissionToWebPermission(ContentPermission::kMicrophone),
            web::PermissionMicrophone);
  EXPECT_EQ(ContentPermissionToWebPermission(ContentPermission::kGeolocation),
            std::nullopt);
}
