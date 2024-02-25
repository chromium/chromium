// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_

#include "media/capture/video/video_frame_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// Adapter that allows a media::VideoFrameReceiver to be used in place of
// a mojom::VideoFrameHandler.
class ReceiverMediaToMojoAdapter : public mojom::VideoFrameHandler {
 public:
  ReceiverMediaToMojoAdapter(
      std::unique_ptr<media::VideoFrameReceiver> receiver);
  ~ReceiverMediaToMojoAdapter() override;

  // video_capture::mojom::VideoFrameHandler:
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameAccessHandlerReady(
      mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
          pending_frame_access_handler) override;
  void OnFrameReadyInBuffer(mojom::ReadyFrameInBufferPtr buffer) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

 private:
  std::unique_ptr<media::VideoFrameReceiver> receiver_;
  scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MEDIA_TO_MOJO_ADAPTER_H_
