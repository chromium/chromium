// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/shared_worker_context_provider_factory.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/base/ui_base_features.h"

namespace viz {
namespace {

bool CheckWorkerContextLost(RasterContextProvider* context_provider) {
  if (!context_provider)
    return false;

  RasterContextProvider::ScopedRasterContextLock lock(context_provider);
  return lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

}  // namespace

SharedWorkerContextProviderFactory::SharedWorkerContextProviderFactory(
    int32_t stream_id,
    gpu::SchedulingPriority priority,
    const GURL& identifying_url,
    command_buffer_metrics::ContextType context_type)
    : stream_id_(stream_id),
      priority_(priority),
      identifying_url_(identifying_url),
      context_type_(context_type) {}

SharedWorkerContextProviderFactory::~SharedWorkerContextProviderFactory() =
    default;

void SharedWorkerContextProviderFactory::Reset() {
  provider_ = nullptr;
}

gpu::ContextResult SharedWorkerContextProviderFactory::Validate(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  if (CheckWorkerContextLost(provider_.get()))
    provider_ = nullptr;

  if (provider_)
    return gpu::ContextResult::kSuccess;

  const auto& gpu_feature_info = gpu_channel_host->gpu_feature_info();
  bool enable_oop_rasterization =
      features::IsUiGpuRasterizationEnabled() &&
      gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;
  bool enable_gpu_rasterization =
      features::IsUiGpuRasterizationEnabled() && !enable_oop_rasterization;

  // TODO(crbug.com/909568): refactor
  // RenderThreadImpl::SharedCompositorWorkerContextProvider to use this.
  const bool need_alpha_channel = false;
  const bool support_locking = true;
  const bool support_gles2_interface = enable_gpu_rasterization;
  const bool support_raster_interface = true;
  const bool support_grcontext = enable_gpu_rasterization;
  const bool support_oopr = enable_oop_rasterization;

  provider_ = CreateContextProvider(
      std::move(gpu_channel_host), gpu_memory_buffer_manager,
      gpu::kNullSurfaceHandle, need_alpha_channel, /*support_stencil=*/false,
      support_locking, support_gles2_interface, support_raster_interface,
      support_grcontext, support_oopr, context_type_);
  auto result = provider_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    provider_ = nullptr;
  return result;
}

scoped_refptr<RasterContextProvider>
SharedWorkerContextProviderFactory::CreateContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::SurfaceHandle surface_handle,
    bool need_alpha_channel,
    bool need_stencil_bits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_grcontext,
    bool support_oopr,
    command_buffer_metrics::ContextType type) {
  DCHECK(gpu_channel_host);

  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = need_alpha_channel ? 8 : -1;
  attributes.depth_size = 0;
  attributes.stencil_size = need_stencil_bits ? 8 : 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;
  attributes.enable_oop_rasterization = support_oopr;
  gpu::SharedMemoryLimits memory_limits =
      gpu::SharedMemoryLimits::ForDisplayCompositor();

  constexpr bool automatic_flushes = false;

  return base::MakeRefCounted<ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager, stream_id_,
      priority_, surface_handle, identifying_url_, automatic_flushes,
      support_locking, support_grcontext, memory_limits, attributes, type);
}

}  // namespace viz
