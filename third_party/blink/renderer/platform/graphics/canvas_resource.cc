// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
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
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"

namespace blink {

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               SkAlphaType alpha_type,
                               const gfx::ColorSpace& color_space)
    : owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()),
      provider_(std::move(provider)),
      alpha_type_(alpha_type),
      color_space_(color_space) {}

CanvasResource::~CanvasResource() {}

gpu::InterfaceBase* CanvasResource::InterfaceBase() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider().InterfaceBase();
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider().ContextGL();
}

gpu::raster::RasterInterface* CanvasResource::RasterInterface() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider().RasterInterface();
}

gpu::webgpu::WebGPUInterface* CanvasResource::WebGPUInterface() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider().WebGPUInterface();
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
  CHECK(resource);

  resource->WaitSyncToken(sync_token);

  if (resource_provider)
    resource_provider->NotifyTexParamsModified(resource.get());

  // TODO(khushalsagar): If multiple readers had access to this resource, losing
  // it once should make sure subsequent releases don't try to recycle this
  // resource.
  if (lost_resource) {
    resource->NotifyResourceLost();
  } else {
    // Allow the resource to determine whether it wants to preserve itself for
    // reuse.
    auto* raw_resource = resource.get();
    raw_resource->OnRefReturned(std::move(resource));
  }
}

// static
void CanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<CanvasResource> resource) {
  if (!resource) {
    return;
  }

  auto& owning_thread_task_runner = resource->owning_thread_task_runner_;
  owning_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&OnPlaceholderReleasedResourceOnOwningThread,
                                std::move(resource)));
}

// static
void CanvasResource::OnPlaceholderReleasedResourceOnOwningThread(
    scoped_refptr<CanvasResource> resource) {
  DCHECK(!resource->is_cross_thread());

  auto weak_provider = resource->WeakProvider();
  ReleaseFrameResources(std::move(weak_provider), std::move(resource),
                        gpu::SyncToken(), /*is_lost=*/false);
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    CanvasResource::ReleaseCallback* out_callback,
    bool needs_verified_synctoken) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  *out_callback = WTF::BindOnce(&ReleaseFrameResources, provider_);

  if (!out_resource)
    return true;

  auto client_shared_image = GetClientSharedImage();
  if (!client_shared_image) {
    return false;
  }

  TRACE_EVENT0("blink", "CanvasResource::PrepareTransferableResource");

  if (CreatesAcceleratedTransferableResources() && !ContextProviderWrapper()) {
    return false;
  }

  *out_resource = viz::TransferableResource::Make(
      client_shared_image, GetTransferableResourceSource(),
      GetSyncTokenWithOptionalVerification(needs_verified_synctoken));

  out_resource->hdr_metadata = GetHDRMetadata();
  out_resource->is_low_latency_rendering = client_shared_image->usage().Has(
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);

  // When the compositor returns an accelerated resource, it provides a sync
  // token to allow subsequent accelerated raster operations to properly
  // sequence their usage of the resource within the GPU service. However, if
  // the compositor is accelerated but raster is *not*, sync tokens are not
  // sufficient for ordering: We must instead ensure that the compositor's
  // GPU-side reads have actually *completed* before the compositor returns the
  // resource for reuse, as after that point subsequent raster operations will
  // start writing to the resource via the CPU without any subsequent
  // synchronization with the GPU service.
  if (!UsesAcceleratedRaster() && CreatesAcceleratedTransferableResources()) {
    DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
    out_resource->synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }

  return true;
}

SkImageInfo CanvasResource::CreateSkImageInfo() const {
  auto size = GetClientSharedImage()->size();
  auto format = GetClientSharedImage()->format();
  return SkImageInfo::Make(SkISize::Make(size.width(), size.height()),
                           viz::ToClosestSkColorType(format), alpha_type_,
                           color_space_.ToSkColorSpace());
}

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<CanvasResourceProvider> provider,
    base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
        shared_image_interface_provider)
    : CanvasResource(std::move(provider),
                     alpha_type,
                     color_space),
      is_accelerated_(false),
      use_oop_rasterization_(false) {
  if (!shared_image_interface_provider) {
    return;
  }
  auto* shared_image_interface =
      shared_image_interface_provider->SharedImageInterface();
  if (!shared_image_interface) {
    return;
  }

  owning_thread_data().client_shared_image =
      shared_image_interface->CreateSharedImageForSoftwareCompositor(
          {format, size, color_space, gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
           "CanvasResourceSharedImage"});

  // This class doesn't currently have a way of verifying the sync token for
  // software SharedImages at the time of vending it in
  // GetSyncTokenWithOptionalVerification(), so we instead ensure that it is
  // verified now.
  owning_thread_data().sync_token =
      shared_image_interface->GenVerifiedSyncToken();
  owning_thread_data().mailbox_needs_new_sync_token = false;
}

