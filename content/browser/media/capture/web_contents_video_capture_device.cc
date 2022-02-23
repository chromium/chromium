// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/capture/video_capture_types.h"
#include "ui/base/layout.h"

namespace content {

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    const GlobalRenderFrameHostId& id)
    : tracker_(new WebContentsFrameTracker(AsWeakPtr(), cursor_controller())) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId,
          tracker_->AsWeakPtr(), id));
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

void WebContentsVideoCaptureDevice::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK(callback);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::Token& crop_id, uint32_t crop_version,
             base::OnceCallback<void(media::mojom::CropRequestResult)> callback,
             base::WeakPtr<WebContentsFrameTracker> tracker) {
            if (!tracker) {
              std::move(callback).Run(
                  media::mojom::CropRequestResult::kErrorGeneric);
              return;
            }
            tracker->Crop(crop_id, crop_version, std::move(callback));
          },
          crop_id, crop_version, std::move(callback), tracker_->AsWeakPtr()));
}

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice() = default;

void WebContentsVideoCaptureDevice::WillStart() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebContentsFrameTracker::WillStartCapturingWebContents,
                     tracker_->AsWeakPtr(),
                     capture_params().SuggestConstraints().max_frame_size));
}

void WebContentsVideoCaptureDevice::DidStop() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebContentsFrameTracker::DidStopCapturingWebContents,
                     tracker_->AsWeakPtr()));
}

}  // namespace content
