// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/isolation_key_provider.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_extra_info.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class DCOMPTexture;
class FenceSyncReleaseDelegate;
class GpuChannelManager;
class GpuChannelMessageFilter;
class GpuMemoryBufferFactory;
class ImageDecodeAcceleratorWorker;
class Scheduler;
class SharedImageStub;
class StreamTexture;
class SyncPointManager;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GPU_IPC_SERVICE_EXPORT GpuChannel : public IPC::Listener,
                                          public IsolationKeyProvider {
 public:
  GpuChannel(const GpuChannel&) = delete;
  GpuChannel& operator=(const GpuChannel&) = delete;
  ~GpuChannel() override;

  static std::unique_ptr<GpuChannel> Create(
      GpuChannelManager* gpu_channel_manager,
      const base::UnguessableToken& channel_token,
      Scheduler* scheduler,
      SyncPointManager* sync_point_manager,
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      int32_t client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host,
      ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
      const gfx::GpuExtraInfo& gpu_extra_info,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  // Init() sets up the underlying IPC channel.  Use a separate method because
  // we don't want to do that in tests.
  void Init(IPC::ChannelHandle channel_handle,
            base::WaitableEvent* shutdown_event);

  void InitForTesting(IPC::Channel* channel);

  base::WeakPtr<GpuChannel> AsWeakPtr();

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  Scheduler* scheduler() const { return scheduler_; }

  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

  void set_client_pid(base::ProcessId pid) { client_pid_ = pid; }
  base::ProcessId client_pid() const { return client_pid_; }

  int client_id() const { return client_id_; }

  uint64_t client_tracing_id() const { return client_tracing_id_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  bool is_gpu_host() const { return is_gpu_host_; }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // gpu::IsolationKeyProvider:
  void GetIsolationKey(const blink::WebGPUExecutionContextToken& token,
                       GetIsolationKeyCallback cb) override;

  void OnCommandBufferScheduled(CommandBufferStub* stub);
  void OnCommandBufferDescheduled(CommandBufferStub* stub);

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  CommandBufferStub* LookupCommandBuffer(int32_t route_id);

  bool HasActiveStatefulContext() const;
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32_t route_id, SequenceId sequence_id);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32_t route_id);

  std::optional<gpu::GpuDiskCacheHandle> GetCacheHandleForType(
      gpu::GpuDiskCacheType type);
  void RegisterCacheHandle(const gpu::GpuDiskCacheHandle& handle);
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& shader);

  uint64_t GetMemoryUsage() const;

  // Executes a DeferredRequest that was previously received and has now been
  // scheduled by the scheduler.
  void ExecuteDeferredRequest(mojom::DeferredRequestParamsPtr params,
                              FenceSyncReleaseDelegate* release_delegate);
  void GetGpuMemoryBufferHandleInfo(
      const gpu::Mailbox& mailbox,
      mojom::GpuChannel::GetGpuMemoryBufferHandleInfoCallback callback);
  void PerformImmediateCleanup();

  void WaitForTokenInRange(
      int32_t routing_id,
      int32_t start,
      int32_t end,
      mojom::GpuChannel::WaitForTokenInRangeCallback callback);
  void WaitForGetOffsetInRange(
      int32_t routing_id,
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end,
      mojom::GpuChannel::WaitForGetOffsetInRangeCallback callback);

  mojom::GpuChannel& GetGpuChannelForTesting();

#if BUILDFLAG(IS_ANDROID)
  const CommandBufferStub* GetOneStub() const;

  bool CreateStreamTexture(
      int32_t stream_id,
      mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver);

  // Called by StreamTexture to remove the GpuChannel's reference to the
  // StreamTexture.
  void DestroyStreamTexture(int32_t stream_id);
#endif

#if BUILDFLAG(IS_WIN)
  bool CreateDCOMPTexture(
      int32_t route_id,
      mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver);

  // Called by DCOMPTexture to remove the GpuChannel's reference to the
  // DCOMPTexture.
  void DestroyDCOMPTexture(int32_t route_id);

  bool RegisterOverlayStateObserver(
      mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
          promotion_hint_observer,
      const gpu::Mailbox& mailbox);
#endif  // BUILDFLAG(IS_WIN)

  SharedImageStub* shared_image_stub() const {
    return shared_image_stub_.get();
  }

  void CreateCommandBuffer(
      mojom::CreateCommandBufferParamsPtr init_params,
      int32_t routing_id,
      base::UnsafeSharedMemoryRegion shared_state_shm,
      mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
      mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
      mojom::GpuChannel::CreateCommandBufferCallback callback);
  void DestroyCommandBuffer(int32_t routing_id);

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(mojo::PlatformHandle service_handle,
                                      mojo::PlatformHandle sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
#endif  // BUILDFLAG(IS_FUCHSIA)

 private:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             const base::UnguessableToken& channel_token,
             Scheduler* scheduler,
             SyncPointManager* sync_point_manager,
             scoped_refptr<gl::GLShareGroup> share_group,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
             int32_t client_id,
             uint64_t client_tracing_id,
             bool is_gpu_host,
             ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
             const gfx::GpuExtraInfo& gpu_extra_info,
             gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  void OnDestroyCommandBuffer(int32_t route_id);

  // Message handlers for control messages.
  bool CreateSharedImageStub(const gfx::GpuExtraInfo& gpu_extra_info);

  std::unique_ptr<IPC::SyncChannel> sync_channel_;  // nullptr in tests.
  raw_ptr<IPC::Sender>
      channel_;  // Same as sync_channel_.get() except in tests.

  base::ProcessId client_pid_ = base::kNullProcessId;

  // Map of routing id to command buffer stub.
  base::flat_map<int32_t, std::unique_ptr<CommandBufferStub>> stubs_;

  // Map of stream id to scheduler sequence id.
  base::flat_map<int32_t, SequenceId> stream_sequences_;

  // Map of disk cache type to the handle.
  base::flat_map<gpu::GpuDiskCacheType, gpu::GpuDiskCacheHandle> caches_;

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  const raw_ptr<GpuChannelManager> gpu_channel_manager_;

  const raw_ptr<Scheduler> scheduler_;

  // Sync point manager. Outlives the channel and is guaranteed to outlive the
  // message loop.
  const raw_ptr<SyncPointManager> sync_point_manager_;

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

  std::unique_ptr<SharedImageStub> shared_image_stub_;

  const bool is_gpu_host_;

#if BUILDFLAG(IS_ANDROID)
  // Set of active StreamTextures.
  base::flat_map<int32_t, scoped_refptr<StreamTexture>> stream_textures_;
#endif

#if BUILDFLAG(IS_WIN)
  // Set of active DCOMPTextures.
  base::flat_map<int32_t, scoped_refptr<DCOMPTexture>> dcomp_textures_;
#endif

  // State shared with the IO thread. Receives all GpuChannel interface messages
  // and schedules tasks for them appropriately.
  const scoped_refptr<GpuChannelMessageFilter> filter_;

  // Member variables should appear before the WeakPtrFactory, to ensure that
  // any WeakPtrs to Controller are invalidated before its members variable's
  // destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_H_
