// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_GPU_BROWSERTEST_HELPERS_H_
#define CONTENT_TEST_GPU_BROWSERTEST_HELPERS_H_

#include "base/memory/scoped_refptr.h"

namespace gpu {
class GpuChannelHost;
}

namespace viz {
class ContextProviderCommandBuffer;
}

namespace content {

// Synchronously establishes a connection to the GPU process and returns the
// GpuChannelHost.
scoped_refptr<gpu::GpuChannelHost>
GpuBrowsertestEstablishGpuChannelSyncRunLoop();

// Creates a new ContextProviderCommandBuffer using the provided
// GpuChannelHost.
scoped_refptr<viz::ContextProviderCommandBuffer> GpuBrowsertestCreateContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

}  // namespace content

#endif  // CONTENT_TEST_GPU_BROWSERTEST_HELPERS_H_
