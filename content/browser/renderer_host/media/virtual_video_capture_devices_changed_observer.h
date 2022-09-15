// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIRTUAL_VIDEO_CAPTURE_DEVICES_CHANGED_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIRTUAL_VIDEO_CAPTURE_DEVICES_CHANGED_OBSERVER_H_

#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"

namespace content {

// Implementation of video_capture::mojom::DevicesChangedObserver that forwards
// a devices changed event to the global (process-local) instance of
// base::DeviceMonitor.
class VirtualVideoCaptureDevicesChangedObserver
    : public video_capture::mojom::DevicesChangedObserver {
 public:
  VirtualVideoCaptureDevicesChangedObserver();
  ~VirtualVideoCaptureDevicesChangedObserver() override;

  // video_capture::mojom::DevicesChangedObserver implementation:
  void OnDevicesChanged() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIRTUAL_VIDEO_CAPTURE_DEVICES_CHANGED_OBSERVER_H_
