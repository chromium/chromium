// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "base/feature_list.h"
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
#include "ui/gfx/color_space.h"

namespace blink {

namespace {
// Controls whether we add SHARED_IMAGE_USAGE_WEBGPU_READ by default to shared
// image backed CanvasResources so that they can be imported into WebGPU without
// an intermediate copy. This could cause a different shared image backing type
// to be used in the GPU process based on the OS platform.
BASE_FEATURE(kCanvasResourceIsWebGPUCompatible,
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether CanvasResource::WaitSyncToken(const SyncToken&) should
// defer wait (when enabled) or wait immediately (when disabled).
BASE_FEATURE(kCanvasResourceDefersWaitSyncToken,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

CanvasResource::CanvasResource()
    : owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()) {}

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

static void ReleaseFrameResources(
    scoped_refptr<CanvasResource>&& resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  CHECK(resource);

  resource->WaitSyncToken(sync_token);

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

  ReleaseFrameResources(std::move(resource), gpu::SyncToken(),
                        /*is_lost=*/false);
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    CanvasResource::ReleaseCallback* out_callback,
    bool needs_verified_synctoken) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  *out_callback = blink::BindOnce(&ReleaseFrameResources);

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

  if (needs_verified_synctoken) {
    VerifySyncToken();
  }

  *out_resource = viz::TransferableResource::Make(
      client_shared_image, GetTransferableResourceSource(), sync_token());

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

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<CanvasResourceProviderSharedImage> provider,
    base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
        shared_image_interface_provider)
    : is_accelerated_(false),
      alpha_type_(alpha_type),
      provider_(std::move(provider)) {
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
  // software SharedImages at the time of vending it in VerifySyncToken(),
  // so we instead ensure that it is verified now.
  owning_thread_data().sync_token =
      shared_image_interface->GenVerifiedSyncToken();
  GetClientSharedImage()->UpdateDestructionSyncToken(
      owning_thread_data().sync_token);
}

scoped_refptr<CanvasResourceSharedImage>
CanvasResourceSharedImage::CreateSoftware(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<CanvasResourceProviderSharedImage> provider,
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
    base::WeakPtr<CanvasResourceProviderSharedImage> provider,
    bool is_accelerated,
    gpu::SharedImageUsageSet shared_image_usage_flags)
    : context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_accelerated_(is_accelerated),
      alpha_type_(alpha_type),
      provider_(std::move(provider)) {
  auto* shared_image_interface =
      context_provider_wrapper_->ContextProvider().SharedImageInterface();
  DCHECK(shared_image_interface);

  // These SharedImages are both read and written by the raster interface (both
  // occur, for example, when copying canvas resources between canvases).
  // Additionally, these SharedImages can be put into
  // AcceleratedStaticBitmapImages (via Bitmap()) that are then copied into GL
  // textures by WebGL (via AcceleratedStaticBitmapImage::CopyToTexture()).
  shared_image_usage_flags =
      shared_image_usage_flags | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE | gpu::SHARED_IMAGE_USAGE_GLES2_READ;
  // Add WEBGPU_READ usage to allow importing into WebGPU without a copy.
  if (base::FeatureList::IsEnabled(kCanvasResourceIsWebGPUCompatible)) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
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

  auto* raster_interface = RasterInterface();
  DCHECK(raster_interface);
  owning_thread_data().client_shared_image = client_shared_image;

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(client_shared_image->creation_sync_token());
}

scoped_refptr<CanvasResourceSharedImage> CanvasResourceSharedImage::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProviderSharedImage> provider,
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

SkImageInfo CanvasResourceSharedImage::CreateSkImageInfo() const {
  auto size = GetClientSharedImage()->size();
  auto format = GetClientSharedImage()->format();
  auto color_space = GetClientSharedImage()->color_space();
  return SkImageInfo::Make(SkISize::Make(size.width(), size.height()),
                           viz::ToClosestSkColorType(format), alpha_type_,
                           color_space.ToSkColorSpace());
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
}

void CanvasResourceSharedImage::Transfer() {
  if (is_cross_thread() || !ContextProviderWrapper())
    return;

  // TODO(khushalsagar): This is for consistency with MailboxTextureHolder
  // transfer path. It's unclear why the verification can not be deferred until
  // the resource needs to be transferred cross-process.
  VerifySyncToken();
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
    image->SetHighEntropyCanvasOpTypes(HighEntropyCanvasOpTypes());
    return image;
  }

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  // Note that the code in CanvasResourceProvider::RecycleResource also uses the
  // ref-count on the resource as a proxy for a read lock to allow recycling the
  // resource once all refs have been released.
  auto release_callback = base::BindOnce(
      &ReleaseFrameResources, scoped_refptr<CanvasResourceSharedImage>(this));

  scoped_refptr<StaticBitmapImage> image;
  const auto& client_shared_image = GetClientSharedImage();

  // If its cross thread, then the sync token was already verified.
  image = AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      client_shared_image, sync_token(), GetAlphaType(),
      context_provider_wrapper_, owning_thread_ref_, owning_thread_task_runner_,
      std::move(release_callback));

