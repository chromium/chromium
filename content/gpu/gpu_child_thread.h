// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
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
#include "ui/gfx/gpu_extra_info.h"
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
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  GpuChildThread(const InProcessChildThreadParams& params,
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  GpuChildThread(const GpuChildThread&) = delete;
  GpuChildThread& operator=(const GpuChildThread&) = delete;

  ~GpuChildThread() override;

  void Init(const base::TimeTicks& process_start_time);

 private:
  GpuChildThread(base::RepeatingClosure quit_closure,
                 ChildThreadImpl::Options options,
                 std::unique_ptr<gpu::GpuInit> gpu_init);

  bool in_process_gpu() const;

  // ChildThreadImpl:
  void BindServiceInterface(mojo::GenericPendingReceiver receiver) override;

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

#if BUILDFLAG(IS_ANDROID)
  static std::unique_ptr<media::AndroidOverlay> CreateAndroidOverlay(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::UnguessableToken& routing_token,
      media::AndroidOverlayConfig);
#endif

  viz::VizMainImpl viz_main_;

  // ServiceFactory for Mojo service hosting.
  std::unique_ptr<GpuServiceFactory> service_factory_;

  // A queue of incoming service interface requests received prior to
  // |service_factory_| initialization.
  std::vector<mojo::GenericPendingReceiver> pending_service_receivers_;

  // A closure which quits the main message loop.
  base::RepeatingClosure quit_closure_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::WeakPtrFactory<GpuChildThread> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
