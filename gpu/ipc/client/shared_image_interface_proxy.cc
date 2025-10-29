// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/shared_image_interface_proxy.h"

#include <bit>

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_fence.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

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
  DCHECK(std::has_single_bit(alignment));
  sum = (sum + alignment - 1) & ~(alignment - 1);
  return sum.AssignIfValid(result);
}

size_t GetRemainingSize(const base::MappedReadOnlyRegion& region,
                        size_t offset) {
  if (offset > region.mapping.size())
    return 0;
  return region.mapping.size() - offset;
}

base::span<uint8_t> GetTargetData(base::MappedReadOnlyRegion& region,
                                  size_t offset,
                                  size_t size) {
  base::CheckedNumeric<size_t> safe_end = offset;
  safe_end += size;
  size_t end;
  if (!safe_end.AssignIfValid(&end) || end > region.mapping.size()) {
    return {};
  }
  return base::span(region.mapping).subspan(offset, size);
}

std::vector<SyncToken> GenerateDependenciesFromSyncToken(
    SyncToken sync_token,
    GpuChannelHost* const host) {
  DCHECK(host);
  std::vector<SyncToken> dependencies;
  if (sync_token.HasData()) {
    dependencies.push_back(sync_token);
    SyncToken& new_token = dependencies.back();
    if (!new_token.verified_flush()) {
      // Only allow unverified sync tokens for the same channel.
      DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
      int sync_token_channel_id =
          ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
      DCHECK_EQ(sync_token_channel_id, host->channel_id());
      new_token.SetVerifyFlush();
    }
  }
  return dependencies;
}

mojom::SharedImageInfoPtr CreateSharedImageInfo(
    const SharedImageInfo& si_info) {
  auto info = mojom::SharedImageInfo::New();
  info->meta = si_info.meta;
  info->debug_label = si_info.debug_label;
  return info;
}

}  // namespace

SharedImageInterfaceProxy::SharedImageInterfaceProxy(
    GpuChannelHost* host,
    int32_t route_id,
    const gpu::SharedImageCapabilities& capabilities)
    : host_(host), route_id_(route_id), capabilities_(capabilities) {}

