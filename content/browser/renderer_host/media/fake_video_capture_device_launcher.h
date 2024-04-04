// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/video/video_capture_system.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"

namespace content {

class FakeVideoCaptureDeviceLauncher
    : public content::VideoCaptureDeviceLauncher {
 public:
  explicit FakeVideoCaptureDeviceLauncher(media::VideoCaptureSystem* system);
  ~FakeVideoCaptureDeviceLauncher() override;

  void LaunchDeviceAsync(
      const std::string& device_id,
      blink::mojom::MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      base::WeakPtr<media::VideoFrameReceiver> receiver,
      base::OnceClosure connection_lost_cb,
      Callbacks* callbacks,
      base::OnceClosure done_cb,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor) override;
  void AbortLaunch() override;

 private:
  raw_ptr<media::VideoCaptureSystem> system_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_FAKE_VIDEO_CAPTURE_DEVICE_LAUNCHER_H_
