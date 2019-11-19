// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/gpu_browsertest_helpers.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "url/gurl.h"

namespace content {

namespace {

void OnEstablishedGpuChannel(
    const base::RepeatingClosure& quit_closure,
    scoped_refptr<gpu::GpuChannelHost>* retvalue,
    scoped_refptr<gpu::GpuChannelHost> established_host) {
  if (retvalue)
    *retvalue = std::move(established_host);
  quit_closure.Run();
}

}  // namespace

scoped_refptr<gpu::GpuChannelHost>
GpuBrowsertestEstablishGpuChannelSyncRunLoop() {
  gpu::GpuChannelEstablishFactory* factory =
      content::BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();
  CHECK(factory);
  base::RunLoop run_loop;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host;
  factory->EstablishGpuChannel(base::BindOnce(
      &OnEstablishedGpuChannel, run_loop.QuitClosure(), &gpu_channel_host));
  run_loop.Run();
  return gpu_channel_host;
}

scoped_refptr<viz::ContextProviderCommandBuffer> GpuBrowsertestCreateContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  gpu::GpuChannelEstablishFactory* factory =
      content::BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();
  // This is for an offscreen context, so the default framebuffer doesn't need
  // any alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = true;
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
      content::kGpuStreamIdDefault, content::kGpuStreamPriorityDefault,
      gpu::kNullSurfaceHandle, GURL(), automatic_flushes, support_locking,
      support_grcontext, gpu::SharedMemoryLimits(), attributes,
      viz::command_buffer_metrics::ContextType::FOR_TESTING);
}

}  // namespace content