SharedImageInterfaceProxy::~SharedImageInterfaceProxy() = default;

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    const SharedImageInfo& si_info,
    std::optional<SharedImagePoolId> pool_id) {
  auto mailbox = Mailbox::Generate();
  auto params = mojom::CreateSharedImageParams::New();
  params->mailbox = mailbox;
  params->si_info = CreateSharedImageInfo(si_info);
  params->pool_id = std::move(pool_id);
  {
    base::AutoLock lock(lock_);
    AddMailbox(mailbox);
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSharedImage(
                std::move(params))),
        /*sync_token_fences=*/{}, ++next_release_id_);
  }

  return mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    SharedImageInfo& si_info,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id,
    gfx::GpuMemoryBufferHandle* handle_to_populate) {
  // Create a GMB here first on IO thread via sync IPC. Then create a mailbox
  // from it.
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    host_->CreateGpuMemoryBuffer(si_info.meta.size, si_info.meta.format,
                                 buffer_usage, handle_to_populate);
  }

  if (handle_to_populate->is_null()) {
    if (!host_->IsLost()) {
      LOG(ERROR) << "Buffer handle is null. Not creating a mailbox from it.";
    }
    return Mailbox();
  }

  // Clear the external sampler prefs for shared memory case if it is set. Note
  // that the |si_info.meta.format| is a reference, so any modifications to it
  // will also be reflected at the place from which this method is called from.
  // https://issues.chromium.org/339546249.
  if (si_info.meta.format.PrefersExternalSampler() &&
      (handle_to_populate->type ==
       gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER)) {
    si_info.meta.format.ClearPrefersExternalSampler();
  }

  // Call existing SI method to create a SI from handle. Note that we are doing
  // 2 IPCs here in this call. 1 to create a GMB above and then another to
  // create a SI from it.
  // TODO(crbug.com/40283107) : This can be optimize to just 1 IPC. Instead of
  // sending a deferred IPC from SIIProxy after receiving the handle,
  // GpuChannelMessageFilter::CreateGpuMemoryBuffer() call in service side can
  // itself can post a task from IO thread to gpu main thread to create a
  // mailbox from handle and then return the handle back to SIIProxy.
  return CreateSharedImage(si_info, handle_to_populate->Clone(),
                           std::move(pool_id));
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    const SharedImageInfo& si_info,
    base::span<const uint8_t> pixel_data) {
  // Pixel data's size must fit into a uint32_t to be sent in
  // CreateSharedImageWithDataParams.
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

  auto mailbox = Mailbox::Generate();
  auto params = mojom::CreateSharedImageWithDataParams::New();
  params->mailbox = mailbox;
  params->si_info = CreateSharedImageInfo(si_info);
  params->pixel_data_offset = shm_offset;
  params->pixel_data_size = pixel_data.size();
  params->done_with_shm = done_with_shm;
  last_flush_id_ = host_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewSharedImageRequest(
          mojom::DeferredSharedImageRequest::NewCreateSharedImageWithData(
              std::move(params))),
      /*sync_token_fences=*/{}, ++next_release_id_);
  AddMailbox(mailbox);
  return mailbox;
}

Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    const SharedImageInfo& si_info,
    gfx::GpuMemoryBufferHandle buffer_handle,
    std::optional<SharedImagePoolId> pool_id) {
  // TODO(kylechar): Verify buffer_handle works for size+format.
  auto mailbox = Mailbox::Generate();

  auto params = mojom::CreateSharedImageWithBufferParams::New();
  params->mailbox = mailbox;
  params->si_info = CreateSharedImageInfo(si_info);
  params->buffer_handle = std::move(buffer_handle);
  params->pool_id = std::move(pool_id);

  base::AutoLock lock(lock_);
  // Note: we enqueue and send the IPC under the lock to guarantee
  // monotonicity of the release ids as seen by the service.
  last_flush_id_ = host_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewSharedImageRequest(
          mojom::DeferredSharedImageRequest::NewCreateSharedImageWithBuffer(
              std::move(params))),
      /*sync_token_fences=*/{}, ++next_release_id_);
  host_->EnsureFlush(last_flush_id_);

  AddMailbox(mailbox);
  return mailbox;
}

void SharedImageInterfaceProxy::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCopyToGpuMemoryBuffer(
                mojom::CopyToGpuMemoryBufferParams::New(mailbox))),
        std::move(dependencies), ++next_release_id_);
  }
}

#if BUILDFLAG(IS_WIN)
void SharedImageInterfaceProxy::CopyToGpuMemoryBufferAsync(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  base::AutoLock lock(lock_);
  host_->CopyToGpuMemoryBufferAsync(
      mailbox, GenerateDependenciesFromSyncToken(std::move(sync_token), host_),
      ++next_release_id_, std::move(callback));
}

void SharedImageInterfaceProxy::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  base::AutoLock lock(lock_);

  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  // Register fence in gpu process in first update.
  auto [token_it, inserted] =
      registered_fence_tokens_.insert(d3d_shared_fence->GetDXGIHandleToken());
  if (inserted) {
    gfx::GpuFenceHandle fence_handle;
    fence_handle.Adopt(d3d_shared_fence->CloneSharedHandle());

    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewRegisterDxgiFence(
                mojom::RegisterDxgiFenceParams::New(
                    mailbox, d3d_shared_fence->GetDXGIHandleToken(),
                    std::move(fence_handle)))),
        std::move(dependencies), /*release_count=*/0);
  }

  last_flush_id_ = host_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewSharedImageRequest(
          mojom::DeferredSharedImageRequest::NewUpdateDxgiFence(
              mojom::UpdateDxgiFenceParams::New(
                  mailbox, d3d_shared_fence->GetDXGIHandleToken(),
                  d3d_shared_fence->GetFenceValue()))),
      std::move(dependencies), /*release_count=*/0);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
void SharedImageInterfaceProxy::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  host_->CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region), std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

