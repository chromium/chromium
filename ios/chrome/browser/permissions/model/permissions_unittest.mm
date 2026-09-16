// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions.h"

#import <optional>

#import "ios/web/public/permissions/permissions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using PermissionsTest = PlatformTest;

// Tests that `WebPermissionFromContentPermission` correctly converts
// `ContentPermission` values into corresponding `web::Permission` or nullopt.
TEST_F(PermissionsTest, WebPermissionFromContentPermission) {
  EXPECT_EQ(WebPermissionFromContentPermission(ContentPermission::kCamera),
            web::PermissionCamera);
  EXPECT_EQ(WebPermissionFromContentPermission(ContentPermission::kMicrophone),
            web::PermissionMicrophone);
  EXPECT_EQ(WebPermissionFromContentPermission(ContentPermission::kGeolocation),
            std::nullopt);
}
