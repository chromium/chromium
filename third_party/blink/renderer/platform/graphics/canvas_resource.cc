// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"

namespace blink {

// TODO(danakj): One day the gpu::mojom::Mailbox type should be shared with
// blink directly and we won't need to use gpu::mojom::blink::Mailbox, nor the
// conversion through WTF::Vector.
gpu::mojom::blink::MailboxPtr SharedBitmapIdToGpuMailboxPtr(
    const viz::SharedBitmapId& id) {
  WTF::Vector<int8_t> name(GL_MAILBOX_SIZE_CHROMIUM);
  for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
    name[i] = id.name[i];
  return {base::in_place, name};
}

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               SkFilterQuality filter_quality,
                               const CanvasColorParams& color_params)
    : owning_thread_id_(base::PlatformThread::CurrentId()),
      provider_(std::move(provider)),
      filter_quality_(filter_quality),
      color_params_(color_params) {}

CanvasResource::~CanvasResource() {
#if DCHECK_IS_ON()
  DCHECK(did_call_on_destroy_);
#endif
}

void CanvasResource::OnDestroy() {
  if (owning_thread_id_ != base::PlatformThread::CurrentId()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists.
    Abandon();
  } else {
    TearDown();
  }
#if DCHECK_IS_ON()
  did_call_on_destroy_ = true;
#endif
}

gpu::InterfaceBase* CanvasResource::InterfaceBase() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->InterfaceBase();
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->ContextGL();
}

gpu::raster::RasterInterface* CanvasResource::RasterInterface() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->RasterInterface();
}

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    if (auto* interface_base = InterfaceBase())
      interface_base->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
}

static void ReleaseFrameResources(
    base::WeakPtr<CanvasResourceProvider> resource_provider,
    scoped_refptr<CanvasResource> resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  resource->WaitSyncToken(sync_token);

  if (resource_provider)
    resource_provider->NotifyTexParamsModified(resource.get());

  // TODO(khushalsagar): If multiple readers had access to this resource, losing
  // it once should make sure subsequent releases don't try to recycle this
  // resource.
  // Also what about single buffered canvas?
  if (lost_resource)
    resource->NotifyResourceLost();
  if (resource_provider && !lost_resource && resource->IsRecycleable())
    resource_provider->RecycleResource(std::move(resource));
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback,
    MailboxSyncMode sync_mode) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  auto func = WTF::Bind(&ReleaseFrameResources, provider_,
                        WTF::Passed(base::WrapRefCounted(this)));
  *out_callback = viz::SingleReleaseCallback::Create(std::move(func));

  if (!out_resource)
    return true;
  if (SupportsAcceleratedCompositing())
    return PrepareAcceleratedTransferableResource(out_resource, sync_mode);
  return PrepareUnacceleratedTransferableResource(out_resource);
}

bool CanvasResource::PrepareAcceleratedTransferableResource(
    viz::TransferableResource* out_resource,
    MailboxSyncMode sync_mode) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareAcceleratedTransferableResource");
  // Gpu compositing is a prerequisite for compositing an accelerated resource
  DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
  if (!ContextProviderWrapper())
    return false;
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(sync_mode);
  if (mailbox.IsZero())
    return false;

  *out_resource = viz::TransferableResource::MakeGL(
      mailbox, GLFilter(), TextureTarget(), GetSyncToken(), gfx::Size(Size()),
      IsOverlayCandidate());

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();
  out_resource->format = color_params_.TransferableResourceFormat();
  out_resource->read_lock_fences_enabled = NeedsReadLockFences();

  return true;
}

bool CanvasResource::PrepareUnacceleratedTransferableResource(
    viz::TransferableResource* out_resource) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareUnacceleratedTransferableResource");
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(kVerifiedSyncToken);
  if (mailbox.IsZero())
    return false;

  // For software compositing, the display compositor assumes an N32 format for
  // the resource type and completely ignores the format set on the
  // TransferableResource. Clients are expected to render in N32 format but use
  // RGBA as the tagged format on resources.
  *out_resource = viz::TransferableResource::MakeSoftware(
      mailbox, gfx::Size(Size()), viz::RGBA_8888);

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();

  return true;
}

