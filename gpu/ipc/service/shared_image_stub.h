// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
#define GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_

#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ipc/ipc_listener.h"

namespace gpu {
class SharedContextState;
struct Mailbox;
class GpuChannel;
class SharedImageFactory;

class GPU_IPC_SERVICE_EXPORT SharedImageStub
    : public IPC::Listener,
      public MemoryTracker,
      public base::trace_event::MemoryDumpProvider {
 public:
  ~SharedImageStub() override;

  using SharedImageDestructionCallback =
      base::OnceCallback<void(const gpu::SyncToken&)>;

  static std::unique_ptr<SharedImageStub> Create(GpuChannel* channel,
                                                 int32_t route_id);

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

  // MemoryTracker implementation:
  void TrackMemoryAllocatedChange(uint64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  SequenceId sequence() const { return sequence_; }
  SharedImageFactory* factory() const { return factory_.get(); }
  GpuChannel* channel() const { return channel_; }

  SharedImageDestructionCallback GetSharedImageDestructionCallback(
      const Mailbox& mailbox);

  bool CreateSharedImage(const Mailbox& mailbox,
                         int client_id,
                         gfx::GpuMemoryBufferHandle handle,
                         gfx::BufferFormat format,
                         SurfaceHandle surface_handle,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage);
  bool UpdateSharedImage(const Mailbox& mailbox,
                         const gfx::GpuFenceHandle& in_fence_handle);

 private:
  SharedImageStub(GpuChannel* channel, int32_t route_id);

  void OnCreateSharedImage(
      const GpuChannelMsg_CreateSharedImage_Params& params);
  void OnCreateSharedImageWithData(
      const GpuChannelMsg_CreateSharedImageWithData_Params& params);
  void OnCreateGMBSharedImage(GpuChannelMsg_CreateGMBSharedImage_Params params);
  void OnUpdateSharedImage(const Mailbox& mailbox,
                           uint32_t release_id,
                           const gfx::GpuFenceHandle& in_fence_handle);
  void OnDestroySharedImage(const Mailbox& mailbox);
  void OnRegisterSharedImageUploadBuffer(base::ReadOnlySharedMemoryRegion shm);
#if defined(OS_WIN)
  void OnCreateSwapChain(const GpuChannelMsg_CreateSwapChain_Params& params);
  void OnPresentSwapChain(const Mailbox& mailbox, uint32_t release_id);
#endif  // OS_WIN
#if defined(OS_FUCHSIA)
  void OnRegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                        zx::channel token);
  void OnReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id);
#endif  // OS_FUCHSIA

  bool MakeContextCurrent();
  ContextResult MakeContextCurrentAndCreateFactory();
  void OnError();

  // Wait on the sync token if any and destroy the shared image.
  void DestroySharedImage(const Mailbox& mailbox, const SyncToken& sync_token);

  GpuChannel* channel_;

  // While this is not a CommandBuffer, this provides a unique identifier for
  // a SharedImageStub, comprised of identifiers which it was already using.
  // TODO(jonross): Look into a rename of CommandBufferId to reflect that it can
  // be a unique identifier for numerous gpu constructs.
  CommandBufferId command_buffer_id_;
  SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  uint64_t size_ = 0;
  // Holds shared memory used in initial data uploads.
  base::ReadOnlySharedMemoryRegion upload_memory_;
  base::ReadOnlySharedMemoryMapping upload_memory_mapping_;

  base::WeakPtrFactory<SharedImageStub> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
