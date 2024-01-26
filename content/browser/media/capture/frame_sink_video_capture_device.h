// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/viz/public/cpp/compositing/video_capture_target_mojom_traits.h"
#include "ui/compositor/compositor.h"

namespace content {

class MouseCursorOverlayController;

class ContextProviderObserver;

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

  FrameSinkVideoCaptureDevice(const FrameSinkVideoCaptureDevice&) = delete;
  FrameSinkVideoCaptureDevice& operator=(const FrameSinkVideoCaptureDevice&) =
      delete;

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
  void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback) override;
  void StopAndDeAllocate() final;
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

  // FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewSubCaptureTargetVersion(uint32_t sub_capture_target_version) final;
  void OnFrameWithEmptyRegionCapture() final;
  void OnStopped() final;
  void OnLog(const std::string& message) final;

  // These are called to notify when the capture target has changed or was
  // permanently lost. NOTE: a target can be temporarily std::nullopt without
  // being permanently lost.
  virtual void OnTargetChanged(
      const std::optional<viz::VideoCaptureTarget>& target,
      uint32_t sub_capture_target_version);
  virtual void OnTargetPermanentlyLost();

 protected:
  MouseCursorOverlayController* cursor_controller() const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    return cursor_controller_.get();
#else
    return nullptr;
#endif
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

  void AllocateAndStartWithReceiverInternal();

  // Starts observing changes to context provider.
  void ObserveContextProvider();

  // Re-creates the |capturer_| if needed. The capturer will be recreated (and
  // re-started if the current one was running) if it is configured to use a
  // pixel format that is different than the pixel format that we are able to
  // use given current device capabilities (e.g. when a capturer was configured
  // to use NV12 format but conditions changed and now we can only capture
  // I420 format).
  void RestartCapturerIfNeeded();

  // Helper, checks if the FrameSinkVideoCapturer should be able to support
  // capture using NV12 pixel format - this depends on device capabilities.
  bool CanSupportNV12Format() const;

  // Helper, returns desired video pixel format based on the contents of
  // |capture_parameters_|. If the capture parameters specify
  // PIXEL_FORMAT_UNKNOWN, it means we need to decide between I420 and NV12.
  media::VideoPixelFormat GetDesiredVideoPixelFormat() const;

  void AllocateCapturer(media::VideoPixelFormat pixel_format);

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
  void RequestWakeLock();

  // Helper to set `gpu_capabilities_` on the appropriate thread. Can be called
  // from any thread, will hop to the sequence on which the device was created.
  // This indirection is needed to support cancellation of handed out callbacks.
  void SetGpuCapabilitiesOnDevice(
      std::optional<gpu::Capabilities> capabilities);

  // Current capture target. This is cached to resolve a race where
  // `OnTargetChanged()` can be called before the |capturer_| is created in
  // `OnCapturerCreated()`.
  std::optional<viz::VideoCaptureTarget> target_;

  // The requested format, rate, and other capture constraints.
  media::VideoCaptureParams capture_params_;

  // Set to true when `MaybeSuspend()` is called, and false when Resume() is
  // called. This reflects the needs of the downstream client.
  bool suspend_requested_ = false;

  // Receives video frames from this capture device, for propagation into the
  // video capture stack. This is set by `AllocateAndStartWithReceiver()`, and
  // cleared by `StopAndDeAllocate()`.
  std::unique_ptr<media::VideoFrameReceiver> receiver_;

  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> capturer_;

  // Capabilities obtained from `viz::ContextProvider`.
  std::optional<gpu::Capabilities> gpu_capabilities_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Instance that is responsible for monitoring for context loss events on the
  // `viz::ContextProvider`. May be null.
  std::unique_ptr<ContextProviderObserver, BrowserThread::DeleteOnUIThread>
      context_provider_observer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A vector that holds the "callbacks" mojo::Remote for each frame while the
  // frame is being processed by VideoFrameReceiver. The index corresponding to
  // a particular frame is used as the BufferId passed to VideoFrameReceiver.
  // Therefore, non-null pointers in this vector must never move to a different
  // position.
  std::vector<mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>>
      frame_callbacks_;

  // Set when `OnFatalError()` is called. This prevents any future
  // AllocateAndStartWithReceiver() calls from succeeding.
  std::optional<std::string> fatal_error_message_;

  SEQUENCE_CHECKER(sequence_checker_);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Controls the overlay that renders the mouse cursor onto each video frame.
  const std::unique_ptr<MouseCursorOverlayController,
                        BrowserThread::DeleteOnUIThread>
      cursor_controller_;
#endif

  // Whenever the sub-capture-target of a stream changes, the associated
  // sub-capture-target-version is incremented. This value is used in frames'
  // metadata so as to allow other modules (mostly Blink) to see which frames
  // are cropped/restricted to the old/new specified sub-capture-target.
  // The value 0 is used before any sub-capture-target is assigned.
  // (Note that by applying and then removing a sub-capture target,
  // values other than 0 can also be associated with an uncropped track.)
  uint32_t sub_capture_target_version_ = 0;

  // Prevent display sleeping while content capture is in progress.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  // Creates WeakPtrs for use on the device thread.
  base::WeakPtrFactory<FrameSinkVideoCaptureDevice> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FRAME_SINK_VIDEO_CAPTURE_DEVICE_H_