GrContext* CanvasResource::GetGrContext() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->GetGrContext();
}

SkImageInfo CanvasResource::CreateSkImageInfo() const {
  return SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
}

GLenum CanvasResource::GLFilter() const {
  return filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

// CanvasResourceSharedBitmap
//==============================================================================

CanvasResourceSharedBitmap::CanvasResourceSharedBitmap(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      size_(size) {
  // Software compositing lazily uses RGBA_8888 as the resource format
  // everywhere but the content is expected to be rendered in N32 format.
  base::MappedReadOnlyRegion shm = viz::bitmap_allocation::AllocateSharedBitmap(
      gfx::Size(Size()), viz::RGBA_8888);

  if (!shm.IsValid())
    return;

  shared_mapping_ = std::move(shm.mapping);
  shared_bitmap_id_ = viz::SharedBitmap::GenerateId();

  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher) {
    resource_dispatcher->DidAllocateSharedBitmap(
        std::move(shm.region),
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
}

CanvasResourceSharedBitmap::~CanvasResourceSharedBitmap() {
  OnDestroy();
}

bool CanvasResourceSharedBitmap::IsValid() const {
  return shared_mapping_.IsValid();
}

IntSize CanvasResourceSharedBitmap::Size() const {
  return size_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedBitmap::Bitmap() {
  if (!IsValid())
    return nullptr;
  // Construct an SkImage that references the shared memory buffer.
  // The release callback holds a reference to |this| to ensure that the
  // canvas resource that owns the shared memory stays alive at least until
  // the SkImage is destroyed.
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
  SkPixmap pixmap(image_info, shared_mapping_.memory(),
                  image_info.minRowBytes());
  this->AddRef();
  sk_sp<SkImage> sk_image = SkImage::MakeFromRaster(
      pixmap,
      [](const void*, SkImage::ReleaseContext resource_to_unref) {
        static_cast<CanvasResourceSharedBitmap*>(resource_to_unref)->Release();
      },
      this);
  auto image = StaticBitmapImage::Create(sk_image);
  image->SetOriginClean(is_origin_clean_);
  return image;
}

scoped_refptr<CanvasResourceSharedBitmap> CanvasResourceSharedBitmap::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  auto resource = AdoptRef(new CanvasResourceSharedBitmap(
      size, color_params, std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceSharedBitmap::TearDown() {
  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher && !shared_bitmap_id_.IsZero()) {
    resource_dispatcher->DidDeleteSharedBitmap(
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
  shared_mapping_ = {};
}

void CanvasResourceSharedBitmap::Abandon() {
  shared_mapping_ = {};
}

const gpu::Mailbox& CanvasResourceSharedBitmap::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  return shared_bitmap_id_;
}

bool CanvasResourceSharedBitmap::HasGpuMailbox() const {
  return !shared_bitmap_id_.IsZero();
}

void CanvasResourceSharedBitmap::TakeSkImage(sk_sp<SkImage> image) {
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(),
      ColorParams().GetSkColorSpaceForSkSurfaces());

  bool read_pixels_successful = image->readPixels(
      image_info, shared_mapping_.memory(), image_info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
}

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    const IntSize& size,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    bool is_overlay_candidate,
    bool is_origin_top_left,
    bool allow_concurrent_read_write_access,
    bool is_accelerated)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_overlay_candidate_(is_overlay_candidate),
      size_(size),
      is_origin_top_left_(is_origin_top_left),
      is_accelerated_(is_accelerated),
      texture_target_(
          is_overlay_candidate_
              ? gpu::GetBufferTextureTarget(
                    gfx::BufferUsage::SCANOUT,
                    BufferFormat(ColorParams().TransferableResourceFormat()),
                    context_provider_wrapper_->ContextProvider()
                        ->GetCapabilities())
              : GL_TEXTURE_2D),
      owning_thread_task_runner_(Thread::Current()->GetTaskRunner()) {
  if (!context_provider_wrapper_)
    return;

  auto* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!is_accelerated_) {
    DCHECK(gpu_memory_buffer_manager);

    gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        gfx::Size(size), ColorParams().GetBufferFormat(),
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE, gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_)
      return;

    gpu_memory_buffer_->SetColorSpace(color_params.GetStorageGfxColorSpace());
  }

  auto* shared_image_interface =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(shared_image_interface);

  uint32_t flags = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
  if (is_overlay_candidate_)
    flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  if (allow_concurrent_read_write_access)
    flags |= gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  gpu::Mailbox shared_image_mailbox;
  if (gpu_memory_buffer_) {
    shared_image_mailbox = shared_image_interface->CreateSharedImage(
        gpu_memory_buffer_.get(), gpu_memory_buffer_manager,
        ColorParams().GetStorageGfxColorSpace(), flags);
  } else {
    shared_image_mailbox = shared_image_interface->CreateSharedImage(
        ColorParams().TransferableResourceFormat(), gfx::Size(size),
        ColorParams().GetStorageGfxColorSpace(), flags);
  }

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(shared_image_interface->GenUnverifiedSyncToken());

  auto* raster_interface = RasterInterface();
  DCHECK(raster_interface);
  owning_thread_data().shared_image_mailbox = shared_image_mailbox;
  owning_thread_data().texture_id_for_read_access =
      raster_interface->CreateAndConsumeForGpuRaster(shared_image_mailbox);

  // For the non-accelerated case, writes are done on the CPU. So we don't need
  // a texture for writes.
  if (!is_accelerated_)
    return;
  if (allow_concurrent_read_write_access) {
    owning_thread_data().texture_id_for_write_access =
        raster_interface->CreateAndConsumeForGpuRaster(shared_image_mailbox);
  } else {
    owning_thread_data().texture_id_for_write_access =
        owning_thread_data().texture_id_for_read_access;
  }
}

scoped_refptr<CanvasResourceSharedImage> CanvasResourceSharedImage::Create(
    const IntSize& size,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    bool is_overlay_candidate,
    bool is_origin_top_left,
    bool allow_concurrent_read_write_access,
    bool is_accelerated) {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Create");
  auto resource = base::AdoptRef(new CanvasResourceSharedImage(
      size, std::move(context_provider_wrapper), std::move(provider),
      filter_quality, color_params, is_overlay_candidate, is_origin_top_left,
      allow_concurrent_read_write_access, is_accelerated));
  return resource->IsValid() ? resource : nullptr;
}

bool CanvasResourceSharedImage::IsValid() const {
  return !mailbox().IsZero();
}

CanvasResourceSharedImage::~CanvasResourceSharedImage() {
  OnDestroy();
}

GLenum CanvasResourceSharedImage::TextureTarget() const {
  return texture_target_;
}

void CanvasResourceSharedImage::TearDown() {
  DCHECK(!is_cross_thread());

  if (ContextProviderWrapper()) {
    auto* raster_interface = RasterInterface();
    auto* shared_image_interface =
        ContextProviderWrapper()->ContextProvider()->SharedImageInterface();
    if (raster_interface && shared_image_interface) {
      gpu::SyncToken shared_image_sync_token;
      raster_interface->GenUnverifiedSyncTokenCHROMIUM(
          shared_image_sync_token.GetData());
      shared_image_interface->DestroySharedImage(shared_image_sync_token,
                                                 mailbox());
    }
    if (raster_interface) {
      if (owning_thread_data().texture_id_for_read_access) {
        raster_interface->DeleteGpuRasterTexture(
            owning_thread_data().texture_id_for_read_access);
      }
      if (owning_thread_data().texture_id_for_write_access &&
          owning_thread_data().texture_id_for_write_access !=
              owning_thread_data().texture_id_for_read_access) {
        raster_interface->DeleteGpuRasterTexture(
            owning_thread_data().texture_id_for_write_access);
      }
    }
  }

  owning_thread_data().texture_id_for_read_access = 0u;
  owning_thread_data().texture_id_for_write_access = 0u;
}

void CanvasResourceSharedImage::Abandon() {
  if (auto context_provider = SharedGpuContext::ContextProviderWrapper()) {
    auto* sii = context_provider->ContextProvider()->SharedImageInterface();
    if (sii)
      sii->DestroySharedImage(gpu::SyncToken(), mailbox());
  }
}

void CanvasResourceSharedImage::WillDraw() {
  DCHECK(!is_cross_thread())
      << "Write access is only allowed on the owning thread";

  // Sync token for software mode is generated from SharedImageInterface each
  // time the GMB is updated.
  if (!is_accelerated_)
    return;

  owning_thread_data().mailbox_needs_new_sync_token = true;
}

// static
void CanvasResourceSharedImage::OnBitmapImageDestroyed(
    scoped_refptr<CanvasResourceSharedImage> resource,
    bool has_read_ref_on_texture,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  if (resource->is_cross_thread()) {
    auto& task_runner = *resource->owning_thread_task_runner_;
    PostCrossThreadTask(
        task_runner, FROM_HERE,
        CrossThreadBindOnce(&CanvasResourceSharedImage::OnBitmapImageDestroyed,
                            std::move(resource), has_read_ref_on_texture,
                            sync_token, is_lost));
    return;
  }

  if (has_read_ref_on_texture) {
    DCHECK_GT(resource->owning_thread_data().bitmap_image_read_refs, 0u);

    resource->owning_thread_data().bitmap_image_read_refs--;
    if (resource->owning_thread_data().bitmap_image_read_refs == 0u &&
        resource->ContextGL()) {
      resource->ContextGL()->EndSharedImageAccessDirectCHROMIUM(
          resource->owning_thread_data().texture_id_for_read_access);
    }
  }

  auto weak_provider = resource->WeakProvider();
  ReleaseFrameResources(std::move(weak_provider), std::move(resource),
                        sync_token, is_lost);
}

void CanvasResourceSharedImage::Transfer() {
  if (is_cross_thread() || !ContextProviderWrapper())
    return;

  // TODO(khushalsagar): This is for consistency with MailboxTextureHolder
  // transfer path. Its unclear why the verification can not be deferred until
  // the resource needs to be transferred cross-process.
  owning_thread_data().mailbox_sync_mode = kVerifiedSyncToken;
  GetSyncToken();
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedImage::Bitmap() {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Bitmap");

  SkImageInfo image_info = CreateSkImageInfo();
  if (!is_accelerated_) {
    if (!gpu_memory_buffer_->Map())
      return nullptr;

    SkPixmap pixmap(CreateSkImageInfo(), gpu_memory_buffer_->memory(0),
                    gpu_memory_buffer_->stride(0));
    auto sk_image = SkImage::MakeRasterCopy(pixmap);
    gpu_memory_buffer_->Unmap();
    return sk_image ? StaticBitmapImage::Create(sk_image) : nullptr;
  }

  // In order to avoid creating multiple representations for this shared image
  // on the same context, the AcceleratedStaticBitmapImage uses the texture id
  // of the resource here. We keep a count of pending shared image releases to
  // correctly scope the read lock for this texture.
  // If this resource is accessed across threads, or the
  // AcceleratedStaticBitmapImage is accessed on a different thread after being
  // created here, the image will create a new representation from the mailbox
  // rather than referring to the shared image's texture ID if it was provided
  // below.
  const bool has_read_ref_on_texture = !is_cross_thread();
  GLuint texture_id_for_image = 0u;
  if (has_read_ref_on_texture) {
    texture_id_for_image = owning_thread_data().texture_id_for_read_access;
    owning_thread_data().bitmap_image_read_refs++;
    if (owning_thread_data().bitmap_image_read_refs == 1u && ContextGL()) {
      ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
          texture_id_for_image, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }
  }

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  // Note that the code in CanvasResourceProvider::RecycleResource also uses the
  // ref-count on the resource as a proxy for a read lock to allow recycling the
  // resource once all refs have been released.
  auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
      &OnBitmapImageDestroyed, scoped_refptr<CanvasResourceSharedImage>(this),
      has_read_ref_on_texture));

  scoped_refptr<StaticBitmapImage> image;

  // If its cross thread, then the sync token was already verified. If not, then
  // we don't need one. The image lazily generates a token if needed.
  gpu::SyncToken token = is_cross_thread() ? sync_token() : gpu::SyncToken();
  image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox(), token, texture_id_for_image, image_info, texture_target_,
      context_provider_wrapper_, owning_thread_id_, is_origin_top_left_,
      std::move(release_callback));

  DCHECK(image);
  return image;
}

void CanvasResourceSharedImage::CopyRenderingResultsToGpuMemoryBuffer(
    const sk_sp<SkImage>& image) {
  DCHECK(!is_cross_thread());

  if (!ContextProviderWrapper() || !gpu_memory_buffer_->Map())
    return;

  auto surface = SkSurface::MakeRasterDirect(CreateSkImageInfo(),
                                             gpu_memory_buffer_->memory(0),
                                             gpu_memory_buffer_->stride(0));
  surface->getCanvas()->drawImage(image, 0, 0);
  auto* sii =
      ContextProviderWrapper()->ContextProvider()->SharedImageInterface();
  gpu_memory_buffer_->Unmap();
  sii->UpdateSharedImage(gpu::SyncToken(), mailbox());
  owning_thread_data().sync_token = sii->GenUnverifiedSyncToken();
}

const gpu::Mailbox& CanvasResourceSharedImage::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  if (!is_cross_thread()) {
    owning_thread_data().mailbox_sync_mode = sync_mode;
  }
  return mailbox();
}

