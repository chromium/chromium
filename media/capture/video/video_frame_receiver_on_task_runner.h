// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_ON_TASK_RUNNER_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_ON_TASK_RUNNER_H_

#include "media/capture/video/video_frame_receiver.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Decorator for VideoFrameReceiver that forwards all incoming calls to the
// given |task_runner|.
class CAPTURE_EXPORT VideoFrameReceiverOnTaskRunner
    : public VideoFrameReceiver {
 public:
  explicit VideoFrameReceiverOnTaskRunner(
      const base::WeakPtr<VideoFrameReceiver>& receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~VideoFrameReceiverOnTaskRunner() override;

  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(ReadyFrameInBuffer frame) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError(VideoCaptureError error) override;
  void OnFrameDropped(VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

 private:
  const base::WeakPtr<VideoFrameReceiver> receiver_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_ON_TASK_RUNNER_H_
