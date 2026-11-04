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
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
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

CanvasResource::CanvasResource(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : gpu::ClientImage(std::move(shared_image)),
      owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()) {
  subclass_manages_destruction_sync_token_ = true;
}

// static
void CanvasResource::DropRefOnOwningThread(
    scoped_refptr<CanvasResource> resource) {
  CHECK(resource);
  DCHECK(!resource->is_cross_thread());

  // Allow the resource to determine whether it wants to preserve itself for
  // reuse.
  auto* raw_resource = resource.get();
  raw_resource->OnRefReturned(std::move(resource));
}

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : CanvasResource(shared_image) {}

void CanvasResourceSharedImage::InitializeSoftware(
    base::WeakPtr<Client> client,
    base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
        shared_image_interface_provider,
    const gfx::HDRMetadata& hdr_metadata) {
  DCHECK(GetSharedImage());
  DCHECK(!is_initialized_);
  DCHECK(shared_image_interface_provider);

  client_ = std::move(client);
  is_accelerated_ = false;
  hdr_metadata_ = hdr_metadata;

  // This class doesn't currently have a way of verifying the sync token for
  // software SharedImages at the time of vending it in VerifySyncToken(),
  // so we instead ensure that it is verified now.
  auto* shared_image_interface =
      shared_image_interface_provider->SharedImageInterface();
  DCHECK(shared_image_interface);
  gpu::SyncToken sync_token = GetSharedImage()->creation_sync_token();
  shared_image_interface->VerifySyncToken(sync_token);
  SetReleaseSyncToken(sync_token);
  GetSharedImage()->UpdateDestructionSyncToken(sync_token);

  is_initialized_ = true;
}

void CanvasResourceSharedImage::Initialize(
    base::WeakPtr<Client> client,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    const gfx::HDRMetadata& hdr_metadata,
    bool is_accelerated) {
  DCHECK(GetSharedImage());
  DCHECK(!is_initialized_);
  DCHECK(context_provider_wrapper);
  DCHECK(context_provider_wrapper->ContextProvider().RasterInterface());

  client_ = std::move(client);
  context_provider_wrapper_ = std::move(context_provider_wrapper);
  is_accelerated_ = is_accelerated;
  hdr_metadata_ = hdr_metadata;

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(GetSharedImage()->creation_sync_token());

  is_initialized_ = true;
}

scoped_refptr<CanvasResourceSharedImage>
CanvasResourceSharedImage::CreateForTesting(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    gpu::SharedImageUsageSet shared_image_usage_flags,
    bool is_software,
    bool is_accelerated,
    base::WeakPtr<Client> client,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
        shared_image_interface_provider) {
  gpu::ImageInfo image_info(size, format, shared_image_usage_flags, color_space,
                            kTopLeft_GrSurfaceOrigin, alpha_type,
                            /*buffer_usage=*/std::nullopt, is_software);
  auto* sii =
      is_software
          ? shared_image_interface_provider->SharedImageInterface()
          : context_provider_wrapper->ContextProvider().SharedImageInterface();
  auto image_pool = gpu::SharedImagePool<CanvasResourceSharedImage>::Create(
      image_info, sii, "CanvasResourceSharedImage", /*max_pool_size=*/0);

  auto canvas_resource = image_pool->GetImage();
  if (is_software) {
    DCHECK(!is_accelerated);
    canvas_resource->InitializeSoftware(
        client, std::move(shared_image_interface_provider), gfx::HDRMetadata());
  } else {
    canvas_resource->Initialize(client, context_provider_wrapper,
                                gfx::HDRMetadata(), is_accelerated);
  }
  return canvas_resource;
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
  if (client_) {
    client_->OnResourceRefReturned(std::move(downcast_ref));
  }
}

SkImageInfo CanvasResourceSharedImage::CreateSkImageInfo() const {
  auto size = GetSharedImage()->size();
  auto format = GetSharedImage()->format();
  auto color_space = GetSharedImage()->color_space();
  auto alpha_type = GetSharedImage()->alpha_type();
  return SkImageInfo::Make(SkISize::Make(size.width(), size.height()),
                           viz::ToClosestSkColorType(format), alpha_type,
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

  if (client_) {
    client_->OnDestroyResource();
  }
}

void CanvasResourceSharedImage::UploadSoftwareRenderingResults(
    SkSurface* sk_surface) {
  // Copy the rendering results from `sk_surface` to the SharedImage backing
  // this resource.
  auto scoped_mapping = GetSharedImage()->Map();
  if (!scoped_mapping) {
    LOG(ERROR) << "MapSharedImage failed.";
    return;
  }

  sk_surface->readPixels(
      scoped_mapping->GetSkPixmapForPlane(0, CreateSkImageInfo()), 0, 0);
  scoped_mapping.reset();

  // If using GPU compositing, inform the service that the SharedImage's backing
  // memory was written to on the CPU and update this resource's sync token to
  // ensure proper sequencing of future accesses to the SI with respect to this
  // call on the service side.
  // NOTE: Giving the GPU service this update is not necessary for the case
  // where the software compositor is being used, as all accesses to the SI's
  // backing happen via shared memory.  It's also not currently trivial to add
  // in this case as setting the sync token here would require it to later be
  // verified before it is sent to the display compositor.
  if (!GetSharedImage()->is_software()) {
    DCHECK(!is_cross_thread());
    auto sync_token =
        GetSharedImage()->BackingWasExternallyUpdated(gpu::SyncToken());
    SetReleaseSyncToken(sync_token);
    GetSharedImage()->UpdateDestructionSyncToken(sync_token);
  }
}

void CanvasResourceSharedImage::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  DCHECK(!is_cross_thread());
  if (sync_token.HasData()) {
    acquire_sync_token_ = sync_token;
    GetSharedImage()->UpdateDestructionSyncToken(acquire_sync_token_);
  }
}

void CanvasResourceSharedImage::NotifyResourceLost() {
  DCHECK(!is_cross_thread());
  resource_is_lost_ = true;
}

void CanvasResourceSharedImage::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_path) const {
  scoped_refptr<gpu::ClientSharedImage> client_si = GetSharedImage();

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

}  // namespace blink
