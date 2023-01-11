// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_GPU_INFO_MONITOR_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_GPU_INFO_MONITOR_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/win/windows_types.h"
#include "gpu/config/gpu_info.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

// A singleton class keeps track of GPUInfo and manages notifications.
class MEDIA_MOJO_EXPORT MediaFoundationGpuInfoMonitor {
 public:
  static MediaFoundationGpuInfoMonitor* GetInstance();

  MediaFoundationGpuInfoMonitor();
  MediaFoundationGpuInfoMonitor(const MediaFoundationGpuInfoMonitor&) = delete;
  MediaFoundationGpuInfoMonitor operator=(
      const MediaFoundationGpuInfoMonitor&) = delete;
  ~MediaFoundationGpuInfoMonitor();

  // Updates GpuInfo in `this` and notify observers if needed.
  void UpdateGpuInfo(const gpu::GPUInfo& gpu_info);

  CHROME_LUID gpu_luid() const { return gpu_luid_; }

  // Adds an observer to get notified on GPU LUID changes.
  using LuidObservers = base::RepeatingCallbackList<void(const CHROME_LUID&)>;
  base::CallbackListSubscription AddLuidObserver(
      LuidObservers::CallbackType cb);

 private:
  CHROME_LUID gpu_luid_;
  LuidObservers luid_observers_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_GPU_INFO_MONITOR_H_