void SharedImageInterfaceProxy::UpdateSharedImage(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  UpdateSharedImage(sync_token, std::unique_ptr<gfx::GpuFence>(), mailbox);
}

void SharedImageInterfaceProxy::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  // If there is a valid SyncToken, there should not be any GpuFence.
  if (sync_token.HasData())
    DCHECK(!acquire_fence);
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);

    gfx::GpuFenceHandle acquire_fence_handle;
    if (acquire_fence)
      acquire_fence_handle = acquire_fence->GetGpuFenceHandle().Clone();
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewUpdateSharedImage(
                mojom::UpdateSharedImageParams::New(
                    mailbox, std::move(acquire_fence_handle)))),
        std::move(dependencies), ++next_release_id_);
  }
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);

    auto it = mailbox_infos_.find(mailbox);
    CHECK(it != mailbox_infos_.end());
    auto& info = it->second;

    CHECK_GT(info.ref_count, 0);
    if (--info.ref_count == 0) {
      info.destruction_sync_tokens.insert(info.destruction_sync_tokens.end(),
                                          dependencies.begin(),
                                          dependencies.end());

      last_flush_id_ = host_->EnqueueDeferredMessage(
          mojom::DeferredRequestParams::NewSharedImageRequest(
              mojom::DeferredSharedImageRequest::NewDestroySharedImage(
                  mailbox)),
          std::move(info.destruction_sync_tokens), /*release_count=*/0);

      host_->DelayedEnsureFlush(last_flush_id_);
      mailbox_infos_.erase(it);
    } else if (!dependencies.empty()) {
      constexpr size_t kMaxSyncTokens = 4;
      // Avoid accumulating too many SyncTokens in case where client
      // continiously adds and removes refs, but never reaches the zero. This
      // will ensure that all subsequent calls (including DestroySharedImage)
      // are happening after sync tokens are released.
      // We flush only old SyncTokens here, as they are more likely to pass
      // already, to reduce potential stalls.
      if (info.destruction_sync_tokens.size() > kMaxSyncTokens) {
        last_flush_id_ = host_->EnqueueDeferredMessage(
            mojom::DeferredRequestParams::NewSharedImageRequest(
                mojom::DeferredSharedImageRequest::NewNop(0)),
            std::move(info.destruction_sync_tokens), /*release_count=*/0);

        info.destruction_sync_tokens.clear();
      }

      info.destruction_sync_tokens.insert(info.destruction_sync_tokens.end(),
                                          dependencies.begin(),
                                          dependencies.end());
    }
  }
}

