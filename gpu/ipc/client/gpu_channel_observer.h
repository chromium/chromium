// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CHANNEL_OBSERVER_H_
#define GPU_IPC_CLIENT_GPU_CHANNEL_OBSERVER_H_

#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT GpuChannelLostObserver {
 public:
  GpuChannelLostObserver() = default;
  ~GpuChannelLostObserver() = default;

  virtual void OnGpuChannelLost() = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_CHANNEL_OBSERVER_H_
