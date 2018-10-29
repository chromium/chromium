// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_GPU_HOST_GPU_HOST_H_
#define SERVICES_WS_GPU_HOST_GPU_HOST_H_

#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/ws/public/mojom/gpu.mojom.h"

#if defined(OS_CHROMEOS)
#include "services/ws/public/mojom/arc.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class SingleThreadTaskRunner;
}

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace gpu {
class ShaderCacheFactory;
}

namespace service_manager {
class Connector;
}

namespace viz {
class GpuClient;
class GpuHostImpl;
class HostGpuMemoryBufferManager;
}

namespace ws {
namespace gpu_host {
class GpuHostDelegate;

// GpuHost sets up connection from clients to the real service implementation in
// the GPU process.
class GpuHost : public viz::GpuHostImpl::Delegate {
 public:
  GpuHost(GpuHostDelegate* delegate,
          service_manager::Connector* connector,
          discardable_memory::DiscardableSharedMemoryManager*
              discardable_shared_memory_manager);
  ~GpuHost() override;

  void CreateFrameSinkManager(viz::mojom::FrameSinkManagerRequest request,
                              viz::mojom::FrameSinkManagerClientPtrInfo client);

  void Shutdown();

  void Add(mojom::GpuRequest request);

#if defined(OS_CHROMEOS)
  void AddArc(mojom::ArcRequest request);
#endif  // defined(OS_CHROMEOS)

 private:
  friend class GpuHostTestApi;

  void OnBadMessageFromGpu();

  // TODO(crbug.com/611505): this goes away after the gpu process split in mus.
  void InitializeVizMain(viz::mojom::VizMainRequest request);
  void DestroyVizMain();

  // viz::GpuHostImpl::Delegate:
  gpu::GPUInfo GetGPUInfo() const override;
  gpu::GpuFeatureInfo GetGpuFeatureInfo() const override;
  void DidInitialize(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const base::Optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu) override;
  void DidFailInitialize() override;
  void DidCreateContextSuccessfully() override;
  void BlockDomainFrom3DAPIs(const GURL& url, gpu::DomainGuilt guilt) override;
  void DisableGpuCompositing() override;
  bool GpuAccessAllowed() const override;
  gpu::ShaderCacheFactory* GetShaderCacheFactory() override;
  void RecordLogMessage(int32_t severity,
                        const std::string& header,
                        const std::string& message) override;
  void BindDiscardableMemoryRequest(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request)
      override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
#if defined(USE_OZONE)
  void TerminateGpuProcess(const std::string& message) override;
  void SendGpuProcessMessage(IPC::Message* message) override;
#endif

  GpuHostDelegate* const delegate_;
  discardable_memory::DiscardableSharedMemoryManager*
      discardable_shared_memory_manager_;
  int32_t next_client_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<viz::GpuHostImpl> gpu_host_impl_;
  gpu::GPUInfo gpu_info_;
  gpu::GpuFeatureInfo gpu_feature_info_;

  std::unique_ptr<viz::HostGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  std::unique_ptr<gpu::ShaderCacheFactory> shader_cache_factory_;

  std::vector<std::unique_ptr<viz::GpuClient>> gpu_clients_;

  // TODO(crbug.com/620927): This should be removed once ozone-mojo is done.
  base::Thread gpu_thread_;
  std::unique_ptr<viz::VizMainImpl> viz_main_impl_;

#if defined(OS_CHROMEOS)
  mojo::StrongBindingSet<mojom::Arc> arc_bindings_;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(GpuHost);
};

}  // namespace gpu_host
}  // namespace ws

#endif  // SERVICES_WS_GPU_HOST_GPU_HOST_H_