  DCHECK(image);
  image->SetHighEntropyCanvasOpTypes(HighEntropyCanvasOpTypes());
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

  std::unique_ptr<gpu::RasterScopedAccess> access =
      BeginAccess(/*readonly=*/true);
  // Additionally ensure that the next compositor read waits for the external
  // write to complete by ensuring that a new sync token is generated on the
  // internal interface as part of generating the TransferableResource. This new
  // sync token will be chained after `external_write_sync_token` thanks to the
  // wait above.
  EndAccess(std::move(access));
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
  GetClientSharedImage()->UpdateDestructionSyncToken(
      owning_thread_data().sync_token);
}

void CanvasResourceSharedImage::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    acquire_sync_token_ = sync_token;
    GetClientSharedImage()->UpdateDestructionSyncToken(acquire_sync_token_);
    if (!base::FeatureList::IsEnabled(kCanvasResourceDefersWaitSyncToken)) {
      if (auto* interface_base = InterfaceBase()) {
        interface_base->WaitSyncTokenCHROMIUM(
            acquire_sync_token_.GetConstData());
      }
    }
  }
}

std::unique_ptr<gpu::RasterScopedAccess> CanvasResourceSharedImage::BeginAccess(
    bool readonly) {
  return GetClientSharedImage()->BeginRasterAccess(
      RasterInterface(), acquire_sync_token_, readonly);
}

void CanvasResourceSharedImage::EndAccess(
    std::unique_ptr<gpu::RasterScopedAccess> access) {
  CHECK(!GetClientSharedImage()->is_software());
  DCHECK(!is_cross_thread());

  owning_thread_data().sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(access));
  GetClientSharedImage()->UpdateDestructionSyncToken(
      owning_thread_data().sync_token);
}

void CanvasResourceSharedImage::VerifySyncToken() {
  if (!owning_thread_data().sync_token.verified_flush()) {
    int8_t* token_data = owning_thread_data().sync_token.GetData();
    auto* raster_interface = RasterInterface();
    raster_interface->ShallowFlushCHROMIUM();
    raster_interface->VerifySyncTokensCHROMIUM(&token_data, 1);
    owning_thread_data().sync_token.SetVerifyFlush();
  }
}

void CanvasResourceSharedImage::NotifyResourceLost() {
  owning_thread_data().is_lost = true;
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

CanvasResourceProviderSharedImage* CanvasResourceSharedImage::Provider() {
  return provider_.get();
}

void CanvasResourceSharedImage::PrepareForWebGPUDummyMailbox() {
  // In the dummy WebGPU mailbox case, we skip write operation to CanvasResource
  // and therefore did not wait on `acquire_sync_token_`. Instead, the consumer
  // needs to do it.
  owning_thread_data().sync_token = acquire_sync_token_;
}

// ExternalCanvasResource
//==============================================================================
scoped_refptr<ExternalCanvasResource> ExternalCanvasResource::Create(
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    gfx::HDRMetadata hdr_metadata,
    viz::ReleaseCallback release_callback,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Create");
  CHECK(client_si);
  auto resource = AdoptRef(new ExternalCanvasResource(
      std::move(client_si), sync_token, resource_source, hdr_metadata,
      std::move(release_callback), std::move(context_provider_wrapper)));
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

  if (release_callback_) {
    GetSyncToken();
    std::move(release_callback_).Run(sync_token(), resource_is_lost_);
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

  GetSyncToken();
  scoped_refptr<StaticBitmapImage> image =
      AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
          client_si_, sync_token(), GetAlphaType(), context_provider_wrapper_,
          owning_thread_ref_, owning_thread_task_runner_,
          std::move(release_callback));
  image->SetHighEntropyCanvasOpTypes(HighEntropyCanvasOpTypes());
  return image;
}

void ExternalCanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    if (auto* interface_base = InterfaceBase()) {
      interface_base->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    }
  }
}

void ExternalCanvasResource::GetSyncToken() {
  // This method is expected to be used both in WebGL and WebGPU, that's why it
  // uses InterfaceBase.
  if (!sync_token_.HasData()) {
    auto* interface = InterfaceBase();
    if (interface)
      interface->GenSyncTokenCHROMIUM(sync_token_.GetData());
  } else {
    VerifySyncToken();
  }
}

void ExternalCanvasResource::VerifySyncToken() {
  if (!sync_token_.verified_flush()) {
    // The offscreencanvas usage needs the sync_token to be verified in order to
    // be able to use it by the compositor. This is why this method produces a
    // verified token even if no verification is explicitly requested.
    int8_t* token_data = sync_token_.GetData();
    auto* interface = InterfaceBase();
    DCHECK(interface);
    interface->ShallowFlushCHROMIUM();
    interface->VerifySyncTokensCHROMIUM(&token_data, 1);
    sync_token_.SetVerifyFlush();
  }
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
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : client_si_(std::move(client_si)),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      sync_token_(sync_token),
      resource_source_(resource_source),
      hdr_metadata_(hdr_metadata),
      release_callback_(std::move(out_callback)),
      alpha_type_(kPremul_SkAlphaType) {
  CHECK(client_si_);
  DCHECK(!release_callback_ || sync_token_.HasData());
}

}  // namespace blink
