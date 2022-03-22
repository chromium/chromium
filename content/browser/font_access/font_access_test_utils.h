// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_

#include "content/public/test/mock_permission_manager.h"

namespace content {

class TestFontAccessPermissionManager : public MockPermissionManager {
 public:
  TestFontAccessPermissionManager();

  TestFontAccessPermissionManager(const TestFontAccessPermissionManager&) =
      delete;
  TestFontAccessPermissionManager& operator=(
      const TestFontAccessPermissionManager&) = delete;

  ~TestFontAccessPermissionManager() override;

  using PermissionCallback =
      base::OnceCallback<void(blink::mojom::PermissionStatus)>;

  void RequestPermission(PermissionType permissions,
                         RenderFrameHost* render_frame_host,
                         const GURL& requesting_origin,
                         bool user_gesture,
                         PermissionCallback callback) override;

  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;

  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) override;

  void SetRequestCallback(
      base::RepeatingCallback<void(PermissionCallback)> request_callback) {
    request_callback_ = std::move(request_callback);
  }

  void SetPermissionStatusForFrame(blink::mojom::PermissionStatus status) {
    permission_status_for_frame_ = status;
  }

 private:
  base::RepeatingCallback<void(PermissionCallback)> request_callback_;
  blink::mojom::PermissionStatus permission_status_for_frame_ =
      blink::mojom::PermissionStatus::ASK;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_
