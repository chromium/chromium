// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_test_utils.h"

#include "content/public/browser/permission_request_description.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

TestFontAccessPermissionManager::TestFontAccessPermissionManager() = default;
TestFontAccessPermissionManager::~TestFontAccessPermissionManager() = default;

void TestFontAccessPermissionManager::RequestPermissionsFromCurrentDocument(
    RenderFrameHost* render_frame_host,
    const PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  EXPECT_EQ(request_description.permissions[0],
            blink::PermissionType::LOCAL_FONTS);
  EXPECT_TRUE(request_description.user_gesture);
  request_callback_.Run(std::move(callback));
}

blink::mojom::PermissionStatus
TestFontAccessPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return permission_status_for_current_document_;
}

}  // namespace content
