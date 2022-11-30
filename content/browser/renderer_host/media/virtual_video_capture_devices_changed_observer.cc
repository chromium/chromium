// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/virtual_video_capture_devices_changed_observer.h"

#include "base/system/system_monitor.h"

namespace content {

VirtualVideoCaptureDevicesChangedObserver::
    VirtualVideoCaptureDevicesChangedObserver() = default;

VirtualVideoCaptureDevicesChangedObserver::
    ~VirtualVideoCaptureDevicesChangedObserver() = default;

void VirtualVideoCaptureDevicesChangedObserver::OnDevicesChanged() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (!monitor)
    return;
  monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

}  // namespace content
