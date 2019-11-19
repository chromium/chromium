// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
class CommandBufferHelper;
class CommandBufferProxyImpl;
class GpuChannelHost;
struct GpuFeatureInfo;
class GpuMemoryBufferManager;
class ImplementationBase;
class TransferBuffer;

namespace gles2 {
class GLES2Implementation;
class GLES2TraceImplementation;
}  // namespace gles2

namespace raster {
class RasterInterface;
}  // namespace raster

namespace webgpu {
class WebGPUInterface;
}  // namespace webgpu
}  // namespace gpu

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {

// Implementation of ContextProvider that provides a GL implementation
// over command buffer to the GPU process.
class ContextProviderCommandBuffer
    : public base::RefCountedThreadSafe<ContextProviderCommandBuffer>,
      public ContextProvider,
      public RasterContextProvider,
      public base::trace_event::MemoryDumpProvider {
 public:
  ContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      int32_t stream_id,
      gpu::SchedulingPriority stream_priority,
      gpu::SurfaceHandle surface_handle,
      const GURL& active_url,
      bool automatic_flushes,
      bool support_locking,
      bool support_grcontext,
      const gpu::SharedMemoryLimits& memory_limits,
      const gpu::ContextCreationAttribs& attributes,
      command_buffer_metrics::ContextType type);

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();
  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;

  gpu::webgpu::WebGPUInterface* WebGPUInterface();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Set the default task runner for command buffers to use for handling IPCs.
  // If not specified, this will be the ThreadTaskRunner for the thread on
  // which BindToThread is called.
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> default_task_runner);

 protected:
  friend class base::RefCountedThreadSafe<ContextProviderCommandBuffer>;
  ~ContextProviderCommandBuffer() override;

  void OnLostContext();

 private:
  void CheckValidThreadOrLockAcquired() const {
#if DCHECK_IS_ON()
    if (support_locking_) {
      context_lock_.AssertAcquired();
    } else {
      DCHECK(context_thread_checker_.CalledOnValidThread());
    }
#endif
  }

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  bool bind_tried_ = false;
  gpu::ContextResult bind_result_;

  const int32_t stream_id_;
  const gpu::SchedulingPriority stream_priority_;
  const gpu::SurfaceHandle surface_handle_;
  const GURL active_url_;
  const bool automatic_flushes_;
  const bool support_locking_;
  const bool support_grcontext_;
  const gpu::SharedMemoryLimits memory_limits_;
  const gpu::ContextCreationAttribs attributes_;
  const command_buffer_metrics::ContextType context_type_;

  scoped_refptr<gpu::GpuChannelHost> channel_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  base::Lock context_lock_;  // Referenced by command_buffer_.
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;
  std::unique_ptr<gpu::CommandBufferHelper> helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;

  // Owned by either gles2_impl_ or raster_interface_, not both.
  gpu::ImplementationBase* impl_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;
  std::unique_ptr<gpu::gles2::GLES2TraceImplementation> trace_impl_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_interface_;
  std::unique_ptr<gpu::webgpu::WebGPUInterface> webgpu_interface_;

  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<ContextCacheController> cache_controller_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;
};

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
