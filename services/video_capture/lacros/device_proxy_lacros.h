// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_PROXY_LACROS_H_
#define SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_PROXY_LACROS_H_

#include <memory>

#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/video/video_capture_device_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device.h"

namespace video_capture {

class VideoFrameHandlerProxyLacros;

// A proxy which is used for communication between the client on Lacros-Chrome
// and the actual video_capture::Device in Ash-Chrome.
class DeviceProxyLacros : public video_capture::Device {
 public:
  DeviceProxyLacros(
      std::optional<mojo::PendingReceiver<mojom::Device>> device_receiver,
      mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote,
      base::OnceClosure cleanup_callback);
  DeviceProxyLacros(const DeviceProxyLacros&) = delete;
  DeviceProxyLacros& operator=(const DeviceProxyLacros&) = delete;
  ~DeviceProxyLacros() override;

 private:
  // video_capture::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<mojom::VideoFrameHandler> handler) override;
  void StartInProcess(
      const media::VideoCaptureParams& requested_settings,
      const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
      media::VideoEffectsContext context) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;
  void RequestRefreshFrame() override;

  std::unique_ptr<VideoFrameHandlerProxyLacros> handler_;

  mojo::Receiver<mojom::Device> receiver_{this};

  mojo::Remote<crosapi::mojom::VideoCaptureDevice> device_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_LACROS_DEVICE_PROXY_LACROS_H_
