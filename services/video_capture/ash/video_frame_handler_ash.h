// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_ASH_VIDEO_FRAME_HANDLER_ASH_H_
#define SERVICES_VIDEO_CAPTURE_ASH_VIDEO_FRAME_HANDLER_ASH_H_

#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace crosapi {

// It is used as a proxy to communicate between actual VideoFrameHandler on
// Lacros-Chrome and the actual video capture device on Ash-Chrome. Since we
// have simplified some structures in crosapi video capture interface to reduce
// dependencies to other components, this class should also be responsible for
// translating those structures between the interfaces.
class VideoFrameHandlerAsh : public video_capture::mojom::VideoFrameHandler {
 public:
  VideoFrameHandlerAsh(
      mojo::PendingReceiver<video_capture::mojom::VideoFrameHandler>
          handler_receiver,
      mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_remote);
  VideoFrameHandlerAsh(const VideoFrameHandlerAsh&) = delete;
  VideoFrameHandlerAsh& operator=(const VideoFrameHandlerAsh&) = delete;
  ~VideoFrameHandlerAsh() override;

  // Implements crosapi::mojom::ScopedAccessPermission, which means that when
  // the mojo pipe associated with the scoped access permission is torn down,
  // the ScopedFrameAccessHandlerNotifier destuctor is invoked. The destructor
  // informs the |frame_access_handler_remote_| that the frame was released.
  class ScopedFrameAccessHandlerNotifier
      : public crosapi::mojom::ScopedAccessPermission {
   public:
    ScopedFrameAccessHandlerNotifier(
        scoped_refptr<video_capture::VideoFrameAccessHandlerRemote>
            frame_access_handler_remote,
        int32_t buffer_id);
    ScopedFrameAccessHandlerNotifier(const ScopedFrameAccessHandlerNotifier&) =
        delete;
    ScopedFrameAccessHandlerNotifier& operator=(
        const ScopedFrameAccessHandlerNotifier&) = delete;
    ~ScopedFrameAccessHandlerNotifier() override;

   private:
    scoped_refptr<video_capture::VideoFrameAccessHandlerRemote>
        frame_access_handler_remote_;
    const int32_t buffer_id_;
  };

 private:
  // video_capture::mojom::VideoFrameHandler implementation.
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameAccessHandlerReady(
      mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
          pending_frame_access_handler) override;
  void OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) override;
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

  mojo::Receiver<video_capture::mojom::VideoFrameHandler> receiver_{this};

  mojo::Remote<crosapi::mojom::VideoFrameHandler> proxy_;
  scoped_refptr<video_capture::VideoFrameAccessHandlerRemote>
      frame_access_handler_remote_;
};

}  // namespace crosapi

#endif  // SERVICES_VIDEO_CAPTURE_ASH_VIDEO_FRAME_HANDLER_ASH_H_
