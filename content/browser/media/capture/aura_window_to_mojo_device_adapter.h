// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_TO_MOJO_DEVICE_ADAPTER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_TO_MOJO_DEVICE_ADAPTER_H_

#include <memory>

#include "content/browser/media/capture/aura_window_video_capture_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/mojom/device.mojom.h"

namespace content {
struct DesktopMediaID;

// Implementation of mojom::Device backed by an instance of a
// AuraWindowVideoCaptureDevice. Note that because AuraWindowVideoCaptureDevice
// does not make use of frame pooling, and thus cannot be started through the
// typical VideoCaptureDevice interface, we cannot use the
// video_capture::DeviceMediaToMojoAdapter. Fortunately, adapting the
// VideoFrameHandler to the type that it expects (media::VideoFrameReceiver)
// is relatively trivial.
class AuraWindowToMojoDeviceAdapter : public video_capture::mojom::Device {
 public:
  explicit AuraWindowToMojoDeviceAdapter(
      const content::DesktopMediaID& device_id);
  ~AuraWindowToMojoDeviceAdapter() override;

  // mojom::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
                 handler_pending_remote) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;
  void RequestRefreshFrame() override;

 private:
  const std::unique_ptr<content::AuraWindowVideoCaptureDevice> device_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_TO_MOJO_DEVICE_ADAPTER_H_