bool CanvasResourceSharedImage::HasGpuMailbox() const {
  return !mailbox().IsZero();
}

const gpu::SyncToken CanvasResourceSharedImage::GetSyncToken() {
  if (is_cross_thread()) {
    // Sync token should be generated at Transfer time, which must always be
    // called before cross-thread usage. And since we don't allow writes on
    // another thread, the sync token generated at Transfer time shouldn't
    // have been invalidated.
    DCHECK(!mailbox_needs_new_sync_token());
    DCHECK(sync_token().verified_flush());

    return sync_token();
  }

  if (mailbox_needs_new_sync_token()) {
    auto* raster_interface = RasterInterface();
    DCHECK(raster_interface);  // caller should already have early exited if
                               // !raster_interface.
    raster_interface->GenUnverifiedSyncTokenCHROMIUM(
        owning_thread_data().sync_token.GetData());
    owning_thread_data().mailbox_needs_new_sync_token = false;
  }

  if (owning_thread_data().mailbox_sync_mode == kVerifiedSyncToken &&
      !owning_thread_data().sync_token.verified_flush()) {
    int8_t* token_data = owning_thread_data().sync_token.GetData();
    auto* raster_interface = RasterInterface();
    raster_interface->ShallowFlushCHROMIUM();
    raster_interface->VerifySyncTokensCHROMIUM(&token_data, 1);
    owning_thread_data().sync_token.SetVerifyFlush();
  }

  return sync_token();
}

