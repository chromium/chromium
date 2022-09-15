// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_

#include <string>

#include "gpu/command_buffer/common/constants.h"

namespace viz {
namespace command_buffer_metrics {

// A rough classification for what the context is used for. These enum types
// correspond to the GPU.ContextLost UMA suffixes. Make sure to update
// UmaRecordContextLost() and tools/metrics/histograms/histograms.xml when
// adding a new value here.
enum class ContextType {
  BROWSER_COMPOSITOR,
  BROWSER_MAIN_THREAD,
  BROWSER_WORKER,
  RENDER_COMPOSITOR,
  RENDER_WORKER,
  RENDERER_MAIN_THREAD,
  VIDEO_ACCELERATOR,
  VIDEO_CAPTURE,
  WEBGL,
  WEBGPU,
  MEDIA,
  UNKNOWN,
  FOR_TESTING,
  XR_COMPOSITING,
};

std::string ContextTypeToString(ContextType type);

void UmaRecordContextInitFailed(ContextType type);

void UmaRecordContextLost(ContextType type,
                          gpu::error::Error error,
                          gpu::error::ContextLostReason reason);

}  // namespace command_buffer_metrics
}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_COMMAND_BUFFER_METRICS_H_
