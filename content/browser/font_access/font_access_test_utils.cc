// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_test_utils.h"

#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

TestFontAccessPermissionManager::TestFontAccessPermissionManager() = default;
TestFontAccessPermissionManager::~TestFontAccessPermissionManager() = default;

void TestFontAccessPermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  EXPECT_EQ(permissions[0], blink::PermissionType::LOCAL_FONTS);
  EXPECT_TRUE(user_gesture);
  request_callback_.Run(std::move(callback));
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host) {
  return permission_status_for_current_document_;
}

}  // namespace content
