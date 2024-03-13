// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/gpu_browsertest_helpers.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    bool wants_raster_interface) {
  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = !wants_raster_interface;
  attributes.enable_grcontext = false;
  attributes.enable_raster_interface = wants_raster_interface;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), content::kGpuStreamIdDefault,
      content::kGpuStreamPriorityDefault, gpu::kNullSurfaceHandle, GURL(),
      automatic_flushes, support_locking, gpu::SharedMemoryLimits(), attributes,
      viz::command_buffer_metrics::ContextType::FOR_TESTING);
}

}  // namespace content
