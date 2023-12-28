// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_CAPTURED_SURFACE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_CAPTURED_SURFACE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "content/browser/renderer_host/media/captured_surface_controller.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

class MockCapturedSurfaceController final : public CapturedSurfaceController {
 public:
  MockCapturedSurfaceController(GlobalRenderFrameHostId capturer_rfh_id,
                                WebContentsMediaCaptureId captured_wc_id)
      : CapturedSurfaceController(capturer_rfh_id, captured_wc_id) {}

  ~MockCapturedSurfaceController() override = default;

  void SetSendWheelResponse(
      blink::mojom::CapturedSurfaceControlResult send_wheel_result);

  void SendWheel(
      blink::mojom::CapturedWheelActionPtr action,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          reply_callback) override;

 private:
  absl::optional<blink::mojom::CapturedSurfaceControlResult> result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_CAPTURED_SURFACE_CONTROLLER_H_
