// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    const gfx::HDRMetadata& hdr_metadata,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> provider,
    bool accelerated_raster,
    base::RepeatingCallback<void(gpu::SyncToken)> wait_sync_token,
    viz::ReleaseCallback release_callback,
    bool use_legacy_external_canvas_resource_bitmap_path)
    : owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()),
      shared_image_(std::move(client_si)),
      sync_token_(sync_token),
      hdr_metadata_(hdr_metadata),
      resource_source_(resource_source),
      accelerated_raster_(accelerated_raster),
      context_provider_wrapper_(provider),
      wait_sync_token_(std::move(wait_sync_token)),
      release_callback_(std::move(release_callback)),
      use_legacy_external_canvas_resource_bitmap_path_(
          use_legacy_external_canvas_resource_bitmap_path) {}

ExportedCanvasResource::~ExportedCanvasResource() {
  // `wait_sync_token_` might hold ref to the resource, so drop it first.
  wait_sync_token_ = base::NullCallback();

  // We PostTask ExportedCanvasResource to the CanvasResource owning thread, but
  // if the thread gets destroyed before, destructor will still run, but we
  // can't tell CanvasResource to recycle.
  if (!is_cross_thread() && release_callback_) {
    std::move(release_callback_).Run(last_access_sync_token_, is_lost_);
  }
}

// static
void ExportedCanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<ExportedCanvasResource>&& exported_resource) {
  if (!exported_resource) {
    return;
  }

  if (exported_resource->is_cross_thread()) {
    auto owning_thread_task_runner =
        exported_resource->owning_thread_task_runner_;
    owning_thread_task_runner->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(exported_resource)));
  }
}

gfx::Size ExportedCanvasResource::Size() const {
  return shared_image_->size();
}

bool ExportedCanvasResource::OriginClean() const {
  return origin_clean_;
}

scoped_refptr<StaticBitmapImage> ExportedCanvasResource::Bitmap() {
  TRACE_EVENT0("blink", "ExportedCanvasResource::Bitmap");
  if (use_legacy_external_canvas_resource_bitmap_path_) {
    // On same thread we need to make sure context was not dropped, but
    // in the cross-thread case, checking a WeakPtr in not thread safe, not
    // to mention that we will use a shared context rather than the context
    // of origin to access the resource. In that case we will find out
    // whether the resource was dropped later, when we attempt to access the
    // mailbox.
    if (!is_cross_thread() && !context_provider_wrapper_) {
      return nullptr;
    }

    // The |release_callback| keeps a ref on this resource to ensure the backing
    // shared image is kept alive until the lifetime of the image.
    auto release_callback = base::BindOnce(
        [](scoped_refptr<ExportedCanvasResource> resource,
           const gpu::SyncToken& sync_token, bool is_lost) {
          // Do nothing but hold onto the refptr.
        },
        base::WrapRefCounted(this));

    scoped_refptr<StaticBitmapImage> image =
        AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
            shared_image_, sync_token_, kPremul_SkAlphaType, hdr_metadata_,
            context_provider_wrapper_, owning_thread_ref_,
            owning_thread_task_runner_, std::move(release_callback));
    if (image) {
      image->SetOriginClean(OriginClean());
    }
    return image;
  } else {
    if (!accelerated_raster_) {
      // Construct an SkImage that references the shared memory buffer.
      auto mapping = shared_image_->Map();
      if (!mapping) {
        LOG(ERROR) << "MapSharedImage Failed.";
        return nullptr;
      }

      auto sk_image =
          SkImages::RasterFromPixmapCopy(mapping->GetSkPixmapForPlane(0));

      // Unmap the underlying buffer.
      mapping.reset();
      if (!sk_image) {
        return nullptr;
      }

      auto image = UnacceleratedStaticBitmapImage::Create(
          sk_image, ImageOrientationEnum::kDefault, hdr_metadata_);
      image->SetOriginClean(OriginClean());
      return image;
    }

    // The |release_callback| keeps a ref on this resource to ensure the backing
    // shared image is kept alive until the lifetime of the image.
    // Note that the code in CanvasResourceProvider::RecycleResource also uses
    // the ref-count on the resource as a proxy for a read lock to allow
    // recycling the resource once all refs have been released.
    auto release_callback = base::BindOnce(
        [](scoped_refptr<ExportedCanvasResource>&& exported_resource,
           const gpu::SyncToken& sync_token, bool lost_resource) {
          exported_resource->last_access_sync_token_ = sync_token;
          if (exported_resource->wait_sync_token_) {
            exported_resource->wait_sync_token_.Run(sync_token);
          }
          if (lost_resource) {
            exported_resource->is_lost_ = true;
          }
        },
        base::WrapRefCounted(this));

    scoped_refptr<StaticBitmapImage> image;

    // If its cross thread, then the sync token was already verified.
    image = AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
        shared_image_, sync_token_, shared_image_->alpha_type(), hdr_metadata_,
        context_provider_wrapper_, owning_thread_ref_,
        owning_thread_task_runner_, std::move(release_callback));

    DCHECK(image);
    if (image) {
      image->SetOriginClean(OriginClean());
    }
    return image;
  }
}

void ExportedCanvasResource::EndDisplayCompositorAccess(
    gpu::SharedImageExportResult export_result,
    bool is_lost) {
  auto sync_token = shared_image_->EndExport(std::move(export_result));
  EndDisplayCompositorAccess(sync_token, is_lost);
}

void ExportedCanvasResource::EndDisplayCompositorAccess(
    gpu::SyncToken sync_token,
    bool is_lost) {
  last_access_sync_token_ = sync_token;
  if (wait_sync_token_) {
    wait_sync_token_.Run(sync_token);
  }

  if (is_lost) {
    is_lost_ = true;
  }
}

viz::TransferableResource
ExportedCanvasResource::PrepareTransferableResource() {
  TRACE_EVENT0("blink", "CanvasResource::PrepareTransferableResource");

  auto resource = viz::TransferableResource::Make(
      shared_image_, resource_source_, sync_token_);

  resource.hdr_metadata = hdr_metadata_;

  // When the compositor returns an resource, it provides a sync token to allow
  // subsequent accelerated raster operations to properly sequence their usage
  // of the resource within the GPU service. However, if raster is not
  // accelerated, sync tokens are not sufficient for ordering: We must instead
  // ensure that the compositor's reads have actually *completed* before the
  // compositor returns the resource for reuse, as after that point subsequent
  // raster operations will start writing to the resource via the CPU without
  // any subsequent synchronization with the GPU service.
  if (!accelerated_raster_) {
    resource.synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }

  return resource;
}

void ExportedCanvasResource::SetOriginClean(bool clean) {
  origin_clean_ = clean;
}

}  // namespace blink
