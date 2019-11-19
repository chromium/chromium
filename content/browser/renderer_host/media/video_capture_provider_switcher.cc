// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"
#include "content/public/browser/video_capture_device_launcher.h"

#include <utility>

#include "base/bind.h"

namespace content {

namespace {

class VideoCaptureDeviceLauncherSwitcher : public VideoCaptureDeviceLauncher {
 public:
  VideoCaptureDeviceLauncherSwitcher(
      std::unique_ptr<VideoCaptureDeviceLauncher> media_device_launcher,
      std::unique_ptr<VideoCaptureDeviceLauncher> other_types_launcher)
      : media_device_launcher_(std::move(media_device_launcher)),
        other_types_launcher_(std::move(other_types_launcher)) {}

  ~VideoCaptureDeviceLauncherSwitcher() override {}

  void LaunchDeviceAsync(const std::string& device_id,
                         blink::mojom::MediaStreamType stream_type,
                         const media::VideoCaptureParams& params,
                         base::WeakPtr<media::VideoFrameReceiver> receiver,
                         base::OnceClosure connection_lost_cb,
                         Callbacks* callbacks,
                         base::OnceClosure done_cb) override {
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      // Use of Unretained() is safe, because |media_device_launcher_| is owned
      // by |this|.
      abort_launch_cb_ =
          base::Bind(&VideoCaptureDeviceLauncher::AbortLaunch,
                     base::Unretained(media_device_launcher_.get()));
      return media_device_launcher_->LaunchDeviceAsync(
          device_id, stream_type, params, std::move(receiver),
          std::move(connection_lost_cb), callbacks, std::move(done_cb));
    }
    // Use of Unretained() is safe, because |other_types_launcher_| is owned by
    // |this|.
    abort_launch_cb_ =
        base::Bind(&VideoCaptureDeviceLauncher::AbortLaunch,
                   base::Unretained(other_types_launcher_.get()));
    return other_types_launcher_->LaunchDeviceAsync(
        device_id, stream_type, params, std::move(receiver),
        std::move(connection_lost_cb), callbacks, std::move(done_cb));
  }

  void AbortLaunch() override {
    if (abort_launch_cb_.is_null())
      return;
    std::move(abort_launch_cb_).Run();
  }

 private:
  const std::unique_ptr<VideoCaptureDeviceLauncher> media_device_launcher_;
  const std::unique_ptr<VideoCaptureDeviceLauncher> other_types_launcher_;
  base::OnceClosure abort_launch_cb_;
};

}  // anonymous namespace

VideoCaptureProviderSwitcher::VideoCaptureProviderSwitcher(
    std::unique_ptr<VideoCaptureProvider> media_device_capture_provider,
    std::unique_ptr<VideoCaptureProvider> other_types_capture_provider)
    : media_device_capture_provider_(std::move(media_device_capture_provider)),
      other_types_capture_provider_(std::move(other_types_capture_provider)) {}

VideoCaptureProviderSwitcher::~VideoCaptureProviderSwitcher() = default;

void VideoCaptureProviderSwitcher::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  media_device_capture_provider_->GetDeviceInfosAsync(
      std::move(result_callback));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
VideoCaptureProviderSwitcher::CreateDeviceLauncher() {
  return std::make_unique<VideoCaptureDeviceLauncherSwitcher>(
      media_device_capture_provider_->CreateDeviceLauncher(),
      other_types_capture_provider_->CreateDeviceLauncher());
}

}  // namespace content
