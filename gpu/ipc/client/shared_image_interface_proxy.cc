// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/shared_image_interface_proxy.h"

#include "base/bits.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {
namespace {

bool SafeIncrementAndAlign(size_t aligned_value,
                           size_t increment,
                           size_t alignment,
                           size_t* result) {
  base::CheckedNumeric<size_t> sum = aligned_value;
  sum += increment;
  // Taken from base::bits::Align.
  // TODO(ericrk): Update base::bits::Align to handle CheckedNumeric.
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  sum = (sum + alignment - 1) & ~(alignment - 1);
  return sum.AssignIfValid(result);
}

size_t GetRemainingSize(const base::MappedReadOnlyRegion& region,
                        size_t offset) {
  if (offset > region.mapping.size())
    return 0;
  return region.mapping.size() - offset;
}

void* GetDataAddress(const base::MappedReadOnlyRegion& region,
                     size_t offset,
                     size_t size) {
  base::CheckedNumeric<size_t> safe_end = offset;
  safe_end += size;
  size_t end;
  if (!safe_end.AssignIfValid(&end) || end > region.mapping.size())
    return nullptr;
  return region.mapping.GetMemoryAs<uint8_t>() + offset;
}

}  // namespace

SharedImageInterfaceProxy::SharedImageInterfaceProxy(GpuChannelHost* host,
                                                     int32_t route_id)
    : host_(host), route_id_(route_id) {}

SharedImageInterfaceProxy::~SharedImageInterfaceProxy() = default;

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  GpuChannelMsg_CreateSharedImage_Params params;
  params.mailbox = Mailbox::GenerateForSharedImage();
  params.format = format;
  params.size = size;
  params.color_space = color_space;
  params.usage = usage;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_CreateSharedImage(route_id_, params));
  }
  return params.mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  // Pixel data's size must fit into a uint32_t to be sent via
  // GpuChannelMsg_CreateSharedImageWithData_Params.
  if (!base::IsValueInRangeForNumericType<uint32_t>(pixel_data.size())) {
    LOG(ERROR)
        << "CreateSharedImage: SharedImage upload data overflows uint32_t";
    return Mailbox();
  }

  // Hold the lock for the rest of this function, as we need to ensure that SHM
  // reallocation / registration and the following use of that SHM via deferred
  // message are not interrupted by a SHM allocation on another thread.
  base::AutoLock lock(lock_);

  bool done_with_shm;
  size_t shm_offset;
  if (!GetSHMForPixelData(pixel_data, &shm_offset, &done_with_shm)) {
    LOG(ERROR) << "CreateSharedImage: Could not get SHM for data upload.";
    return Mailbox();
  }

  GpuChannelMsg_CreateSharedImageWithData_Params params;
  params.mailbox = Mailbox::GenerateForSharedImage();
  params.format = format;
  params.size = size;
  params.color_space = color_space;
  params.usage = usage;
  params.pixel_data_offset = shm_offset;
  params.pixel_data_size = pixel_data.size();
  params.done_with_shm = done_with_shm;
  params.release_id = ++next_release_id_;
  last_flush_id_ = host_->EnqueueDeferredMessage(
      GpuChannelMsg_CreateSharedImageWithData(route_id_, params));
  return params.mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(gpu_memory_buffer->GetType() == gfx::NATIVE_PIXMAP ||
         gpu_memory_buffer->GetType() == gfx::ANDROID_HARDWARE_BUFFER ||
         gpu_memory_buffer_manager);

  auto mailbox = Mailbox::GenerateForSharedImage();

  GpuChannelMsg_CreateGMBSharedImage_Params params;
  params.mailbox = mailbox;
  params.handle = gpu_memory_buffer->CloneHandle();
  params.size = gpu_memory_buffer->GetSize();
  params.format = gpu_memory_buffer->GetFormat();
  params.color_space = color_space;
  params.usage = usage;

  // TODO(piman): DCHECK GMB format support.
  DCHECK(gpu::IsImageSizeValidForGpuMemoryBufferFormat(params.size,
                                                       params.format));

  bool requires_sync_token = params.handle.type == gfx::IO_SURFACE_BUFFER;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    // Note: we send the IPC under the lock, after flushing previous work (if
    // any) to guarantee monotonicity of the release ids as seen by the service.
    // Although we don't strictly need to for correctness, we also flush
    // DestroySharedImage messages, so that we get a chance to delete resources
    // before creating new ones.
    // TODO(piman): support messages with handles in EnqueueDeferredMessage.
    host_->EnsureFlush(last_flush_id_);
    host_->Send(
        new GpuChannelMsg_CreateGMBSharedImage(route_id_, std::move(params)));
  }
  if (requires_sync_token) {
    gpu::SyncToken sync_token = GenVerifiedSyncToken();

    gpu_memory_buffer_manager->SetDestructionSyncToken(gpu_memory_buffer,
                                                       sync_token);
  }
  return mailbox;
}

void SharedImageInterfaceProxy::UpdateSharedImage(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, nullptr, mailbox);
}

