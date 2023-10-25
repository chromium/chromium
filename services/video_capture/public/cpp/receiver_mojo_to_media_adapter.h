// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_

#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/video_frame_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// Adapter that allows a mojom::VideoFrameReceiver to be used in place of
// a media::VideoFrameReceiver.
class ReceiverMojoToMediaAdapter : public media::VideoFrameReceiver {
 public:
  ReceiverMojoToMediaAdapter(mojo::Remote<mojom::VideoFrameHandler> handler);
  ~ReceiverMojoToMediaAdapter() override;

  base::WeakPtr<media::VideoFrameReceiver> GetWeakPtr();

  // media::VideoFrameReceiver implementation.
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override;
  void OnBufferRetired(int buffer_id) override;
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
  mojo::Remote<mojom::VideoFrameHandler> video_frame_handler_;
  scoped_refptr<ScopedAccessPermissionMap> scoped_access_permission_map_;
  base::WeakPtrFactory<ReceiverMojoToMediaAdapter> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_
