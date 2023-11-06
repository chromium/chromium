// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    const GlobalRenderFrameHostId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker_ = base::SequenceBound<WebContentsFrameTracker>(
      GetUIThreadTaskRunner({}),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      weak_ptr_factory_.GetWeakPtr(), cursor_controller());
  tracker_
      .AsyncCall(
          &WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId)
      .WithArgs(id);
}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() = default;

// static
std::unique_ptr<WebContentsVideoCaptureDevice>
WebContentsVideoCaptureDevice::Create(const std::string& device_id) {
  WebContentsMediaCaptureId media_id;
  if (!WebContentsMediaCaptureId::Parse(device_id, &media_id)) {
    return nullptr;
  }

  const GlobalRenderFrameHostId routing_id(media_id.render_process_id,
                                           media_id.main_render_frame_id);
  return std::make_unique<WebContentsVideoCaptureDevice>(routing_id);
}

void WebContentsVideoCaptureDevice::ApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  tracker_.AsyncCall(&WebContentsFrameTracker::ApplySubCaptureTarget)
      .WithArgs(type, target, sub_capture_target_version,
                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                    std::move(callback),
                    media::mojom::ApplySubCaptureTargetResult::kErrorGeneric));
}

void WebContentsVideoCaptureDevice::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (info->metadata.source_size.has_value()) {
    const gfx::Size new_size = *info->metadata.source_size;
    // Only update the captured content size when the size changes. See also
    // the comment in WebContentsFrameTracker::SetCapturedContentSize which
    // expects that behavior.
    if (new_size != content_size_) {
      tracker_.AsyncCall(&WebContentsFrameTracker::SetCapturedContentSize)
          .WithArgs(new_size);
      content_size_ = new_size;
    }
  }

  FrameSinkVideoCaptureDevice::OnFrameCaptured(
      std::move(data), std::move(info), content_rect, std::move(callbacks));
}

void WebContentsVideoCaptureDevice::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  tracker_.AsyncCall(&WebContentsFrameTracker::OnUtilizationReport)
      .WithArgs(feedback);

  // We still want to capture the base class' behavior for utilization reports.
  FrameSinkVideoCaptureDevice::OnUtilizationReport(std::move(feedback));
}

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice() = default;

void WebContentsVideoCaptureDevice::WillStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker_.AsyncCall(&WebContentsFrameTracker::WillStartCapturingWebContents)
      .WithArgs(capture_params().SuggestConstraints().max_frame_size,
                capture_params().is_high_dpi_enabled);
}

void WebContentsVideoCaptureDevice::DidStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracker_.AsyncCall(&WebContentsFrameTracker::DidStopCapturingWebContents);

  // Currently, the video capture device is effectively a single-use object, so
  // resetting capture_size_ isn't strictly necessary, but it helps ensure that
  // SetCapturedContentSize works consistently in case the objects get reused in
  // the future.
  content_size_.SetSize(0, 0);
}

}  // namespace content