SyncToken SharedImageInterfaceProxy::GenVerifiedSyncToken() {
  SyncToken sync_token = GenUnverifiedSyncToken();
  VerifySyncToken(sync_token);
  return sync_token;
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

void SharedImageInterfaceProxy::VerifySyncToken(SyncToken& sync_token) {
  // Force a synchronous IPC to validate sync token.
  host_->VerifyFlush(UINT32_MAX);
  sync_token.SetVerifyFlush();
}

bool SharedImageInterfaceProxy::CanVerifySyncToken(
    const gpu::SyncToken& sync_token) {
  // Can only wait on an unverified sync token if it is from the same channel.
  int sync_token_channel_id =
      ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
  if (sync_token.namespace_id() != gpu::CommandBufferNamespace::GPU_IO ||
      sync_token_channel_id != host_->channel_id()) {
    return false;
  }
  return true;
}

void SharedImageInterfaceProxy::VerifyFlush() {
  host_->VerifyFlush(UINT32_MAX);
}

void SharedImageInterfaceProxy::WaitSyncToken(const SyncToken& sync_token) {
  if (!sync_token.HasData())
    return;

  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewNop(0)),
        std::move(dependencies), /*release_count=*/0);
  }
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
    auto shm = base::ReadOnlySharedMemoryRegion::Create(size_to_alloc);
    if (!shm.IsValid())
      return false;

    // Duplicate the buffer for sharing to the GPU process.
    base::ReadOnlySharedMemoryRegion readonly_shm = shm.region.Duplicate();
    if (!readonly_shm.IsValid())
      return false;

    // Share the SHM to the GPU process. In order to ensure that any deferred
    // messages which rely on the previous SHM have a chance to execute before
    // it is replaced, send this message in the deferred queue.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewRegisterUploadBuffer(
                std::move(readonly_shm))),
        /*sync_token_fences=*/{}, /*release_count=*/0);
    host_->EnsureFlush(last_flush_id_);

    upload_buffer_ = std::move(shm);
    upload_buffer_offset_ = 0;
  }

  // We now have an `upload_buffer_` that fits our data.

  base::span<uint8_t> target =
      GetTargetData(upload_buffer_, upload_buffer_offset_, pixel_data.size());
  DCHECK(!target.empty());
  target.copy_from(pixel_data);
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

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageInterfaceProxy::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  host_->GetGpuChannel().RegisterSysmemBufferCollection(
      mojo::PlatformHandle(std::move(service_handle)),
      mojo::PlatformHandle(std::move(sysmem_token)), format, usage,
      register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void SharedImageInterfaceProxy::AddReferenceToSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  std::vector<SyncToken> dependencies =
      GenerateDependenciesFromSyncToken(std::move(sync_token), host_);
  {
    base::AutoLock lock(lock_);
    if (AddMailboxOrAddReference(mailbox)) {
      // Note: we enqueue the IPC under the lock to guarantee monotonicity of
      // the release ids as seen by the service.
      last_flush_id_ = host_->EnqueueDeferredMessage(
          mojom::DeferredRequestParams::NewSharedImageRequest(
              mojom::DeferredSharedImageRequest::NewAddReferenceToSharedImage(
                  mojom::AddReferenceToSharedImageParams::New(mailbox))),
          std::move(dependencies), ++next_release_id_);
    }
  }
}

void SharedImageInterfaceProxy::AddMailbox(const Mailbox& mailbox) {
  bool added = AddMailboxOrAddReference(mailbox);
  CHECK(added);
}

bool SharedImageInterfaceProxy::AddMailboxOrAddReference(
    const Mailbox& mailbox) {
  lock_.AssertAcquired();

  auto& info = mailbox_infos_[mailbox];
  info.ref_count++;
  return info.ref_count == 1;
}

void SharedImageInterfaceProxy::NotifyMailboxAdded(
    const Mailbox& mailbox,
    gpu::SharedImageUsageSet usage) {
  base::AutoLock lock(lock_);
  AddMailbox(mailbox);
}

void SharedImageInterfaceProxy::CreateSharedImagePool(
    const SharedImagePoolId& pool_id,
    mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote) {
  auto params = mojom::CreateSharedImagePoolParams::New();
  params->pool_id = pool_id;
  params->client_remote = std::move(client_remote);
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewCreateSharedImagePool(
                std::move(params))),
        /*sync_token_fences=*/{}, ++next_release_id_);
  }
}

void SharedImageInterfaceProxy::DestroySharedImagePool(
    const SharedImagePoolId& pool_id) {
  auto params = mojom::DestroySharedImagePoolParams::New();
  params->pool_id = pool_id;
  {
    base::AutoLock lock(lock_);
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    last_flush_id_ = host_->EnqueueDeferredMessage(
        mojom::DeferredRequestParams::NewSharedImageRequest(
            mojom::DeferredSharedImageRequest::NewDestroySharedImagePool(
                std::move(params))),
        /*sync_token_fences=*/{}, ++next_release_id_);
  }
}

SharedImageInterfaceProxy::SharedImageRefData::SharedImageRefData() = default;
SharedImageInterfaceProxy::SharedImageRefData::~SharedImageRefData() = default;

SharedImageInterfaceProxy::SharedImageRefData::SharedImageRefData(
    SharedImageRefData&&) = default;
SharedImageInterfaceProxy::SharedImageRefData&
SharedImageInterfaceProxy::SharedImageRefData::operator=(SharedImageRefData&&) =
    default;

}  // namespace gpu
