// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_router.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

struct GPUCreateCommandBufferConfig;

namespace base {
class WaitableEvent;
}

namespace gpu {
class GpuChannelManager;
class GpuChannelMessageFilter;
class ImageDecodeAcceleratorStub;
class ImageDecodeAcceleratorWorker;
class Scheduler;
class SharedImageStub;
class StreamTexture;
class SyncPointManager;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GPU_IPC_SERVICE_EXPORT GpuChannel : public IPC::Listener,
                                          public IPC::Sender {
 public:
  ~GpuChannel() override;

  static std::unique_ptr<GpuChannel> Create(
      GpuChannelManager* gpu_channel_manager,
      Scheduler* scheduler,
      SyncPointManager* sync_point_manager,
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      int32_t client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host,
      ImageDecodeAcceleratorWorker* image_decode_accelerator_worker);

  // Init() sets up the underlying IPC channel.  Use a separate method because
  // we don't want to do that in tests.
  void Init(IPC::ChannelHandle channel_handle,
            base::WaitableEvent* shutdown_event);

  void InitForTesting(IPC::Channel* channel);

  base::WeakPtr<GpuChannel> AsWeakPtr();

  void SetUnhandledMessageListener(IPC::Listener* listener);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  Scheduler* scheduler() const { return scheduler_; }

  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  gles2::ImageManager* image_manager() const { return image_manager_.get(); }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

  base::ProcessId GetClientPID() const;
  bool IsConnected() const;

  int client_id() const { return client_id_; }

  uint64_t client_tracing_id() const { return client_tracing_id_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  bool is_gpu_host() const { return is_gpu_host_; }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  void AddFilter(IPC::MessageFilter* filter);
  void RemoveFilter(IPC::MessageFilter* filter);

  void OnCommandBufferScheduled(CommandBufferStub* stub);
  void OnCommandBufferDescheduled(CommandBufferStub* stub);

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  CommandBufferStub* LookupCommandBuffer(int32_t route_id);

  bool HasActiveWebGLContext() const;
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32_t route_id,
                SequenceId sequence_id,
                IPC::Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32_t route_id);

  void CacheShader(const std::string& key, const std::string& shader);

  uint64_t GetMemoryUsage() const;

  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      SurfaceHandle surface_handle);

  void HandleMessage(const IPC::Message& msg);

  // Some messages such as WaitForGetOffsetInRange and WaitForTokenInRange are
  // processed as soon as possible because the client is blocked until they
  // are completed.
  void HandleOutOfOrderMessage(const IPC::Message& msg);

  void HandleMessageForTesting(const IPC::Message& msg);

  ImageDecodeAcceleratorStub* GetImageDecodeAcceleratorStub() const;

#if defined(OS_ANDROID)
  const CommandBufferStub* GetOneStub() const;

  // Called by StreamTexture to remove the GpuChannel's reference to the
  // StreamTexture.
  void DestroyStreamTexture(int32_t stream_id);
#endif

  SharedImageStub* shared_image_stub() const {
    return shared_image_stub_.get();
  }

 private:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             Scheduler* scheduler,
             SyncPointManager* sync_point_manager,
             scoped_refptr<gl::GLShareGroup> share_group,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
             int32_t client_id,
             uint64_t client_tracing_id,
             bool is_gpu_host,
             ImageDecodeAcceleratorWorker* image_decode_accelerator_worker);

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessageHelper(const IPC::Message& msg);

  // Message handlers for control messages.
  void OnCreateCommandBuffer(const GPUCreateCommandBufferConfig& init_params,
                             int32_t route_id,
                             base::UnsafeSharedMemoryRegion shared_state_shm,
                             gpu::ContextResult* result,
                             gpu::Capabilities* capabilities);
  void OnDestroyCommandBuffer(int32_t route_id);
  void OnCreateStreamTexture(int32_t stream_id, bool* succeeded);
  bool CreateSharedImageStub();

  std::unique_ptr<IPC::SyncChannel> sync_channel_;  // nullptr in tests.
  IPC::Sender* channel_;  // Same as sync_channel_.get() except in tests.

  base::ProcessId peer_pid_ = base::kNullProcessId;

  // The message filter on the io thread.
  scoped_refptr<GpuChannelMessageFilter> filter_;

  // Map of routing id to command buffer stub.
  base::flat_map<int32_t, std::unique_ptr<CommandBufferStub>> stubs_;

  // Map of stream id to scheduler sequence id.
  base::flat_map<int32_t, SequenceId> stream_sequences_;

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* const gpu_channel_manager_;

  Scheduler* const scheduler_;

  // Sync point manager. Outlives the channel and is guaranteed to outlive the
  // message loop.
  SyncPointManager* const sync_point_manager_;

  IPC::Listener* unhandled_message_listener_ = nullptr;

  // Used to implement message routing functionality to CommandBuffer objects
  IPC::MessageRouter router_;

  // The id of the client who is on the other side of the channel.
  const int32_t client_id_;

  // The tracing ID used for memory allocations associated with this client.
  const uint64_t client_tracing_id_;

  // The task runners for the main thread and the io thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gl::GLShareGroup> share_group_;

  std::unique_ptr<gles2::ImageManager> image_manager_;
  std::unique_ptr<SharedImageStub> shared_image_stub_;

  const bool is_gpu_host_;

#if defined(OS_ANDROID)
  // Set of active StreamTextures.
  base::flat_map<int32_t, scoped_refptr<StreamTexture>> stream_textures_;
#endif

  // Member variables should appear before the WeakPtrFactory, to ensure that
  // any WeakPtrs to Controller are invalidated before its members variable's
  // destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_H_
