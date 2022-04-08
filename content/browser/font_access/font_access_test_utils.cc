// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_test_utils.h"

namespace content {

TestFontAccessPermissionManager::TestFontAccessPermissionManager() = default;
TestFontAccessPermissionManager::~TestFontAccessPermissionManager() = default;

void TestFontAccessPermissionManager::RequestPermission(
    PermissionType permissions,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    PermissionCallback callback) {
  EXPECT_EQ(permissions, PermissionType::LOCAL_FONTS);
  EXPECT_TRUE(user_gesture);
  request_callback_.Run(std::move(callback));
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForFrame(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return permission_status_for_frame_;
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForCurrentDocument(
    PermissionType permission,
    RenderFrameHost* render_frame_host) {
  return permission_status_for_frame_;
}

}  // namespace content
