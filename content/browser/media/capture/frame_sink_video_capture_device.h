// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

class MouseCursorOverlayController;

// A virtualized VideoCaptureDevice that captures the displayed contents of a
// frame sink (see viz::CompositorFrameSink), such as the composited main view
// of a WebContents instance, producing a stream of video frames.
//
// From the point-of-view of the VIZ service, this is a consumer of video frames
// (viz::mojom::FrameSinkVideoConsumer). However, from the point-of-view of the
// video capture stack, this is a device (media::VideoCaptureDevice) that
// produces video frames. Therefore, a FrameSinkVideoCaptureDevice is really a
// proxy between the two subsystems.
//
// Usually, a subclass implementation is instantiated and used, such as
// WebContentsVideoCaptureDevice or AuraWindowCaptureDevice. These subclasses
// provide additional implementation, to update which frame sink is targeted for
// capture, and to notify other components that capture is taking place.
class CONTENT_EXPORT FrameSinkVideoCaptureDevice
    : public media::VideoCaptureDevice,
      public viz::mojom::FrameSinkVideoConsumer {
 public:
  FrameSinkVideoCaptureDevice();
  ~FrameSinkVideoCaptureDevice() override;

  // Deviation from the VideoCaptureDevice interface: Since the memory pooling
  // provided by a VideoCaptureDevice::Client is not needed, this
  // FrameSinkVideoCaptureDevice will provide frames to a VideoFrameReceiver
  // directly.
  void AllocateAndStartWithReceiver(
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoFrameReceiver> receiver);

  // Returns the VideoCaptureParams passed to AllocateAndStartWithReceiver().
  const media::VideoCaptureParams& capture_params() const {
    return capture_params_;
  }

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void RequestRefreshFrame() final;
  void MaybeSuspend() final;
  void Resume() final;
  void StopAndDeAllocate() final;
  void OnUtilizationReport(int frame_feedback_id, double utilization) final;

  // FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) final;
  void OnStopped() final;

  // These are called to notify when the capture target has changed or was
  // permanently lost.
  void OnTargetChanged(const viz::FrameSinkId& frame_sink_id);
  void OnTargetPermanentlyLost();

 protected:
  MouseCursorOverlayController* cursor_controller() const {
    return cursor_controller_.get();
  }

  // Subclasses override these to perform additional start/stop tasks.
  virtual void WillStart();
  virtual void DidStop();

  // Establishes connection to FrameSinkVideoCapturer. The default
  // implementation calls CreateCapturerViaGlobalManager(), but subclasses
  // and/or tests may provide alternatives.
  virtual void CreateCapturer(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver);

  // Establishes connection to FrameSinkVideoCapturer using the global
  // viz::HostFrameSinkManager.
  static void CreateCapturerViaGlobalManager(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver);

 private:
  using BufferId = decltype(media::VideoCaptureDevice::Client::Buffer::id);

  // If not consuming and all preconditions are met, set up and start consuming.
  void MaybeStartConsuming();

  // If consuming, shut it down.
  void MaybeStopConsuming();

  // Notifies the capturer that consumption of the frame is complete.
  void OnFramePropagationComplete(BufferId buffer_id);

  // Helper that logs the given error |message| to the |receiver_| and then
  // stops capture and this VideoCaptureDevice.
  void OnFatalError(std::string message);

  // Helper that requests wake lock to prevent the display from sleeping while
  // capturing is going on.
  void RequestWakeLock(std::unique_ptr<service_manager::Connector> connector);

  // Current capture target. This is cached to resolve a race where
  // OnTargetChanged() can be called before the |capturer_| is created in
  // OnCapturerCreated().
  viz::FrameSinkId target_;

  // The requested format, rate, and other capture constraints.
  media::VideoCaptureParams capture_params_;

  // Set to true when MaybeSuspend() is called, and false when Resume() is
  // called. This reflects the needs of the downstream client.
  bool suspend_requested_ = false;

  // Receives video frames from this capture device, for propagation into the
  // video capture stack. This is set by AllocateAndStartWithReceiver(), and
  // cleared by StopAndDeAllocate().
  std::unique_ptr<media::VideoFrameReceiver> receiver_;

  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> capturer_;

  // A vector that holds the "callbacks" mojo::Remote for each frame while the
  // frame is being processed by VideoFrameReceiver. The index corresponding to
  // a particular frame is used as the BufferId passed to VideoFrameReceiver.
  // Therefore, non-null pointers in this vector must never move to a different
  // position.
  std::vector<mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>>
      frame_callbacks_;

  // Set when OnFatalError() is called. This prevents any future
  // AllocateAndStartWithReceiver() calls from succeeding.
  base::Optional<std::string> fatal_error_message_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Controls the overlay that renders the mouse cursor onto each video frame.
  const std::unique_ptr<MouseCursorOverlayController,
                        BrowserThread::DeleteOnUIThread>
      cursor_controller_;

  // Prevent display sleeping while content capture is in progress.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  // Creates WeakPtrs for use on the device thread.
  base::WeakPtrFactory<FrameSinkVideoCaptureDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameSinkVideoCaptureDevice);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_