scoped_refptr<CanvasResourceSharedImage>
CanvasResourceSharedImage::CreateSoftware(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<CanvasResourceProvider> provider,
    base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
        shared_image_interface_provider) {
  auto resource = AdoptRef(new CanvasResourceSharedImage(
      size, format, alpha_type, color_space, std::move(provider),
      std::move(shared_image_interface_provider)));
  return resource->IsValid() ? resource : nullptr;
}

CanvasResourceSharedImage::CanvasResourceSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags)
    : CanvasResource(std::move(provider),
                     alpha_type,
                     color_space),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_accelerated_(is_accelerated),
      use_oop_rasterization_(is_accelerated &&
                             context_provider_wrapper_->ContextProvider()
                                 .GetCapabilities()
                                 .gpu_rasterization) {
  auto* shared_image_interface =
      context_provider_wrapper_->ContextProvider().SharedImageInterface();
  DCHECK(shared_image_interface);

  // These SharedImages are both read and written by the raster interface (both
  // occur, for example, when copying canvas resources between canvases).
  // Additionally, these SharedImages can be put into
  // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into GL
  // textures by WebGL (via AcceleratedStaticBitmapImage::CopyToTexture()).
  // Hence, GLES2_READ usage is necessary regardless of whether raster is over
  // GLES.
  shared_image_usage_flags =
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE | gpu::SHARED_IMAGE_USAGE_GLES2_READ;
  if (use_oop_rasterization_) {
    shared_image_usage_flags =
        shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  } else {
    // The GLES2_WRITE flag is needed due to raster being over GL.
    shared_image_usage_flags =
        shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;
  }

  scoped_refptr<gpu::ClientSharedImage> client_shared_image;
  if (!is_accelerated_) {
    // Ideally we should add SHARED_IMAGE_USAGE_CPU_WRITE_ONLY to the shared
    // image usage flag here since mailbox will be used for CPU writes by the
    // client. But doing that stops us from using CompoundImagebacking as many
    // backings do not support SHARED_IMAGE_USAGE_CPU_WRITE_ONLY.
    // TODO(crbug.com/1478238): Add that usage flag back here once the issue is
    // resolved.

    client_shared_image = shared_image_interface->CreateSharedImage(
        {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
         gpu::SharedImageUsageSet(shared_image_usage_flags),
         "CanvasResourceRasterGmb"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
    if (!client_shared_image) {
      return;
    }
  } else {
    client_shared_image = shared_image_interface->CreateSharedImage(
        {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
         gpu::SharedImageUsageSet(shared_image_usage_flags),
         "CanvasResourceRaster"},
        gpu::kNullSurfaceHandle);
    CHECK(client_shared_image);
  }

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(shared_image_interface->GenUnverifiedSyncToken());

  auto* raster_interface = RasterInterface();
  DCHECK(raster_interface);
  owning_thread_data().client_shared_image = client_shared_image;

  if (use_oop_rasterization_)
    return;

  // For the non-accelerated case, writes are done on the CPU. So we don't need
  // a texture for reads or writes.
  if (!is_accelerated_)
    return;

  owning_thread_data().texture_id_for_read_access =
      raster_interface->CreateAndConsumeForGpuRaster(client_shared_image);

  if (shared_image_usage_flags.Has(
          gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    owning_thread_data().texture_id_for_write_access =
        raster_interface->CreateAndConsumeForGpuRaster(client_shared_image);
  } else {
    owning_thread_data().texture_id_for_write_access =
        owning_thread_data().texture_id_for_read_access;
  }
}

scoped_refptr<CanvasResourceSharedImage> CanvasResourceSharedImage::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags) {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Create");
  auto resource = base::AdoptRef(new CanvasResourceSharedImage(
      size, format, alpha_type, color_space,
      std::move(context_provider_wrapper), std::move(provider), is_accelerated,
      shared_image_usage_flags));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceSharedImage::OnRefReturned(
    scoped_refptr<CanvasResource>&& resource) {
  // Create a downcast ref to the resource as a CanvasResourceSI to pass over to
  // the provider.
  auto downcast_ref = scoped_refptr<CanvasResourceSharedImage>(this);
  CHECK_EQ(downcast_ref, resource);

  // Reset the passed-in ref now that we've added a ref in `downcast_ref` to
  // ensure that the provider sees the actual number of currently-outstanding
  // refs (necessary for the provider to actually recycle the resource in the
  // case where there this is the last outstanding ref).
  resource.reset();
  if (Provider()) {
    Provider()->OnResourceRefReturned(std::move(downcast_ref));
  }
}

bool CanvasResourceSharedImage::IsValid() const {
  return !!owning_thread_data_.client_shared_image;
}

void CanvasResourceSharedImage::BeginWriteAccess() {
  RasterInterface()->BeginSharedImageAccessDirectCHROMIUM(
      GetTextureIdForWriteAccess(),
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

void CanvasResourceSharedImage::EndWriteAccess() {
  RasterInterface()->EndSharedImageAccessDirectCHROMIUM(
      GetTextureIdForWriteAccess());
}

GrBackendTexture CanvasResourceSharedImage::CreateGrTexture() const {
  scoped_refptr<gpu::ClientSharedImage> client_si = GetClientSharedImage();

  GrGLTextureInfo texture_info = {};
  texture_info.fID = GetTextureIdForWriteAccess();
  texture_info.fTarget = GetClientSharedImage()->GetTextureTarget();
  texture_info.fFormat =
      context_provider_wrapper_->ContextProvider().GetGrGLTextureFormat(
          client_si->format());
  return GrBackendTextures::MakeGL(client_si->size().width(),
                                   client_si->size().height(),
                                   skgpu::Mipmapped::kNo, texture_info);
}

CanvasResourceSharedImage::~CanvasResourceSharedImage() {
  if (is_cross_thread()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists and it is not possible to do cleanup of any GPU
    // context-associated state.
    return;
  }

  if (Provider()) {
    Provider()->OnDestroyResource();
  }

  // The context deletes all shared images on destruction which means no
  // cleanup is needed if the context was lost.
  if (ContextProviderWrapper() && IsValid()) {
    auto* raster_interface = RasterInterface();
    auto* shared_image_interface =
        ContextProviderWrapper()->ContextProvider().SharedImageInterface();
    if (raster_interface && shared_image_interface) {
      gpu::SyncToken shared_image_sync_token;
      raster_interface->GenUnverifiedSyncTokenCHROMIUM(
          shared_image_sync_token.GetData());
      owning_thread_data().client_shared_image->UpdateDestructionSyncToken(
          shared_image_sync_token);
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

void CanvasResourceSharedImage::Transfer() {
  if (is_cross_thread() || !ContextProviderWrapper())
    return;

  // TODO(khushalsagar): This is for consistency with MailboxTextureHolder
  // transfer path. It's unclear why the verification can not be deferred until
  // the resource needs to be transferred cross-process.
  GetSyncTokenWithOptionalVerification(true);
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedImage::Bitmap() {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Bitmap");

  if (!is_accelerated_) {
    if (!IsValid()) {
      return nullptr;
    }

    // Construct an SkImage that references the shared memory buffer.
    auto mapping = GetClientSharedImage()->Map();
    if (!mapping) {
      LOG(ERROR) << "MapSharedImage Failed.";
      return nullptr;
    }

    auto sk_image = SkImages::RasterFromPixmapCopy(
        mapping->GetSkPixmapForPlane(0, CreateSkImageInfo()));

    // Unmap the underlying buffer.
    mapping.reset();
    if (!sk_image) {
      return nullptr;
    }

    auto image = UnacceleratedStaticBitmapImage::Create(sk_image);
    image->SetOriginClean(OriginClean());
    return image;
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
  auto release_callback = base::BindOnce(
      &OnBitmapImageDestroyed, scoped_refptr<CanvasResourceSharedImage>(this),
      has_read_ref_on_texture);

  scoped_refptr<StaticBitmapImage> image;
  const auto& client_shared_image = GetClientSharedImage();

  // If its cross thread, then the sync token was already verified.
  image = AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      client_shared_image, GetSyncToken(), texture_id_for_image,
      client_shared_image->size(), client_shared_image->format(),
      GetAlphaType(), GetColorSpace(), context_provider_wrapper_,
      owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback));

  DCHECK(image);
  return image;
}

const scoped_refptr<gpu::ClientSharedImage>&
CanvasResourceSharedImage::GetClientSharedImage() const {
  CHECK(owning_thread_data_.client_shared_image);
  return owning_thread_data_.client_shared_image;
}

void CanvasResourceSharedImage::EndExternalWrite(
    const gpu::SyncToken& external_write_sync_token) {
  // Ensure that any subsequent internal accesses wait for the external write to
  // complete.
  WaitSyncToken(external_write_sync_token);

  // Additionally ensure that the next compositor read waits for the external
  // write to complete by ensuring that a new sync token is generated on the
  // internal interface as part of generating the TransferableResource. This new
  // sync token will be chained after `external_write_sync_token` thanks to the
  // wait above.
  owning_thread_data_.mailbox_needs_new_sync_token = true;
}

void CanvasResourceSharedImage::UploadSoftwareRenderingResults(
    SkSurface* sk_surface) {
  auto scoped_mapping = GetClientSharedImage()->Map();
  if (!scoped_mapping) {
    LOG(ERROR) << "MapSharedImage failed.";
    return;
  }

  sk_surface->readPixels(
      scoped_mapping->GetSkPixmapForPlane(0, CreateSkImageInfo()), 0, 0);

  // Making the below call is not necessary for the case where the the software
  // compositor is being used, as all accesses to the SI's backing happen via
  // shared memory. It's also not currently trivial to add in this case as
  // setting the sync token here would require it to later be verified before it
  // is sent to the display compositor.
  if (GetClientSharedImage()->is_software()) {
    return;
  }

  // Unmap the SI, inform the service that the SharedImage's backing memory was
  // written to on the CPU and update this resource's sync token to ensure
  // proper sequencing of future accesses to the SI with respect to this call on
  // the service side.
  scoped_mapping.reset();

  DCHECK(!is_cross_thread());
  owning_thread_data().sync_token =
      GetClientSharedImage()->BackingWasExternallyUpdated(gpu::SyncToken());
}

const gpu::SyncToken
CanvasResourceSharedImage::GetSyncTokenWithOptionalVerification(
    bool needs_verified_token) {
  if (GetClientSharedImage()->is_software()) {
    // This class doesn't currently have a way of verifying the sync token
    // within this call for software SharedImages, so it instead ensures that it
    // is verified at the time of generation.
    DCHECK(!mailbox_needs_new_sync_token());
    DCHECK(sync_token().verified_flush());

    return sync_token();
  }

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

  if (needs_verified_token &&
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

void CanvasResourceSharedImage::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_path) const {
  if (!IsValid())
    return;

  scoped_refptr<gpu::ClientSharedImage> client_si = GetClientSharedImage();

  std::string dump_name =
      base::StringPrintf("%s/CanvasResource_0x%" PRIXPTR, parent_path.c_str(),
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = pmd->CreateAllocatorDump(dump_name);
  size_t memory_size =
      client_si->format().EstimatedSizeInBytes(client_si->size());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_size);

  client_si->OnMemoryDump(
      pmd, dump->guid(),
      static_cast<int>(gpu::TracingImportance::kClientOwner));
}

// ExternalCanvasResource
//==============================================================================
scoped_refptr<ExternalCanvasResource> ExternalCanvasResource::Create(
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    gfx::HDRMetadata hdr_metadata,
    viz::ReleaseCallback release_callback,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Create");
  CHECK(client_si);
  auto resource = AdoptRef(new ExternalCanvasResource(
      std::move(client_si), sync_token, resource_source, hdr_metadata,
      std::move(release_callback), std::move(context_provider_wrapper),
      std::move(provider)));
  return resource->IsValid() ? resource : nullptr;
}

ExternalCanvasResource::~ExternalCanvasResource() {
  if (is_cross_thread()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists and it is not possible to do cleanup of any GPU
    // context-associated state.
    return;
  }

  if (Provider()) {
    Provider()->OnDestroyResource();
  }

  if (release_callback_) {
    std::move(release_callback_).Run(GetSyncToken(), resource_is_lost_);
  }
}

bool ExternalCanvasResource::IsValid() const {
  // On same thread we need to make sure context was not dropped, but
  // in the cross-thread case, checking a WeakPtr in not thread safe, not
  // to mention that we will use a shared context rather than the context
  // of origin to access the resource. In that case we will find out
  // whether the resource was dropped later, when we attempt to access the
  // mailbox.
  return is_cross_thread() || context_provider_wrapper_;
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

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      client_si_, GetSyncToken(), /*shared_image_texture_id=*/0u,
      client_si_->size(), client_si_->format(), GetAlphaType(), GetColorSpace(),
      context_provider_wrapper_, owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback));
}

const gpu::SyncToken
ExternalCanvasResource::GetSyncTokenWithOptionalVerification(
    bool needs_verified_token) {
  // This method is expected to be used both in WebGL and WebGPU, that's why it
  // uses InterfaceBase.
  if (!sync_token_.HasData()) {
    auto* interface = InterfaceBase();
    if (interface)
      interface->GenSyncTokenCHROMIUM(sync_token_.GetData());
  } else if (!sync_token_.verified_flush()) {
    // The offscreencanvas usage needs the sync_token to be verified in order to
    // be able to use it by the compositor. This is why this method produces a
    // verified token even if `needs_verified_token` is false.
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
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    gfx::HDRMetadata hdr_metadata,
    viz::ReleaseCallback out_callback,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider)
    : CanvasResource(std::move(provider),
                     kPremul_SkAlphaType,
                     client_si->color_space()),
      client_si_(std::move(client_si)),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      sync_token_(sync_token),
      resource_source_(resource_source),
      hdr_metadata_(hdr_metadata),
      release_callback_(std::move(out_callback)) {
  CHECK(client_si_);
  DCHECK(!release_callback_ || sync_token_.HasData());
}

// CanvasResourceSwapChain
//==============================================================================
scoped_refptr<CanvasResourceSwapChain> CanvasResourceSwapChain::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider) {
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::Create");
  CHECK(context_provider_wrapper);
  auto resource = AdoptRef(new CanvasResourceSwapChain(
      size, format, alpha_type, color_space,
      std::move(context_provider_wrapper), std::move(provider)));

  // As the context provider wrapper is non-null, the created resource will be
  // valid.
  CHECK(resource->IsValid());
  return resource;
}

CanvasResourceSwapChain::~CanvasResourceSwapChain() {
  if (is_cross_thread()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists and it is not possible to do cleanup of any GPU
    // context-associated state.
    return;
  }

  if (Provider()) {
    Provider()->OnDestroyResource();
  }

  // The context deletes all shared images on destruction which means no
  // cleanup is needed if the context was lost.
  if (!context_provider_wrapper_) {
    return;
  }

  if (!use_oop_rasterization_) {
    auto* raster_interface =
        context_provider_wrapper_->ContextProvider().RasterInterface();
    DCHECK(raster_interface);
    raster_interface->EndSharedImageAccessDirectCHROMIUM(
        back_buffer_texture_id_);
    raster_interface->DeleteGpuRasterTexture(back_buffer_texture_id_);
  }

  // No synchronization is needed here because the GL SharedImageRepresentation
  // will keep the backing alive on the service until the textures are deleted.
  front_buffer_shared_image_->UpdateDestructionSyncToken(gpu::SyncToken());
  back_buffer_shared_image_->UpdateDestructionSyncToken(gpu::SyncToken());
}

bool CanvasResourceSwapChain::IsValid() const {
  return !!context_provider_wrapper_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceSwapChain::Bitmap() {
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

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      back_buffer_shared_image_, GetSyncToken(), shared_texture_id,
      back_buffer_shared_image_->size(), back_buffer_shared_image_->format(),
      GetAlphaType(), GetColorSpace(), context_provider_wrapper_,
      owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback));
}

const scoped_refptr<gpu::ClientSharedImage>&
CanvasResourceSwapChain::GetClientSharedImage() const {
  return front_buffer_shared_image_;
}

const gpu::SyncToken
CanvasResourceSwapChain::GetSyncTokenWithOptionalVerification(
    bool needs_verified_token) {
  DCHECK(sync_token_.verified_flush());
  return sync_token_;
}

void CanvasResourceSwapChain::PresentSwapChain() {
  DCHECK(!is_cross_thread());
  DCHECK(context_provider_wrapper_);
  TRACE_EVENT0("blink", "CanvasResourceSwapChain::PresentSwapChain");

  auto* raster_interface =
      context_provider_wrapper_->ContextProvider().RasterInterface();
  DCHECK(raster_interface);

  auto* sii =
      context_provider_wrapper_->ContextProvider().SharedImageInterface();
  DCHECK(sii);

  // Synchronize presentation and rendering.
  raster_interface->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
  sii->PresentSwapChain(sync_token_, back_buffer_shared_image_->mailbox());
  // This only gets called via the CanvasResourceDispatcher export path so a
  // verified sync token will be needed ultimately.
  sync_token_ = sii->GenVerifiedSyncToken();
  raster_interface->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  // Relinquish shared image access before copy when using legacy GL raster.
  if (!use_oop_rasterization_) {
    raster_interface->EndSharedImageAccessDirectCHROMIUM(
        back_buffer_texture_id_);
  }
  // PresentSwapChain() flips the front and back buffers, but the mailboxes
  // still refer to the current front and back buffer after present.  So the
  // front buffer contains the content we just rendered, and it needs to be
  // copied into the back buffer to support a retained mode like canvas expects.
  // The wait sync token ensure that the present executes before we do the copy.
  // Don't generate sync token after the copy so that it's not on critical path.
  raster_interface->CopySharedImage(front_buffer_shared_image_->mailbox(),
                                    back_buffer_shared_image_->mailbox(), 0, 0,
                                    0, 0,
                                    back_buffer_shared_image_->size().width(),
                                    back_buffer_shared_image_->size().height());
  // Restore shared image access after copy when using legacy GL raster.
  if (!use_oop_rasterization_) {
    raster_interface->BeginSharedImageAccessDirectCHROMIUM(
        back_buffer_texture_id_,
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceSwapChain::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

CanvasResourceSwapChain::CanvasResourceSwapChain(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider)
    : CanvasResource(std::move(provider),
                     alpha_type,
                     color_space),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      use_oop_rasterization_(context_provider_wrapper_->ContextProvider()
                                 .GetCapabilities()
                                 .gpu_rasterization) {
  CHECK(context_provider_wrapper_);

  // These SharedImages are both read and written by the raster interface (both
  // occur, for example, when copying canvas resources between canvases).
  // Additionally, these SharedImages can be put into
  // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into GL
  // textures by WebGL (via AcceleratedStaticBitmapImage::CopyToTexture()).
  // Hence, GLES2_READ usage is necessary regardless of whether raster is over
  // GLES.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
  if (use_oop_rasterization_) {
    usage = usage | gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  } else {
    // The GLES2_WRITE flag is needed due to raster being over GL.
    usage = usage | gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;
  }

  auto* sii =
      context_provider_wrapper_->ContextProvider().SharedImageInterface();
  DCHECK(sii);
  gpu::SharedImageInterface::SwapChainSharedImages shared_images =
      sii->CreateSwapChain(format, size, color_space, kTopLeft_GrSurfaceOrigin,
                           kPremul_SkAlphaType, usage);
  CHECK(shared_images.back_buffer);
  CHECK(shared_images.front_buffer);
  back_buffer_shared_image_ = std::move(shared_images.back_buffer);
  front_buffer_shared_image_ = std::move(shared_images.front_buffer);
  sync_token_ = sii->GenVerifiedSyncToken();

  // Wait for the mailboxes to be ready to be used.
  auto* raster_interface =
      context_provider_wrapper_->ContextProvider().RasterInterface();
  DCHECK(raster_interface);
  raster_interface->WaitSyncTokenCHROMIUM(sync_token_.GetData());

  // In OOPR mode we use mailboxes directly. We early out here because
  // we don't need a texture id, as access is managed in the gpu process.
  if (use_oop_rasterization_)
    return;

  back_buffer_texture_id_ = raster_interface->CreateAndConsumeForGpuRaster(
      back_buffer_shared_image_->mailbox());
  raster_interface->BeginSharedImageAccessDirectCHROMIUM(
      back_buffer_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

}  // namespace blink
