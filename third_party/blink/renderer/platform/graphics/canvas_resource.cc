// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gpu.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"

namespace blink {

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               cc::PaintFlags::FilterQuality filter_quality,
                               const SkColorInfo& info)
    : owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()),
      provider_(std::move(provider)),
      info_(info),
      filter_quality_(filter_quality) {}

CanvasResource::~CanvasResource() {
#if DCHECK_IS_ON()
  DCHECK(did_call_on_destroy_);
#endif
}

void CanvasResource::OnDestroy() {
  if (is_cross_thread()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists.
    Abandon();
  } else {
    if (provider_)
      provider_->OnDestroyResource();
    TearDown();
  }
#if DCHECK_IS_ON()
  did_call_on_destroy_ = true;
#endif
}

void CanvasResource::Release() {
  if (last_unref_callback_ && HasOneRef()) {
    // "this" will not be destroyed if last_unref_callback_ retains the
    // reference.
#if DCHECK_IS_ON()
    auto last_ref = base::WrapRefCounted(this);
    WTF::ThreadSafeRefCounted<CanvasResource>::Release();  // does not destroy.
#else
    // In a DCHECK build, AdoptRef would fail because it is only supposed to be
    // used on new objects.  Nonetheless, we prefer to use AdoptRef "illegally"
    // in non-DCHECK builds to avoid unnecessary atomic operations.
    auto last_ref = base::AdoptRef(this);
#endif
    std::move(last_unref_callback_).Run(std::move(last_ref));
  } else {
    WTF::ThreadSafeRefCounted<CanvasResource>::Release();
  }
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

gpu::webgpu::WebGPUInterface* CanvasResource::WebGPUInterface() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->WebGPUInterface();
}

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    if (auto* interface_base = InterfaceBase())
      interface_base->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
}

static void ReleaseFrameResources(
    base::WeakPtr<CanvasResourceProvider> resource_provider,
    scoped_refptr<CanvasResource>&& resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // If there is a LastUnrefCallback, we need to abort because recycling the
  // resource now will prevent the LastUnrefCallback from ever being called.
  // In such cases, ReleaseFrameResources will be called again when
  // CanvasResourceDispatcher destroys the corresponding FrameResource object,
  // at which time this resource will be safely recycled.
  if (!resource || resource->HasLastUnrefCallback())
    return;

  resource->WaitSyncToken(sync_token);

  if (resource_provider)
    resource_provider->NotifyTexParamsModified(resource.get());

  // TODO(khushalsagar): If multiple readers had access to this resource, losing
  // it once should make sure subsequent releases don't try to recycle this
  // resource.
  if (lost_resource)
    resource->NotifyResourceLost();
  if (resource_provider && !lost_resource && resource->IsRecycleable() &&
      resource->HasOneRef()) {
    resource_provider->RecycleResource(std::move(resource));
  }
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    CanvasResource::ReleaseCallback* out_callback,
    MailboxSyncMode sync_mode) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  *out_callback = WTF::BindOnce(&ReleaseFrameResources, provider_);

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

  *out_resource = viz::TransferableResource::MakeGpu(
      mailbox, GLFilter(), TextureTarget(), GetSyncToken(), Size(),
      GetResourceFormat(), IsOverlayCandidate());

  out_resource->color_space = GetColorSpace();
  if (NeedsReadLockFences()) {
    out_resource->synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }

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
  *out_resource =
      viz::TransferableResource::MakeSoftware(mailbox, Size(), viz::RGBA_8888);

  out_resource->color_space = GetColorSpace();

  return true;
}

GrDirectContext* CanvasResource::GetGrContext() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->GetGrContext();
}

SkImageInfo CanvasResource::CreateSkImageInfo() const {
  return SkImageInfo::Make(SkISize::Make(Size().width(), Size().height()),
                           info_);
}

GLenum CanvasResource::GLFilter() const {
  return filter_quality_ == cc::PaintFlags::FilterQuality::kNone ? GL_NEAREST
                                                                 : GL_LINEAR;
}

viz::ResourceFormat CanvasResource::GetResourceFormat() const {
  return viz::SkColorTypeToResourceFormat(info_.colorType());
}

