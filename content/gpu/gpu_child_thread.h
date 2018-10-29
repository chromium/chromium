// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "content/child/child_thread_impl.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class GpuServiceFactory;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to gpu::GpuChannelManager, which is responsible for issuing
// rendering commands to the GPU.
class GpuChildThread : public ChildThreadImpl,
                       public viz::VizMainImpl::Delegate {
 public:
  GpuChildThread(base::RepeatingClosure quit_closure,
                 std::unique_ptr<gpu::GpuInit> gpu_init,
                 viz::VizMainImpl::LogMessages deferred_messages);

  GpuChildThread(const InProcessChildThreadParams& params,
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  ~GpuChildThread() override;

  void Init(const base::Time& process_start_time);

 private:
  GpuChildThread(base::RepeatingClosure quit_closure,
                 const ChildThreadImpl::Options& options,
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  void CreateVizMainService(viz::mojom::VizMainAssociatedRequest request);

  bool in_process_gpu() const;

  // ChildThreadImpl:.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener implementation via ChildThreadImpl:
  void OnAssociatedInterfaceRequest(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // viz::VizMainImpl::Delegate:
  void OnInitializationFailed() override;
  void OnGpuServiceConnection(viz::GpuServiceImpl* gpu_service) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;
  void QuitMainMessageLoop() override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  void BindServiceFactoryRequest(
      service_manager::mojom::ServiceFactoryRequest request);

#if defined(OS_ANDROID)
  static std::unique_ptr<media::AndroidOverlay> CreateAndroidOverlay(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::UnguessableToken& routing_token,
      media::AndroidOverlayConfig);
#endif

  viz::VizMainImpl viz_main_;

  // ServiceFactory for service_manager::Service hosting.
  std::unique_ptr<GpuServiceFactory> service_factory_;

  // Bindings to the service_manager::mojom::ServiceFactory impl.
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  // Holds a closure that releases pending interface requests on the IO thread.
  base::Closure release_pending_requests_closure_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::WeakPtrFactory<GpuChildThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
