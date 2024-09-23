// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_CAPTURED_SURFACE_CONTROLLER_H_
#define CONTENT_PUBLIC_TEST_MOCK_CAPTURED_SURFACE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "content/browser/media/captured_surface_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

class MockCapturedSurfaceController final : public CapturedSurfaceController {
 public:
  MockCapturedSurfaceController(GlobalRenderFrameHostId capturer_rfh_id,
                                WebContentsMediaCaptureId captured_wc_id);

  ~MockCapturedSurfaceController() override;

  void SetSendWheelResponse(
      blink::mojom::CapturedSurfaceControlResult send_wheel_result);

  // CapturedSurfaceController impl
  MOCK_METHOD(void,
              UpdateCaptureTarget,
              (WebContentsMediaCaptureId),
              (override));
  void SendWheel(
      blink::mojom::CapturedWheelActionPtr action,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          reply_callback) override;

  void SetGetZoomLevelResponse(
      std::optional<int> get_zoom_level_value,
      blink::mojom::CapturedSurfaceControlResult get_zoom_level_result);

  void SetSetZoomLevelResponse(
      blink::mojom::CapturedSurfaceControlResult set_zoom_level_result);

  void SetZoomLevel(
      int zoom_level,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          reply_callback) override;

  void SetRequestPermissionResponse(
      blink::mojom::CapturedSurfaceControlResult request_permission_result);

  void RequestPermission(
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          reply_callback) override;

 private:
  std::optional<blink::mojom::CapturedSurfaceControlResult> send_wheel_result_;
  std::optional<
      std::pair<std::optional<int>, blink::mojom::CapturedSurfaceControlResult>>
      get_zoom_level_result_;
  std::optional<blink::mojom::CapturedSurfaceControlResult>
      set_zoom_level_result_;
  std::optional<blink::mojom::CapturedSurfaceControlResult>
      request_permission_result_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_CAPTURED_SURFACE_CONTROLLER_H_