gfx::BufferFormat CanvasResource::GetBufferFormat() const {
  return viz::BufferFormat(GetResourceFormat());
}

gfx::ColorSpace CanvasResource::GetColorSpace() const {
  SkColorSpace* color_space = info_.colorSpace();
  return color_space ? gfx::ColorSpace(*color_space)
                     : gfx::ColorSpace::CreateSRGB();
}

// CanvasResourceSharedBitmap
//==============================================================================

CanvasResourceSharedBitmap::CanvasResourceSharedBitmap(
    const SkImageInfo& info,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, info.colorInfo()),
      size_(info.width(), info.height()) {
  // Software compositing lazily uses RGBA_8888 as the resource format
  // everywhere but the content is expected to be rendered in N32 format.
  base::MappedReadOnlyRegion shm =
      viz::bitmap_allocation::AllocateSharedBitmap(Size(), viz::RGBA_8888);

  if (!shm.IsValid())
    return;

  shared_mapping_ = std::move(shm.mapping);
  shared_bitmap_id_ = viz::SharedBitmap::GenerateId();

  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher) {
    resource_dispatcher->DidAllocateSharedBitmap(std::move(shm.region),
                                                 shared_bitmap_id_);
  }
}

CanvasResourceSharedBitmap::~CanvasResourceSharedBitmap() {
  OnDestroy();
}

bool CanvasResourceSharedBitmap::IsValid() const {
  return shared_mapping_.IsValid();
}

gfx::Size CanvasResourceSharedBitmap::Size() const {
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
      SkISize::Make(Size().width(), Size().height()), GetSkColorInfo());
  SkPixmap pixmap(image_info, shared_mapping_.memory(),
                  image_info.minRowBytes());
  AddRef();
  sk_sp<SkImage> sk_image = SkImage::MakeFromRaster(
      pixmap,
      [](const void*, SkImage::ReleaseContext resource_to_unref) {
        static_cast<CanvasResourceSharedBitmap*>(resource_to_unref)->Release();
      },
      this);
  auto image = UnacceleratedStaticBitmapImage::Create(sk_image);
  image->SetOriginClean(is_origin_clean_);
  return image;
}

