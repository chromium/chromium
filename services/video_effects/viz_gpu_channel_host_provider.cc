// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/viz_gpu_channel_host_provider.h"

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/cxx23_to_underlying.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/video_effects/video_effects_service_impl.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "url/gurl.h"

namespace {

scoped_refptr<viz::ContextProviderCommandBuffer> CreateAndBindContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::ContextType context_type) {
  CHECK(gpu_channel_host);
  CHECK(!gpu_channel_host->IsLost());
  CHECK(context_type == gpu::CONTEXT_TYPE_WEBGPU ||
        context_type == gpu::CONTEXT_TYPE_OPENGLES2);

  auto context_creation_attribs = gpu::ContextCreationAttribs();
  context_creation_attribs.context_type = context_type;
  context_creation_attribs.enable_gles2_interface = false;
  context_creation_attribs.enable_raster_interface =
      context_type == gpu::CONTEXT_TYPE_OPENGLES2;
  context_creation_attribs.bind_generates_resource =
      context_type == gpu::CONTEXT_TYPE_WEBGPU;

  // TODO(bialpio): replace `gpu::SharedMemoryLimits::ForOOPRasterContext()`
  // with something better suited or explain why it's appropriate the way it is
  // now.
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), content::kGpuStreamIdDefault,
          gpu::SchedulingPriority::kNormal, gpu::kNullSurfaceHandle,
          GURL("chrome://gpu/VideoEffects"), true /* automatic flushes */,
          false /* support locking */,
          context_type == gpu::CONTEXT_TYPE_WEBGPU
              ? gpu::SharedMemoryLimits::ForWebGPUContext()
              : gpu::SharedMemoryLimits::ForOOPRasterContext(),
          context_creation_attribs,
          viz::command_buffer_metrics::ContextType::VIDEO_CAPTURE);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentSequence();
  if (context_result != gpu::ContextResult::kSuccess) {
    LOG(ERROR) << "Bind context provider failed. context_result: "
               << base::to_underlying(context_result);
    return nullptr;
  }

  return context_provider;
}

}  // namespace

namespace video_effects {

VizGpuChannelHostProvider::VizGpuChannelHostProvider(
    std::unique_ptr<viz::Gpu> viz_gpu)
    : viz_gpu_(std::move(viz_gpu)) {
  CHECK(viz_gpu_);
}

VizGpuChannelHostProvider::~VizGpuChannelHostProvider() = default;

scoped_refptr<viz::ContextProviderCommandBuffer>
VizGpuChannelHostProvider::GetWebGpuContextProvider() {
  if (webgpu_context_provider_) {
    return webgpu_context_provider_;
  }
  webgpu_context_provider_ = CreateAndBindContextProvider(
      GetGpuChannelHost(), gpu::CONTEXT_TYPE_WEBGPU);
  return webgpu_context_provider_;
}

scoped_refptr<viz::RasterContextProvider>
VizGpuChannelHostProvider::GetRasterInterfaceContextProvider() {
  if (raster_interface_context_provider_) {
    return raster_interface_context_provider_;
  }
  raster_interface_context_provider_ = CreateAndBindContextProvider(
      GetGpuChannelHost(), gpu::CONTEXT_TYPE_OPENGLES2);
  return raster_interface_context_provider_;
}

scoped_refptr<gpu::ClientSharedImageInterface>
VizGpuChannelHostProvider::GetSharedImageInterface() {
  if (shared_image_interface_) {
    return shared_image_interface_;
  }
  shared_image_interface_ =
      GetGpuChannelHost()->CreateClientSharedImageInterface();
  return shared_image_interface_;
}

scoped_refptr<gpu::GpuChannelHost>
VizGpuChannelHostProvider::GetGpuChannelHost() {
  if (!gpu_channel_host_) {
    gpu_channel_host_ = viz_gpu_->GetGpuChannel();
  }
  if (!gpu_channel_host_ || gpu_channel_host_->IsLost()) {
    gpu_channel_host_ = viz_gpu_->EstablishGpuChannelSync();
  }
  return gpu_channel_host_;
}

}  // namespace video_effects
