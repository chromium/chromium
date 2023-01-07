// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_gpu_info_monitor.h"

namespace media {

// static
MediaFoundationGpuInfoMonitor* MediaFoundationGpuInfoMonitor::GetInstance() {
  static auto* instance = new MediaFoundationGpuInfoMonitor();
  return instance;
}

MediaFoundationGpuInfoMonitor::MediaFoundationGpuInfoMonitor() = default;
MediaFoundationGpuInfoMonitor::~MediaFoundationGpuInfoMonitor() = default;

void MediaFoundationGpuInfoMonitor::UpdateGpuInfo(
    const gpu::GPUInfo& gpu_info) {
  auto new_gpu_luid = gpu_info.active_gpu().luid;
  if (new_gpu_luid != gpu_luid_) {
    gpu_luid_ = new_gpu_luid;
    luid_observers_.Notify(new_gpu_luid);
  }
}

base::CallbackListSubscription MediaFoundationGpuInfoMonitor::AddLuidObserver(
    MediaFoundationGpuInfoMonitor::LuidObservers::CallbackType cb) {
  return luid_observers_.Add(std::move(cb));
}

}  // namespace media