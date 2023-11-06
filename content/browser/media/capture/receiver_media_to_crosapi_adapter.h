// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_RECEIVER_MEDIA_TO_CROSAPI_ADAPTER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_RECEIVER_MEDIA_TO_CROSAPI_ADAPTER_H_
#include <string>

#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/video/video_frame_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// This class is responsible for translating the crosapi::VideoFrameHandler
// calls (typically from the Ash-chrome capturer) to the
// media::VideoFrameReceiver types used within the browser-process of Lacros-
// chrome. The crosapi types are simplified for use with the static API, but
// the rest of the video capture pipeline expects the less-simplified version.
class ReceiverMediaToCrosapiAdapter : public crosapi::mojom::VideoFrameHandler {
 public:
  ReceiverMediaToCrosapiAdapter(
      mojo::PendingReceiver<crosapi::mojom::VideoFrameHandler> proxy_receiver,
      std::unique_ptr<media::VideoFrameReceiver> handler);
  ReceiverMediaToCrosapiAdapter(const ReceiverMediaToCrosapiAdapter&) = delete;
  ReceiverMediaToCrosapiAdapter& operator=(
      const ReceiverMediaToCrosapiAdapter&) = delete;
  ~ReceiverMediaToCrosapiAdapter() override;

  // crosapi::mojom::VideoFrameHandler implementation that others may need to
  // call.
  void OnError(media::VideoCaptureError error) override;
  void OnLog(const std::string& message) override;

 private:
  // crosapi::mojom::VideoFrameHandler implementation.
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int buffer_id,
                   crosapi::mojom::VideoBufferHandlePtr buffer_handle) override;
  void DEPRECATED_OnFrameReadyInBuffer(
      crosapi::mojom::ReadyFrameInBufferPtr buffer,
      std::vector<crosapi::mojom::ReadyFrameInBufferPtr> scaled_buffers)
      override;
  void OnFrameReadyInBuffer(
      crosapi::mojom::ReadyFrameInBufferPtr buffer) override;
  void OnBufferRetired(int buffer_id) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void DEPRECATED_OnNewCropVersion(uint32_t crop_version) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

  mojo::Receiver<crosapi::mojom::VideoFrameHandler> receiver_{this};
  std::unique_ptr<media::VideoFrameReceiver> handler_;
};

}  // namespace video_capture

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_RECEIVER_MEDIA_TO_CROSAPI_ADAPTER_H_
