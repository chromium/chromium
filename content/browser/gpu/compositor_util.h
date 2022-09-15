// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
#define CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"

namespace content {

// Note: When adding a function here, please make sure the logic is not
// duplicated in the renderer.

// Returns true if zero-copy uploads is on (via flags, or platform default).
// Only one of one-copy and zero-copy can be enabled at a time.
bool IsZeroCopyUploadEnabled();

// Returns true if a partial raster is on (via flags).
bool IsPartialRasterEnabled();

// Returns true if all compositor resources should use GPU memory buffers.
bool IsGpuMemoryBufferCompositorResourcesEnabled();

// Returns the number of multisample antialiasing samples (via flags) for
// GPU rasterization.
int GpuRasterizationMSAASampleCount();

// Returns the number of raster threads to use for compositing.
int NumberOfRendererRasterThreads();

// Returns true if main thread can be pipelined with activation.
bool IsMainFrameBeforeActivationEnabled();

base::Value GetFeatureStatus();
base::Value GetProblems();
std::vector<std::string> GetDriverBugWorkarounds();

base::Value GetFeatureStatusForHardwareGpu();
base::Value GetProblemsForHardwareGpu();
std::vector<std::string> GetDriverBugWorkaroundsForHardwareGpu();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
