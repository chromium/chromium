// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/media/capture/frame_sink_video_capture_device.h"
#include "content/browser/media/capture/web_contents_frame_tracker.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

namespace content {

// Captures the displayed contents of a WebContents, producing a stream of video
// frames.
//
// Generally, Create() is called with a device ID string that contains
// information necessary for finding a WebContents instance. Thereafter, this
// capture device will capture from the frame sink corresponding to the main
// frame of the RenderFrameHost tree for that WebContents instance. As the
// RenderFrameHost tree mutates (e.g., due to page navigations, crashes, or
// reloads), capture will continue without interruption.
class CONTENT_EXPORT WebContentsVideoCaptureDevice
    : public FrameSinkVideoCaptureDevice {
 public:
  explicit WebContentsVideoCaptureDevice(const GlobalRenderFrameHostId& id);

  WebContentsVideoCaptureDevice(WebContentsVideoCaptureDevice&&) = delete;
  WebContentsVideoCaptureDevice(const WebContentsVideoCaptureDevice&) = delete;
  WebContentsVideoCaptureDevice& operator=(
      const WebContentsVideoCaptureDevice&&) = delete;
  WebContentsVideoCaptureDevice& operator=(
      const WebContentsVideoCaptureDevice&) = delete;
  ~WebContentsVideoCaptureDevice() override;

  // Creates a WebContentsVideoCaptureDevice instance from the given
  // |device_id|. Returns null if |device_id| is invalid.
  static std::unique_ptr<WebContentsVideoCaptureDevice> Create(
      const std::string& device_id);

  // VideoCaptureDevice overrides.
  void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback) final;

  // FrameSinkVideoConsumer overrides.
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) final;

  // VideoCaptureDevice overrides.
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

  // For testing, we need the ability to create a device without its tracker.
 protected:
  WebContentsVideoCaptureDevice();

 private:
  // FrameSinkVideoCaptureDevice overrides: These increment/decrement the
  // WebContents's capturer count, which causes the embedder to be notified.
  void WillStart() final;
  void DidStop() final;

  gfx::Size content_size_;

  // A helper that runs on the UI thread to monitor changes to the
  // RenderFrameHost tree during the lifetime of a WebContents instance, and
  // posts notifications back to update the target frame sink.
  base::SequenceBound<WebContentsFrameTracker> tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to dispense WeakPtrs for the WebContentsFrameTracker to use to post
  // tasks to the VideoCaptureDevice.
  base::WeakPtrFactory<WebContentsVideoCaptureDevice> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