void CanvasResourceSharedImage::NotifyResourceLost() {
  owning_thread_data().is_lost = true;

  if (WeakProvider())
    Provider()->NotifyTexParamsModified(this);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceSharedImage::ContextProviderWrapper() const {
  DCHECK(!is_cross_thread());
  return context_provider_wrapper_;
}

// ExternalCanvasResource
//==============================================================================
scoped_refptr<ExternalCanvasResource> ExternalCanvasResource::Create(
    const gpu::Mailbox& mailbox,
    const IntSize& size,
    GLenum texture_target,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Create");
  auto resource = AdoptRef(
      new ExternalCanvasResource(mailbox, size, texture_target, color_params,
                                 std::move(context_provider_wrapper),
                                 std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

ExternalCanvasResource::~ExternalCanvasResource() {
  OnDestroy();
}

bool ExternalCanvasResource::IsValid() const {
  return context_provider_wrapper_ && !mailbox_.IsZero();
}

void ExternalCanvasResource::Abandon() {
  // We don't need to do anything since we don't own the mailbox.
}

void ExternalCanvasResource::TakeSkImage(sk_sp<SkImage> image) {
  NOTREACHED();
}

scoped_refptr<StaticBitmapImage> ExternalCanvasResource::Bitmap() {
  NOTREACHED();
  return nullptr;
}

void ExternalCanvasResource::TearDown() {
  Abandon();
}

const gpu::Mailbox& ExternalCanvasResource::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::GetOrCreateGpuMailbox");
  DCHECK_EQ(sync_mode, kVerifiedSyncToken);
  return mailbox_;
}

bool ExternalCanvasResource::HasGpuMailbox() const {
  return !mailbox_.IsZero();
}

const gpu::SyncToken ExternalCanvasResource::GetSyncToken() {
  TRACE_EVENT0("blink", "ExternalCanvasResource::GetSyncToken");
  if (!sync_token_.HasData()) {
    auto* gl = ContextGL();
    if (gl)
      gl->GenSyncTokenCHROMIUM(sync_token_.GetData());
  }
  return sync_token_;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
ExternalCanvasResource::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

ExternalCanvasResource::ExternalCanvasResource(
    const gpu::Mailbox& mailbox,
    const IntSize& size,
    GLenum texture_target,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      texture_target_(texture_target),
      mailbox_(mailbox) {}

// CanvasResourceSwapChain
//==============================================================================
scoped_refptr<CanvasResourceSwapChain> CanvasResourceSwapChain::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::Create");
  auto resource = AdoptRef(new CanvasResourceSwapChain(
      size, color_params, std::move(context_provider_wrapper),
      std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

CanvasResourceSwapChain::~CanvasResourceSwapChain() {
  OnDestroy();
}

bool CanvasResourceSwapChain::IsValid() const {
  return context_provider_wrapper_ && HasGpuMailbox();
}

void CanvasResourceSwapChain::TakeSkImage(sk_sp<SkImage> image) {
  NOTREACHED();
}

scoped_refptr<StaticBitmapImage> CanvasResourceSwapChain::Bitmap() {
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());

  // It's safe to share the front buffer texture id if we're on the same thread
  // since the |release_callback| ensures this resource will be alive.
  GLuint shared_texture_id = 0u;
  if (base::PlatformThread::CurrentId() == owning_thread_id_)
    shared_texture_id = front_buffer_texture_id_;

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
      [](scoped_refptr<CanvasResourceSwapChain>, const gpu::SyncToken&, bool) {
        // Do nothing but hold onto the refptr.
      },
      base::RetainedRef(this)));

  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      front_buffer_mailbox_, sync_token_, shared_texture_id, image_info,
      GL_TEXTURE_2D, context_provider_wrapper_, owning_thread_id_,
      /*is_origin_top_left=*/true, std::move(release_callback));
}

void CanvasResourceSwapChain::Abandon() {
  if (auto context_provider = SharedGpuContext::ContextProviderWrapper()) {
    auto* sii = context_provider->ContextProvider()->SharedImageInterface();
    DCHECK(sii);
    sii->DestroySharedImage(gpu::SyncToken(), front_buffer_mailbox_);
    sii->DestroySharedImage(gpu::SyncToken(), back_buffer_mailbox_);
  }
}

void CanvasResourceSwapChain::TearDown() {
  if (!context_provider_wrapper_)
    return;
  auto* gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  DCHECK(gl);
  gl->EndSharedImageAccessDirectCHROMIUM(front_buffer_texture_id_);
  gl->DeleteTextures(1u, &front_buffer_texture_id_);
  gl->EndSharedImageAccessDirectCHROMIUM(back_buffer_texture_id_);
  gl->DeleteTextures(1u, &back_buffer_texture_id_);
  // No synchronization is needed here because the GL SharedImageRepresentation
  // will keep the backing alive on the service until the textures are deleted.
  auto* sii =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(gpu::SyncToken(), front_buffer_mailbox_);
  sii->DestroySharedImage(gpu::SyncToken(), back_buffer_mailbox_);
}

const gpu::Mailbox& CanvasResourceSwapChain::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  DCHECK_EQ(sync_mode, kVerifiedSyncToken);
  return front_buffer_mailbox_;
}

bool CanvasResourceSwapChain::HasGpuMailbox() const {
  return !front_buffer_mailbox_.IsZero();
}

const gpu::SyncToken CanvasResourceSwapChain::GetSyncToken() {
  DCHECK(sync_token_.verified_flush());
  return sync_token_;
}

void CanvasResourceSwapChain::PresentSwapChain() {
  DCHECK_EQ(base::PlatformThread::CurrentId(), owning_thread_id_);
  DCHECK(context_provider_wrapper_);
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::PresentSwapChain");

  auto* gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  DCHECK(gl);

  // Skia could've changed the filter state if the front buffer was exported as
  // a bitmap image.  This will be a nop on the service side most of the time.
  gl->BindTexture(GL_TEXTURE_2D, front_buffer_texture_id_);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilter());
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilter());

  auto* sii =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(sii);

  // Synchronize presentation and rendering.
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
  sii->PresentSwapChain(sync_token_, back_buffer_mailbox_);
  // This only gets called via the CanvasResourceDispatcher export path so a
  // verified sync token will be needed ultimately.
  sync_token_ = sii->GenVerifiedSyncToken();
  gl->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  // PresentSwapChain() flips the front and back buffers, but the mailboxes
  // still refer to the current front and back buffer after present.  So the
  // front buffer contains the content we just rendered, and it needs to be
  // copied into the back buffer to support a retained mode like canvas expects.
  // The wait sync token ensure that the present executes before we do the copy.
  gl->CopySubTextureCHROMIUM(
      front_buffer_texture_id_, 0, GL_TEXTURE_2D, back_buffer_texture_id_, 0, 0,
      0, 0, 0, size_.Width(), size_.Height(), GL_FALSE, GL_FALSE, GL_FALSE);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceSwapChain::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

CanvasResourceSwapChain::CanvasResourceSwapChain(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size) {
  if (!context_provider_wrapper_)
    return;

  uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto* sii =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(sii);
  gpu::SharedImageInterface::SwapChainMailboxes mailboxes =
      sii->CreateSwapChain(ColorParams().TransferableResourceFormat(),
                           gfx::Size(size),
                           ColorParams().GetStorageGfxColorSpace(), usage);
  back_buffer_mailbox_ = mailboxes.back_buffer;
  front_buffer_mailbox_ = mailboxes.front_buffer;
  sync_token_ = sii->GenVerifiedSyncToken();

  // Wait for the mailboxes to be ready to be used.
  auto* gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  DCHECK(gl);
  gl->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  front_buffer_texture_id_ =
      gl->CreateAndTexStorage2DSharedImageCHROMIUM(front_buffer_mailbox_.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      front_buffer_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  back_buffer_texture_id_ =
      gl->CreateAndTexStorage2DSharedImageCHROMIUM(back_buffer_mailbox_.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      back_buffer_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

}  // namespace blink
