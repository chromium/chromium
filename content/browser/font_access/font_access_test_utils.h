// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_

#include "content/public/test/mock_permission_manager.h"

namespace blink {
enum class PermissionType;
}

namespace content {
struct PermissionRequestDescription;

class TestFontAccessPermissionManager : public MockPermissionManager {
 public:
  TestFontAccessPermissionManager();

  TestFontAccessPermissionManager(const TestFontAccessPermissionManager&) =
      delete;
  TestFontAccessPermissionManager& operator=(
      const TestFontAccessPermissionManager&) = delete;

  ~TestFontAccessPermissionManager() override;

  using PermissionCallback = base::OnceCallback<void(
      const std::vector<blink::mojom::PermissionStatus>&)>;

  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;

  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override;

  void SetRequestCallback(
      base::RepeatingCallback<void(PermissionCallback)> request_callback) {
    request_callback_ = std::move(request_callback);
  }

  void SetPermissionStatusForCurrentDocument(
      blink::mojom::PermissionStatus status) {
    permission_status_for_current_document_ = status;
  }

 private:
  base::RepeatingCallback<void(PermissionCallback)> request_callback_;
  blink::mojom::PermissionStatus permission_status_for_current_document_ =
      blink::mojom::PermissionStatus::ASK;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_TEST_UTILS_H_
