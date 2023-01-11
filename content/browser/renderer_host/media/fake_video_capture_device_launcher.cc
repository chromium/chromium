// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/fake_video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class FakeLaunchedVideoCaptureDevice
    : public content::LaunchedVideoCaptureDevice {
 public:
  FakeLaunchedVideoCaptureDevice(
      std::unique_ptr<media::VideoCaptureDevice> device)
      : device_(std::move(device)) {}

  void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) override {
    device_->GetPhotoState(std::move(callback));
  }
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) override {
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
  }
  void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) override {
    device_->TakePhoto(std::move(callback));
  }
  void MaybeSuspendDevice() override { device_->MaybeSuspend(); }
  void ResumeDevice() override { device_->Resume(); }
  void Crop(const base::Token& crop_id,
            uint32_t crop_version,
            base::OnceCallback<void(media::mojom::CropRequestResult)> callback)
      override {
    device_->Crop(crop_id, crop_version, std::move(callback));
  }
  void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }
  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override {
    // Do nothing.
  }
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override {
    device_->OnUtilizationReport(feedback);
  }

 private:
  std::unique_ptr<media::VideoCaptureDevice> device_;
};

}  // anonymous namespace

namespace content {

FakeVideoCaptureDeviceLauncher::FakeVideoCaptureDeviceLauncher(
    media::VideoCaptureSystem* system)
    : system_(system) {
  DCHECK(system_);
}

FakeVideoCaptureDeviceLauncher::~FakeVideoCaptureDeviceLauncher() = default;

void FakeVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  auto device = system_->CreateDevice(device_id).ReleaseDevice();
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
      new media::VideoCaptureBufferPoolImpl(
          media::VideoCaptureBufferType::kSharedMemory));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      media::VideoCaptureBufferType::kSharedMemory,
      std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
          receiver, base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(buffer_pool), base::BindRepeating([]() {
        return std::unique_ptr<media::VideoCaptureJpegDecoder>();
      }));
#else
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      media::VideoCaptureBufferType::kSharedMemory,
      std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
          receiver, base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(buffer_pool));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  device->AllocateAndStart(params, std::move(device_client));
  auto launched_device =
      std::make_unique<FakeLaunchedVideoCaptureDevice>(std::move(device));
  callbacks->OnDeviceLaunched(std::move(launched_device));
}

void FakeVideoCaptureDeviceLauncher::AbortLaunch() {
  // Do nothing.
}

}  // namespace content
