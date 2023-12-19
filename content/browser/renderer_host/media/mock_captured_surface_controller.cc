// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/mock_captured_surface_controller.h"

#include "content/browser/renderer_host/media/captured_surface_control_permission_manager.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

void MockCapturedSurfaceController::SetSendWheelResponse(
    blink::mojom::CapturedSurfaceControlResult send_wheel_result) {
  result_ = send_wheel_result;
}

void MockCapturedSurfaceController::SendWheel(
    blink::mojom::CapturedWheelActionPtr action,
    base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
        reply_callback) {
  CHECK(result_);
  const blink::mojom::CapturedSurfaceControlResult send_wheel_result = *result_;
  result_ = absl::nullopt;
  std::move(reply_callback).Run(send_wheel_result);
}

}  // namespace content