scoped_refptr<CanvasResourceSharedBitmap> CanvasResourceSharedBitmap::Create(
    const SkImageInfo& info,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality) {
  auto resource = AdoptRef(new CanvasResourceSharedBitmap(
      info, std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceSharedBitmap::TearDown() {
  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher && !shared_bitmap_id_.IsZero())
    resource_dispatcher->DidDeleteSharedBitmap(shared_bitmap_id_);
  shared_mapping_ = {};
}

void CanvasResourceSharedBitmap::Abandon() {
  shared_mapping_ = {};
}

void CanvasResourceSharedBitmap::NotifyResourceLost() {
  // Release our reference to the shared memory mapping since the resource can
  // no longer be safely recycled and this memory is needed for copy-on-write.
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
      SkISize::Make(Size().width(), Size().height()), GetSkColorInfo());
  bool read_pixels_successful = image->readPixels(
      image_info, shared_mapping_.memory(), image_info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
}

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality,
    const SkColorInfo& info)
    : CanvasResource(provider, filter_quality, info) {}

// CanvasResourceRasterSharedImage
//==============================================================================

CanvasResourceRasterSharedImage::CanvasResourceRasterSharedImage(
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality,
    bool is_origin_top_left,
    bool is_accelerated,
    uint32_t shared_image_usage_flags)
    : CanvasResourceSharedImage(std::move(provider),
                                filter_quality,
                                info.colorInfo()),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(info.width(), info.height()),
      is_origin_top_left_(is_origin_top_left),
      is_accelerated_(is_accelerated),
#if BUILDFLAG(IS_MAC)
      // On Mac, WebGPU usage is always backed by an IOSurface which should
      // should also use the GL_TEXTURE_RECTANGLE target instead of
      // GL_TEXTURE_2D. Setting |is_overlay_candidate_| both allows overlays,
      // and causes |texture_target_| to take the value returned from
      // gpu::GetBufferTextureTarget.
      is_overlay_candidate_(
          shared_image_usage_flags &
          (gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_WEBGPU)),
#else
      is_overlay_candidate_(shared_image_usage_flags &
                            gpu::SHARED_IMAGE_USAGE_SCANOUT),
#endif
      supports_display_compositing_(shared_image_usage_flags &
                                    gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
      texture_target_(is_overlay_candidate_
                          ? gpu::GetBufferTextureTarget(
                                gfx::BufferUsage::SCANOUT,
                                GetBufferFormat(),
                                context_provider_wrapper_->ContextProvider()
                                    ->GetCapabilities())
                          : GL_TEXTURE_2D),
      use_oop_rasterization_(is_accelerated &&
                             context_provider_wrapper_->ContextProvider()
                                 ->GetCapabilities()
                                 .supports_oop_raster) {
  auto* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!is_accelerated_) {
    DCHECK(gpu_memory_buffer_manager);
    DCHECK(shared_image_usage_flags & gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);

    gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        Size(), GetBufferFormat(), gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gpu::kNullSurfaceHandle, nullptr);
    if (!gpu_memory_buffer_)
      return;

    gpu_memory_buffer_->SetColorSpace(GetColorSpace());
  }

  auto* shared_image_interface =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(shared_image_interface);

  // The GLES2 flag is needed for rendering via GL using a GrContext.
  if (use_oop_rasterization_) {
    // TODO(crbug.com/1050845): Ideally we'd like to get rid of the GLES2 usage
    // flags. Adding them for now to isolate other errors/prepare for field
    // trials.
    shared_image_usage_flags = shared_image_usage_flags |
                               gpu::SHARED_IMAGE_USAGE_RASTER |
                               gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
                               gpu::SHARED_IMAGE_USAGE_GLES2 |
                               gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
  } else {
    shared_image_usage_flags = shared_image_usage_flags |
                               gpu::SHARED_IMAGE_USAGE_GLES2 |
                               gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
  }

  GrSurfaceOrigin surface_origin = is_origin_top_left_
                                       ? kTopLeft_GrSurfaceOrigin
                                       : kBottomLeft_GrSurfaceOrigin;
  SkAlphaType surface_alpha_type = GetSkColorInfo().alphaType();
  gpu::Mailbox shared_image_mailbox;
  if (gpu_memory_buffer_) {
    shared_image_mailbox = shared_image_interface->CreateSharedImage(
        gpu_memory_buffer_.get(), gpu_memory_buffer_manager, GetColorSpace(),
        surface_origin, surface_alpha_type, shared_image_usage_flags);
  } else {
    shared_image_mailbox = shared_image_interface->CreateSharedImage(
        GetResourceFormat(), Size(), GetColorSpace(), surface_origin,
        surface_alpha_type, shared_image_usage_flags, gpu::kNullSurfaceHandle);
  }

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(shared_image_interface->GenUnverifiedSyncToken());

  auto* raster_interface = RasterInterface();
  DCHECK(raster_interface);
  owning_thread_data().shared_image_mailbox = shared_image_mailbox;

  if (use_oop_rasterization_)
    return;

  // For the non-accelerated case, writes are done on the CPU. So we don't need
  // a texture for reads or writes.
  if (!is_accelerated_)
    return;

  owning_thread_data().texture_id_for_read_access =
      raster_interface->CreateAndConsumeForGpuRaster(shared_image_mailbox);

  if (shared_image_usage_flags &
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
    owning_thread_data().texture_id_for_write_access =
        raster_interface->CreateAndConsumeForGpuRaster(shared_image_mailbox);
  } else {
    owning_thread_data().texture_id_for_write_access =
        owning_thread_data().texture_id_for_read_access;
  }
}

scoped_refptr<CanvasResourceRasterSharedImage>
CanvasResourceRasterSharedImage::Create(
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality,
    bool is_origin_top_left,
    bool is_accelerated,
    uint32_t shared_image_usage_flags) {
  TRACE_EVENT0("blink", "CanvasResourceRasterSharedImage::Create");
  auto resource = base::AdoptRef(new CanvasResourceRasterSharedImage(
      info, std::move(context_provider_wrapper), std::move(provider),
      filter_quality, is_origin_top_left, is_accelerated,
      shared_image_usage_flags));
  return resource->IsValid() ? resource : nullptr;
}

