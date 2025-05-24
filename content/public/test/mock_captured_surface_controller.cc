// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_captured_surface_controller.h"

#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

using CapturedSurfaceControlResult =
    ::blink::mojom::CapturedSurfaceControlResult;

MockCapturedSurfaceController::MockCapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id)
    : CapturedSurfaceController(capturer_rfh_id,
                                captured_wc_id,
                                base::DoNothing()) {}

MockCapturedSurfaceController::~MockCapturedSurfaceController() = default;

void MockCapturedSurfaceController::SetSendWheelResponse(
    CapturedSurfaceControlResult send_wheel_result) {
  send_wheel_result_ = send_wheel_result;
}

void MockCapturedSurfaceController::SendWheel(
    blink::mojom::CapturedWheelActionPtr action,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  std::optional<CapturedSurfaceControlResult> send_wheel_result;
  std::swap(send_wheel_result_, send_wheel_result);

  CHECK(send_wheel_result);
  std::move(reply_callback).Run(*send_wheel_result);
}

void MockCapturedSurfaceController::SetUpdateZoomLevelResponse(
    CapturedSurfaceControlResult update_zoom_level_result) {
  update_zoom_level_result_ = update_zoom_level_result;
}

void MockCapturedSurfaceController::UpdateZoomLevel(
    blink::mojom::ZoomLevelAction action,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  CHECK(update_zoom_level_result_);
  const CapturedSurfaceControlResult update_zoom_level_result =
      *update_zoom_level_result_;
  update_zoom_level_result_ = std::nullopt;
  std::move(reply_callback).Run(update_zoom_level_result);
}

void MockCapturedSurfaceController::SetRequestPermissionResponse(
    CapturedSurfaceControlResult request_permission_result) {
  request_permission_result_ = request_permission_result;
}

void MockCapturedSurfaceController::RequestPermission(
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  std::optional<CapturedSurfaceControlResult> request_permission_result;
  std::swap(request_permission_result_, request_permission_result);

  CHECK(request_permission_result);
  std::move(reply_callback).Run(*request_permission_result);
}

}  // namespace content