void SharedImageInterfaceProxy::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    DCHECK(!acquire_fence);

    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host_->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  {
    base::AutoLock lock(lock_);
    gfx::GpuFenceHandle acquire_fence_handle;
    if (acquire_fence) {
      acquire_fence_handle =
          gfx::CloneHandleForIPC(acquire_fence->GetGpuFenceHandle());
      // TODO(dcastagna): This message will be wrapped, handles can't be passed
      // in inner messages. Use EnqueueDeferredMessage if it will be possible to
      // have handles in inner messages in the future.
      host_->EnsureFlush(last_flush_id_);
      host_->Send(new GpuChannelMsg_UpdateSharedImage(
          route_id_, mailbox, ++next_release_id_, acquire_fence_handle));
      return;
    }
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_UpdateSharedImage(route_id_, mailbox, ++next_release_id_,
                                        acquire_fence_handle),
        std::move(dependencies));
  }
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host_->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_DestroySharedImage(route_id_, mailbox),
        std::move(dependencies));
  }
}

SyncToken SharedImageInterfaceProxy::GenVerifiedSyncToken() {
  SyncToken sync_token = GenUnverifiedSyncToken();
  // Force a synchronous IPC to validate sync token.
  host_->VerifyFlush(UINT32_MAX);
  sync_token.SetVerifyFlush();
  return sync_token;
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

void SharedImageInterfaceProxy::Flush() {
  base::AutoLock lock(lock_);
  host_->EnsureFlush(last_flush_id_);
}

bool SharedImageInterfaceProxy::GetSHMForPixelData(
    base::span<const uint8_t> pixel_data,
    size_t* shm_offset,
    bool* done_with_shm) {
  const size_t kUploadBufferSize = 1 * 1024 * 1024;  // 1MB
  *shm_offset = 0;
  *done_with_shm = false;

  lock_.AssertAcquired();
  if (!upload_buffer_.IsValid() ||
      GetRemainingSize(upload_buffer_, upload_buffer_offset_) <
          pixel_data.size()) {
    size_t size_to_alloc = std::max(kUploadBufferSize, pixel_data.size());
    auto shm = mojo::CreateReadOnlySharedMemoryRegion(size_to_alloc);
    if (!shm.IsValid())
      return false;

    // Duplicate the buffer for sharing to the GPU process.
    base::ReadOnlySharedMemoryRegion shared_shm = shm.region.Duplicate();
    if (!shared_shm.IsValid())
      return false;

    // Share the SHM to the GPU process. In order to ensure that any deferred
    // messages which rely on the previous SHM have a chance to execute before
    // it is replaced, flush before sending.
    host_->EnsureFlush(last_flush_id_);
    host_->Send(new GpuChannelMsg_RegisterSharedImageUploadBuffer(
        route_id_, std::move(shared_shm)));

    upload_buffer_ = std::move(shm);
    upload_buffer_offset_ = 0;
  }

  // We now have an |upload_buffer_| that fits our data.

  void* target =
      GetDataAddress(upload_buffer_, upload_buffer_offset_, pixel_data.size());
  DCHECK(target);
  memcpy(target, pixel_data.data(), pixel_data.size());
  *shm_offset = upload_buffer_offset_;

  // Now that we've successfully used up a portion of our buffer, increase our
  // |upload_buffer_offset_|. If our |upload_buffer_offset_| is at the end (or
  // past the end with rounding), we discard the current buffer. We'll allocate
  // a new buffer the next time we enter this function.
  bool discard_buffer = false;
  if (SafeIncrementAndAlign(upload_buffer_offset_, pixel_data.size(),
                            4 /* alignment */, &upload_buffer_offset_)) {
    discard_buffer =
        GetRemainingSize(upload_buffer_, upload_buffer_offset_) == 0;
  } else {
    discard_buffer = true;
  }

  if (discard_buffer) {
    *done_with_shm = true;
    upload_buffer_ = base::MappedReadOnlyRegion();
    upload_buffer_offset_ = 0;
  }

  return true;
}

SharedImageInterface::SwapChainMailboxes
SharedImageInterfaceProxy::CreateSwapChain(viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           uint32_t usage) {
#if defined(OS_WIN)
  GpuChannelMsg_CreateSwapChain_Params params;
  params.front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  params.back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  params.format = format;
  params.size = size;
  params.color_space = color_space;
  params.usage = usage;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_CreateSwapChain(route_id_, params));
  }
  return {params.front_buffer_mailbox, params.back_buffer_mailbox};
#else
  NOTREACHED();
  return {};
#endif  // OS_WIN
}

void SharedImageInterfaceProxy::PresentSwapChain(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
#if defined(OS_WIN)
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host_->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  {
    base::AutoLock lock(lock_);
    uint32_t release_id = ++next_release_id_;
    last_flush_id_ = host_->EnqueueDeferredMessage(
        GpuChannelMsg_PresentSwapChain(route_id_, mailbox, release_id),
        std::move(dependencies));
    host_->EnsureFlush(last_flush_id_);
  }
#else
  NOTREACHED();
#endif  // OS_WIN
}

#if defined(OS_FUCHSIA)
void SharedImageInterfaceProxy::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token) {
  host_->Send(
      new GpuChannelMsg_RegisterSysmemBufferCollection(route_id_, id, token));
}

void SharedImageInterfaceProxy::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  host_->Send(new GpuChannelMsg_ReleaseSysmemBufferCollection(route_id_, id));
}
#endif  // defined(OS_FUCHSIA)

}  // namespace gpu
