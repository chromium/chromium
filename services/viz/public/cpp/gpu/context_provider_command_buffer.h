// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/types/pass_key.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "skia/buildflags.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
class CommandBufferHelper;
class CommandBufferProxyImpl;
class SharedImageInterface;
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

namespace viz {

enum class WebGLContextType { kWebGL1, kWebGL2 };

// Implementation of ContextProvider that provides a GL implementation
// over command buffer to the GPU process.
class ContextProviderCommandBuffer
    : public base::subtle::RefCountedThreadSafeBase,
      public ContextProvider,
      public RasterContextProvider,
      public base::trace_event::MemoryDumpProvider {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  static scoped_refptr<ContextProviderCommandBuffer> CreateForGL(
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t stream_id,
      gpu::SchedulingPriority stream_priority,
      const GURL& active_url,
      command_buffer_metrics::ContextType type,
      bool lose_context_when_out_of_memory = false);

  static scoped_refptr<ContextProviderCommandBuffer> CreateForWebGL(
      scoped_refptr<gpu::GpuChannelHost> channel,
      const GURL& active_url,
      WebGLContextType context_type,
      bool prefer_low_power_gpu,
      bool fail_if_major_performance_caveat);

  static scoped_refptr<ContextProviderCommandBuffer> CreateForRaster(
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t stream_id,
      gpu::SchedulingPriority stream_priority,
      const GURL& active_url,
      bool automatic_flushes,
      bool support_locking,
      const gpu::SharedMemoryLimits& memory_limits,
      command_buffer_metrics::ContextType type,
      bool enable_gpu_rasterization,
      bool lose_context_when_out_of_memory);

  static scoped_refptr<ContextProviderCommandBuffer> CreateForWebGPU(
      scoped_refptr<gpu::GpuChannelHost> channel,
      const GURL& active_url,
      command_buffer_metrics::ContextType type,
      base::SharedMemoryMapper* buffer_mapper);

  ContextProviderCommandBuffer(
      base::PassKey<ContextProviderCommandBuffer> pass_key,
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t stream_id,
      gpu::SchedulingPriority stream_priority,
      const GURL& active_url,
      bool automatic_flushes,
      bool support_locking,
      const gpu::SharedMemoryLimits& memory_limits,
      gpu::mojom::ContextCreationAttribsPtr attributes,
      command_buffer_metrics::ContextType type,
      base::SharedMemoryMapper* buffer_mapper = nullptr);

  // Virtual for testing.
  virtual gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

  // ContextProvider / RasterContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentSequence() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::raster::RasterInterface* RasterInterface() override;
  gpu::ContextSupport* ContextSupport() override;
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
  friend class base::DeleteHelper<ContextProviderCommandBuffer>;
  ~ContextProviderCommandBuffer() override;

  // Used by MockContextProviderCommandBuffer in
  // media/mojo/clients/mojo_gpu_video_accelerator_factories_unittest.cc
  explicit ContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel);

 private:
  void OnLostContext();

  void CheckValidSequenceOrLockAcquired() const {
    if (support_locking_) {
      context_lock_.AssertAcquired();
    } else {
      DCHECK_CALLED_ON_VALID_SEQUENCE(context_sequence_checker_);
    }
  }

  SEQUENCE_CHECKER(context_sequence_checker_);

  bool bind_tried_ = false;
  gpu::ContextResult bind_result_;

  const int32_t stream_id_;
  const gpu::SchedulingPriority stream_priority_;
  const GURL active_url_;
  const bool automatic_flushes_;
  const bool support_locking_;
  const gpu::SharedMemoryLimits memory_limits_;
  const gpu::mojom::ContextCreationAttribsPtr attributes_;
  const command_buffer_metrics::ContextType context_type_;

  scoped_refptr<gpu::GpuChannelHost> channel_;
  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;

  // |shared_image_interface_| must be torn down after |command_buffer_| to
  // ensure any dependent commands in the command stream are flushed before the
  // associated shared images are destroyed.
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;

  //////////////////////////////////////////////////////////////////////////////
  // IMPORTANT NOTE: All of the objects in this block are part of a complex   //
  // graph of raw pointers (holder or pointee of various raw_ptrs). They are  //
  // defined in topological order: only later items point to earlier items.   //
  // - When writing any member, always ensure its pointers to earlier members
  //   are guaranteed to stay alive.
  // - When clearing OR overwriting any member, always ensure objects that
  //   point to it have already been cleared.
  //     - The topological order of definitions guarantees that the
  //       destructors will be called in the correct order (bottom to top).
  //     - When overwriting multiple members, similarly do so in reverse order.
  //
  // Please note these comments are likely not to stay perfectly up-to-date.

  base::Lock context_lock_;
  // Points to the context_lock_ field of `this`.
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;

  // Points to command_buffer_.
  std::unique_ptr<gpu::CommandBufferHelper> helper_;
  // Points to helper_.
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;

  // Points to transfer_buffer_, helper_, and command_buffer_.
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;
  // Points to gles2_impl_.
  std::unique_ptr<gpu::gles2::GLES2TraceImplementation> trace_impl_;
  // Points to transfer_buffer_, helper_, and command_buffer_.
  std::unique_ptr<gpu::raster::RasterInterface> raster_interface_;
  // Points to transfer_buffer_, helper_, and command_buffer_.
  std::unique_ptr<gpu::webgpu::WebGPUInterface> webgpu_interface_;
  // This is an alias for gles2_impl_, raster_interface_, or webgpu_interface_.
  raw_ptr<gpu::ImplementationBase> impl_ = nullptr;

  // END IMPORTANT NOTE                                                       //
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ContextCacheController> cache_controller_;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  // Shared memory mapper used by command buffer proxies created from this
  // provider when creating shared memory mappings.
  // TODO(crbug.com/40837434) remove this member again once users of the command
  // buffer proxy can specify the mapper for each mapping individually.
  raw_ptr<base::SharedMemoryMapper> buffer_mapper_ = nullptr;
};

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
