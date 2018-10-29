// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/host_context_factory.h"

#include "base/bind.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/ids.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "ui/compositor/host/host_context_factory_private.h"

namespace ws {

// NOTE: resize_task_runner needs to be specialized on mac.
HostContextFactory::HostContextFactory(
    Gpu* gpu,
    viz::HostFrameSinkManager* host_frame_sink_manager)
    : gpu_(gpu),
      context_factory_private_(std::make_unique<ui::HostContextFactoryPrivate>(
          kWindowServerClientId,
          host_frame_sink_manager,
          base::ThreadTaskRunnerHandle::Get())),
      weak_ptr_factory_(this) {}

HostContextFactory::~HostContextFactory() = default;

ui::ContextFactoryPrivate* HostContextFactory::GetContextFactoryPrivate() {
  return context_factory_private_.get();
}

void HostContextFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  if (!compositor)
    return;

  scoped_refptr<viz::ContextProvider> context_provider =
      gpu_->CreateContextProvider(std::move(gpu_channel));
  // If the binding fails, then we need to return early since the compositor
  // expects a successfully initialized/bound provider.
  if (context_provider->BindToCurrentThread() != gpu::ContextResult::kSuccess) {
    // TODO(danakj): We should retry if the result was not kFatalFailure.
    return;
  }
  context_factory_private_->ConfigureCompositor(
      compositor.get(), std::move(context_provider), nullptr);
}

void HostContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  gpu_->EstablishGpuChannel(
      base::BindOnce(&HostContextFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
HostContextFactory::SharedMainThreadContextProvider() {
  if (!shared_main_thread_context_provider_) {
    scoped_refptr<gpu::GpuChannelHost> gpu_channel =
        gpu_->EstablishGpuChannelSync();
    shared_main_thread_context_provider_ =
        gpu_->CreateContextProvider(std::move(gpu_channel));
    if (shared_main_thread_context_provider_->BindToCurrentThread() !=
        gpu::ContextResult::kSuccess)
      shared_main_thread_context_provider_ = nullptr;
  }
  return shared_main_thread_context_provider_;
}

void HostContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  context_factory_private_->UnconfigureCompositor(compositor);
}

double HostContextFactory::GetRefreshRate() const {
  return 60.0;
}

gpu::GpuMemoryBufferManager* HostContextFactory::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

cc::TaskGraphRunner* HostContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

bool HostContextFactory::SyncTokensRequiredForDisplayCompositor() {
  // This runs out-of-process, so must be using a different context from the
  // UI compositor, and requires synchronization between them.
  return true;
}

}  // namespace ws
