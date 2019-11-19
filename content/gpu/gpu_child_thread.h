// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <vector>

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
#include "gpu/config/gpu_extra_info.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
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
                 ChildThreadImpl::Options options,
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  void CreateVizMainService(
      mojo::PendingAssociatedReceiver<viz::mojom::VizMain> pending_receiver);

  bool in_process_gpu() const;

  // ChildThreadImpl:
  bool Send(IPC::Message* msg) override;
  void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver) override;
  void BindServiceInterface(mojo::GenericPendingReceiver receiver) override;

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

  // Returns a closure which calls into the VizMainImpl to perform shutdown
  // before quitting the main message loop. Must be called on the main thread.
  static base::RepeatingClosure MakeQuitSafelyClosure();
  static void QuitSafelyHelper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

#if defined(OS_ANDROID)
  static std::unique_ptr<media::AndroidOverlay> CreateAndroidOverlay(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::UnguessableToken& routing_token,
      media::AndroidOverlayConfig);
#endif

  viz::VizMainImpl viz_main_;

  // ServiceFactory for service_manager::Service hosting.
  std::unique_ptr<GpuServiceFactory> service_factory_;

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  // A closure which quits the main message loop.
  base::RepeatingClosure quit_closure_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Retains pending GPU-process service startup requests (i.e. RunService
  // invocations from the browser) until the process is fully initialized.
  struct PendingServiceRequest {
    PendingServiceRequest(
        const std::string& service_name,
        mojo::PendingReceiver<service_manager::mojom::Service> receiver);
    PendingServiceRequest(PendingServiceRequest&&);
    ~PendingServiceRequest();

    std::string service_name;
    mojo::PendingReceiver<service_manager::mojom::Service> receiver;
  };
  std::vector<PendingServiceRequest> pending_service_requests_;

  base::WeakPtrFactory<GpuChildThread> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
