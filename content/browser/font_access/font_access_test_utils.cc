// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_test_utils.h"

#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

TestFontAccessPermissionManager::TestFontAccessPermissionManager() = default;
TestFontAccessPermissionManager::~TestFontAccessPermissionManager() = default;

void TestFontAccessPermissionManager::RequestPermission(
    blink::PermissionType permissions,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    PermissionCallback callback) {
  EXPECT_EQ(permissions, blink::PermissionType::LOCAL_FONTS);
  EXPECT_TRUE(user_gesture);
  request_callback_.Run(std::move(callback));
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForFrame(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return permission_status_for_frame_;
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host) {
  return permission_status_for_frame_;
}

}  // namespace content