bool CanvasResourceRasterSharedImage::IsValid() const {
  return !mailbox().IsZero();
}

void CanvasResourceRasterSharedImage::BeginReadAccess() {
  RasterInterface()->BeginSharedImageAccessDirectCHROMIUM(
      GetTextureIdForReadAccess(), GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
}

void CanvasResourceRasterSharedImage::EndReadAccess() {
  RasterInterface()->EndSharedImageAccessDirectCHROMIUM(
      GetTextureIdForReadAccess());
}

void CanvasResourceRasterSharedImage::BeginWriteAccess() {
  RasterInterface()->BeginSharedImageAccessDirectCHROMIUM(
      GetTextureIdForWriteAccess(),
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

void CanvasResourceRasterSharedImage::EndWriteAccess() {
  RasterInterface()->EndSharedImageAccessDirectCHROMIUM(
      GetTextureIdForWriteAccess());
}

GrBackendTexture CanvasResourceRasterSharedImage::CreateGrTexture() const {
  const auto& capabilities =
      context_provider_wrapper_->ContextProvider()->GetCapabilities();

  GrGLTextureInfo texture_info = {};
  texture_info.fID = GetTextureIdForWriteAccess();
  texture_info.fTarget = TextureTarget();
  texture_info.fFormat = viz::TextureStorageFormat(
      GetResourceFormat(), capabilities.angle_rgbx_internal_format);
  return GrBackendTexture(Size().width(), Size().height(), GrMipMapped::kNo,
                          texture_info);
}

CanvasResourceRasterSharedImage::~CanvasResourceRasterSharedImage() {
  OnDestroy();
}

void CanvasResourceRasterSharedImage::TearDown() {
  DCHECK(!is_cross_thread());

  // The context deletes all shared images on destruction which means no
  // cleanup is needed if the context was lost.
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

void CanvasResourceRasterSharedImage::Abandon() {
  // Called when the owning thread has been torn down which will destroy the
  // context on which the shared image was created so no cleanup is necessary.
}

void CanvasResourceRasterSharedImage::WillDraw() {
  DCHECK(!is_cross_thread())
      << "Write access is only allowed on the owning thread";

  // Sync token for software mode is generated from SharedImageInterface each
  // time the GMB is updated.
  if (!is_accelerated_)
    return;

  owning_thread_data().mailbox_needs_new_sync_token = true;
}

// static
void CanvasResourceRasterSharedImage::OnBitmapImageDestroyed(
    scoped_refptr<CanvasResourceRasterSharedImage> resource,
    bool has_read_ref_on_texture,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  DCHECK(!resource->is_cross_thread());

  if (has_read_ref_on_texture) {
    DCHECK(!resource->use_oop_rasterization_);
    DCHECK_GT(resource->owning_thread_data().bitmap_image_read_refs, 0u);

    resource->owning_thread_data().bitmap_image_read_refs--;
    if (resource->owning_thread_data().bitmap_image_read_refs == 0u &&
        resource->RasterInterface()) {
      resource->RasterInterface()->EndSharedImageAccessDirectCHROMIUM(
          resource->owning_thread_data().texture_id_for_read_access);
    }
  }

  auto weak_provider = resource->WeakProvider();
  ReleaseFrameResources(std::move(weak_provider), std::move(resource),
                        sync_token, is_lost);
}

void CanvasResourceRasterSharedImage::Transfer() {
  if (is_cross_thread() || !ContextProviderWrapper())
    return;

  // TODO(khushalsagar): This is for consistency with MailboxTextureHolder
  // transfer path. Its unclear why the verification can not be deferred until
  // the resource needs to be transferred cross-process.
  owning_thread_data().mailbox_sync_mode = kVerifiedSyncToken;
  GetSyncToken();
}

scoped_refptr<StaticBitmapImage> CanvasResourceRasterSharedImage::Bitmap() {
  TRACE_EVENT0("blink", "CanvasResourceRasterSharedImage::Bitmap");

  SkImageInfo image_info = CreateSkImageInfo();
  if (!is_accelerated_) {
    if (!gpu_memory_buffer_->Map())
      return nullptr;

    SkPixmap pixmap(CreateSkImageInfo(), gpu_memory_buffer_->memory(0),
                    gpu_memory_buffer_->stride(0));
    auto sk_image = SkImage::MakeRasterCopy(pixmap);
    gpu_memory_buffer_->Unmap();
    return sk_image ? UnacceleratedStaticBitmapImage::Create(sk_image)
                    : nullptr;
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
  const bool has_read_ref_on_texture =
      !is_cross_thread() && !use_oop_rasterization_;
  GLuint texture_id_for_image = 0u;
  if (has_read_ref_on_texture) {
    texture_id_for_image = owning_thread_data().texture_id_for_read_access;
    owning_thread_data().bitmap_image_read_refs++;
    if (owning_thread_data().bitmap_image_read_refs == 1u &&
        RasterInterface()) {
      RasterInterface()->BeginSharedImageAccessDirectCHROMIUM(
          texture_id_for_image, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }
  }

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  // Note that the code in CanvasResourceProvider::RecycleResource also uses the
  // ref-count on the resource as a proxy for a read lock to allow recycling the
  // resource once all refs have been released.
  auto release_callback =
      base::BindOnce(&OnBitmapImageDestroyed,
                     scoped_refptr<CanvasResourceRasterSharedImage>(this),
                     has_read_ref_on_texture);

  scoped_refptr<StaticBitmapImage> image;

  // If its cross thread, then the sync token was already verified.
  if (!is_cross_thread()) {
    owning_thread_data().mailbox_sync_mode = kUnverifiedSyncToken;
  }
  image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox(), GetSyncToken(), texture_id_for_image, image_info,
      texture_target_, is_origin_top_left_, context_provider_wrapper_,
      owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback), supports_display_compositing_,
      is_overlay_candidate_);

  DCHECK(image);
  return image;
}

void CanvasResourceRasterSharedImage::CopyRenderingResultsToGpuMemoryBuffer(
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

const gpu::Mailbox& CanvasResourceRasterSharedImage::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  if (!is_cross_thread()) {
    owning_thread_data().mailbox_sync_mode = sync_mode;
  }
  return mailbox();
}

bool CanvasResourceRasterSharedImage::HasGpuMailbox() const {
  return !mailbox().IsZero();
}

const gpu::SyncToken CanvasResourceRasterSharedImage::GetSyncToken() {
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

void CanvasResourceRasterSharedImage::NotifyResourceLost() {
  owning_thread_data().is_lost = true;

  if (WeakProvider())
    Provider()->NotifyTexParamsModified(this);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceRasterSharedImage::ContextProviderWrapper() const {
  DCHECK(!is_cross_thread());
  return context_provider_wrapper_;
}

void CanvasResourceRasterSharedImage::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    size_t bytes_per_pixel) const {
  if (!IsValid())
    return;

  std::string dump_name =
      base::StringPrintf("canvas/ResourceProvider/CanvasResource/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = pmd->CreateAllocatorDump(dump_name);
  size_t memory_size = Size().height() * Size().width() * bytes_per_pixel;
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_size);

  auto guid = gpu::GetSharedImageGUIDForTracing(mailbox());
  pmd->CreateSharedGlobalAllocatorDump(guid);
  // This memory is allocated on our behalf, claim it by setting the importance
  // to >0.
  pmd->AddOwnershipEdge(dump->guid(), guid, 1);
}

// ExternalCanvasResource
//==============================================================================
scoped_refptr<ExternalCanvasResource> ExternalCanvasResource::Create(
    const gpu::Mailbox& mailbox,
    viz::ReleaseCallback release_callback,
    gpu::SyncToken sync_token,
    const SkImageInfo& info,
    GLenum texture_target,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality,
    bool is_origin_top_left,
    bool is_overlay_candidate) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Create");
  auto resource = AdoptRef(new ExternalCanvasResource(
      mailbox, std::move(release_callback), sync_token, info, texture_target,
      std::move(context_provider_wrapper), std::move(provider), filter_quality,
      is_origin_top_left, is_overlay_candidate));
  return resource->IsValid() ? resource : nullptr;
}

ExternalCanvasResource::~ExternalCanvasResource() {
  OnDestroy();
}

bool ExternalCanvasResource::IsValid() const {
  // On same thread we need to make sure context was not dropped, but
  // in the cross-thread case, checking a WeakPtr in not thread safe, not
  // to mention that we will use a shared context rather than the context
  // of origin to access the resource. In that case we will find out
  // whether the resource was dropped later, when we attempt to access the
  // mailbox.
  return (is_cross_thread() || context_provider_wrapper_) && HasGpuMailbox();
}

void ExternalCanvasResource::Abandon() {
  // We don't need to destroy the shared image mailbox since we don't own it.
}

void ExternalCanvasResource::TakeSkImage(sk_sp<SkImage> image) {
  NOTREACHED();
}

scoped_refptr<StaticBitmapImage> ExternalCanvasResource::Bitmap() {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Bitmap");
  if (!IsValid())
    return nullptr;

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  auto release_callback = base::BindOnce(
      [](scoped_refptr<ExternalCanvasResource> resource,
         const gpu::SyncToken& sync_token, bool is_lost) {
        // Do nothing but hold onto the refptr.
      },
      base::RetainedRef(this));

  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox_, GetSyncToken(), /*shared_image_texture_id=*/0u,
      CreateSkImageInfo(), texture_target_, is_origin_top_left_,
      context_provider_wrapper_, owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback), /*supports_display_compositing=*/true,
      is_overlay_candidate_);
}

void ExternalCanvasResource::TearDown() {
  if (release_callback_)
    std::move(release_callback_).Run(GetSyncToken(), resource_is_lost_);
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
  // This method is expected to be used both in WebGL and WebGPU, that's why it
  // uses InterfaceBase.
  if (!sync_token_.HasData()) {
    auto* interface = InterfaceBase();
    if (interface)
      interface->GenSyncTokenCHROMIUM(sync_token_.GetData());
  } else if (!sync_token_.verified_flush()) {
    // The offscreencanvas usage needs the sync_token to be verified in order to
    // be able to use it by the compositor.
    int8_t* token_data = sync_token_.GetData();
    auto* interface = InterfaceBase();
    DCHECK(interface);
    interface->ShallowFlushCHROMIUM();
    interface->VerifySyncTokensCHROMIUM(&token_data, 1);
    sync_token_.SetVerifyFlush();
  }
  return sync_token_;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
ExternalCanvasResource::ContextProviderWrapper() const {
  // The context provider is not thread-safe, nor is the WeakPtr that holds it.
  DCHECK(!is_cross_thread());
  return context_provider_wrapper_;
}

ExternalCanvasResource::ExternalCanvasResource(
    const gpu::Mailbox& mailbox,
    viz::ReleaseCallback out_callback,
    gpu::SyncToken sync_token,
    const SkImageInfo& info,
    GLenum texture_target,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality,
    bool is_origin_top_left,
    bool is_overlay_candidate)
    : CanvasResource(std::move(provider), filter_quality, info.colorInfo()),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(info.width(), info.height()),
      mailbox_(mailbox),
      texture_target_(texture_target),
      release_callback_(std::move(out_callback)),
      sync_token_(sync_token),
      is_origin_top_left_(is_origin_top_left),
      is_overlay_candidate_(is_overlay_candidate) {
  DCHECK(!release_callback_ || sync_token_.HasData());
}

// CanvasResourceSwapChain
//==============================================================================
scoped_refptr<CanvasResourceSwapChain> CanvasResourceSwapChain::Create(
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality) {
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::Create");
  auto resource = AdoptRef(
      new CanvasResourceSwapChain(info, std::move(context_provider_wrapper),
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
      SkISize::Make(Size().width(), Size().height()), GetSkColorInfo());

  // It's safe to share the back buffer texture id if we're on the same thread
  // since the |release_callback| ensures this resource will be alive.
  GLuint shared_texture_id = !is_cross_thread() ? back_buffer_texture_id_ : 0u;

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  auto release_callback = base::BindOnce(
      [](scoped_refptr<CanvasResourceSwapChain>, const gpu::SyncToken&, bool) {
        // Do nothing but hold onto the refptr.
      },
      base::RetainedRef(this));

  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      back_buffer_mailbox_, GetSyncToken(), shared_texture_id, image_info,
      GL_TEXTURE_2D, true /*is_origin_top_left*/, context_provider_wrapper_,
      owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback), /*supports_display_compositing=*/true,
      /*is_overlay_candidate=*/true);
}

void CanvasResourceSwapChain::Abandon() {
  // Called when the owning thread has been torn down which will destroy the
  // context on which the shared image was created so no cleanup is necessary.
}

void CanvasResourceSwapChain::TearDown() {
  // The context deletes all shared images on destruction which means no
  // cleanup is needed if the context was lost.
  if (!context_provider_wrapper_)
    return;

  if (!use_oop_rasterization_) {
    auto* raster_interface =
        context_provider_wrapper_->ContextProvider()->RasterInterface();
    DCHECK(raster_interface);
    raster_interface->EndSharedImageAccessDirectCHROMIUM(
        back_buffer_texture_id_);
    raster_interface->DeleteGpuRasterTexture(back_buffer_texture_id_);
  }

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
  DCHECK(!is_cross_thread());
  DCHECK(context_provider_wrapper_);
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::PresentSwapChain");

  auto* raster_interface =
      context_provider_wrapper_->ContextProvider()->RasterInterface();
  DCHECK(raster_interface);

  auto* sii =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(sii);

  // Synchronize presentation and rendering.
  raster_interface->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
  sii->PresentSwapChain(sync_token_, back_buffer_mailbox_);
  // This only gets called via the CanvasResourceDispatcher export path so a
  // verified sync token will be needed ultimately.
  sync_token_ = sii->GenVerifiedSyncToken();
  raster_interface->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  // PresentSwapChain() flips the front and back buffers, but the mailboxes
  // still refer to the current front and back buffer after present.  So the
  // front buffer contains the content we just rendered, and it needs to be
  // copied into the back buffer to support a retained mode like canvas expects.
  // The wait sync token ensure that the present executes before we do the copy.
  raster_interface->CopySubTexture(front_buffer_mailbox_, back_buffer_mailbox_,
                                   GL_TEXTURE_2D, 0, 0, 0, 0, size_.width(),
                                   size_.height(), false /* unpack_flip_y */,
                                   false /* unpack_premultiply_alpha */);
  // Don't generate sync token here so that the copy is not on critical path.
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceSwapChain::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

CanvasResourceSwapChain::CanvasResourceSwapChain(
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    cc::PaintFlags::FilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, info.colorInfo()),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(info.width(), info.height()),
      use_oop_rasterization_(context_provider_wrapper_->ContextProvider()
                                 ->GetCapabilities()
                                 .supports_oop_raster) {
  if (!context_provider_wrapper_)
    return;

  uint32_t usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                   gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  if (use_oop_rasterization_) {
    usage = usage | gpu::SHARED_IMAGE_USAGE_RASTER |
            gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  }

  auto* sii =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(sii);
  gpu::SharedImageInterface::SwapChainMailboxes mailboxes =
      sii->CreateSwapChain(GetResourceFormat(), Size(), GetColorSpace(),
                           kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
                           usage);
  back_buffer_mailbox_ = mailboxes.back_buffer;
  front_buffer_mailbox_ = mailboxes.front_buffer;
  sync_token_ = sii->GenVerifiedSyncToken();

  // Wait for the mailboxes to be ready to be used.
  auto* raster_interface =
      context_provider_wrapper_->ContextProvider()->RasterInterface();
  DCHECK(raster_interface);
  raster_interface->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  // In OOPR mode we use mailboxes directly. We early out here because
  // we don't need a texture id, as access is managed in the gpu process.
  if (use_oop_rasterization_)
    return;

  back_buffer_texture_id_ =
      raster_interface->CreateAndConsumeForGpuRaster(back_buffer_mailbox_);
  raster_interface->BeginSharedImageAccessDirectCHROMIUM(
      back_buffer_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

}  // namespace blink
