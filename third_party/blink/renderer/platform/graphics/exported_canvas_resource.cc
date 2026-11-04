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

class PLATFORM_EXPORT ExternalCanvasResource final : public CanvasResource {
 public:
  ExternalCanvasResource(
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const gpu::SyncToken& sync_token,
      viz::TransferableResource::ResourceSource resource_source,
      const gfx::HDRMetadata& hdr_metadata,
      viz::ReleaseCallback out_callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>);

  bool CreatesAcceleratedTransferableResources() const override { return true; }
  void NotifyResourceLost() override { resource_is_lost_ = true; }
  void WaitSyncToken(const gpu::SyncToken&) override;

  const gfx::HDRMetadata& GetHdrMetadata() const override {
    return hdr_metadata_;
  }

  viz::TransferableResource::ResourceSource GetTransferableResourceSource()
      const final {
    return resource_source_;
  }
  bool UsesAcceleratedRaster() const final { return true; }

  ~ExternalCanvasResource() override;

  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  viz::TransferableResource::ResourceSource resource_source_;
  const gfx::HDRMetadata hdr_metadata_;
  viz::ReleaseCallback release_callback_;
  bool resource_is_lost_ = false;
  const SkAlphaType alpha_type_;

  // SyncToken to wait on at destruction.
  gpu::SyncToken destruction_sync_token_;
};

ExternalCanvasResource::~ExternalCanvasResource() {
  if (is_cross_thread()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists and it is not possible to do cleanup of any GPU
    // context-associated state.
    return;
  }

  if (release_callback_) {
    std::move(release_callback_)
        .Run(destruction_sync_token_, resource_is_lost_);
  }
}

void ExternalCanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    destruction_sync_token_ = sync_token;
  }
}

ExternalCanvasResource::ExternalCanvasResource(
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    const gfx::HDRMetadata& hdr_metadata,
    viz::ReleaseCallback out_callback,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : CanvasResource(std::move(client_si)),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_source_(resource_source),
      hdr_metadata_(hdr_metadata),
      release_callback_(std::move(out_callback)),
      alpha_type_(kPremul_SkAlphaType) {
  CHECK(GetSharedImage());
  SetReleaseSyncToken(sync_token);
}

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<gpu::ClientSharedImage> client_si,
    const gpu::SyncToken& sync_token,
    viz::TransferableResource::ResourceSource resource_source,
    const gfx::HDRMetadata& hdr_metadata,
    viz::ReleaseCallback callback,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> provider)
    : resource_(
          base::MakeRefCounted<ExternalCanvasResource>(std::move(client_si),
                                                       sync_token,
                                                       resource_source,
                                                       hdr_metadata,
                                                       std::move(callback),
                                                       provider)),
      owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()),
      use_legacy_external_canvas_resource_bitmap_path_(true) {
  CHECK(resource_);
  CHECK(!resource_->is_cross_thread());
}

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<CanvasResourceSharedImage> resource)
    : resource_(std::move(resource)),
      owning_thread_ref_(base::PlatformThread::CurrentRef()),
      owning_thread_task_runner_(
          ThreadScheduler::Current()->CleanupTaskRunner()),
      use_legacy_external_canvas_resource_bitmap_path_(false) {
  CHECK(resource_);
  CHECK(!resource_->is_cross_thread());
}

ExportedCanvasResource::~ExportedCanvasResource() {
  // We PostTask ExportedCanvasResource to the CanvasResource owning thread, but
  // if the thread gets destroyed before, destructor will still run, but we
  // can't tell CanvasResource to recycle.
  if (!is_cross_thread()) {
    CanvasResource::DropRefOnOwningThread(std::move(resource_));
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
  return resource_->GetSharedImage()->size();
}

bool ExportedCanvasResource::OriginClean() const {
  return origin_clean_;
}

scoped_refptr<StaticBitmapImage> ExportedCanvasResource::Bitmap() {
  TRACE_EVENT0("blink", "ExportedCanvasResource::Bitmap");
  if (use_legacy_external_canvas_resource_bitmap_path_) {
    ExternalCanvasResource* external_resource =
        static_cast<ExternalCanvasResource*>(resource_.get());

    // On same thread we need to make sure context was not dropped, but
    // in the cross-thread case, checking a WeakPtr in not thread safe, not
    // to mention that we will use a shared context rather than the context
    // of origin to access the resource. In that case we will find out
    // whether the resource was dropped later, when we attempt to access the
    // mailbox.
    if (!is_cross_thread() && !external_resource->context_provider_wrapper_) {
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
            resource_->GetSharedImage(), resource_->sync_token(),
            external_resource->alpha_type_, resource_->GetHdrMetadata(),
            external_resource->context_provider_wrapper_, owning_thread_ref_,
            owning_thread_task_runner_, std::move(release_callback));
    return image;
  } else {
    CanvasResourceSharedImage* resource_si =
        static_cast<CanvasResourceSharedImage*>(resource_.get());
    if (!resource_si->is_accelerated_) {
      // Construct an SkImage that references the shared memory buffer.
      auto mapping = resource_->GetSharedImage()->Map();
      if (!mapping) {
        LOG(ERROR) << "MapSharedImage Failed.";
        return nullptr;
      }

      auto sk_image = SkImages::RasterFromPixmapCopy(
          mapping->GetSkPixmapForPlane(0, resource_si->CreateSkImageInfo()));

      // Unmap the underlying buffer.
      mapping.reset();
      if (!sk_image) {
        return nullptr;
      }

      auto image = UnacceleratedStaticBitmapImage::Create(
          sk_image, ImageOrientationEnum::kDefault,
          resource_->GetHdrMetadata());
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
          CHECK(exported_resource->resource_);

          exported_resource->resource_->WaitSyncToken(sync_token);
          if (lost_resource) {
            exported_resource->resource_->NotifyResourceLost();
          }
        },
        base::WrapRefCounted(this));

    scoped_refptr<StaticBitmapImage> image;
    const auto& client_shared_image = resource_->GetSharedImage();

    // If its cross thread, then the sync token was already verified.
    image = AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
        client_shared_image, resource_->sync_token(),
        client_shared_image->alpha_type(), resource_->GetHdrMetadata(),
        resource_si->context_provider_wrapper_, owning_thread_ref_,
        owning_thread_task_runner_, std::move(release_callback));

    DCHECK(image);
    return image;
  }
}

void ExportedCanvasResource::EndDisplayCompositorAccess(
    gpu::SharedImageExportResult export_result,
    bool is_lost) {
  auto sync_token =
      resource_->GetSharedImage()->EndExport(std::move(export_result));
  EndDisplayCompositorAccess(sync_token, is_lost);
}

void ExportedCanvasResource::EndDisplayCompositorAccess(
    gpu::SyncToken sync_token,
    bool is_lost) {
  resource_->WaitSyncToken(sync_token);
  if (is_lost) {
    resource_->NotifyResourceLost();
  }
}

viz::TransferableResource
ExportedCanvasResource::PrepareTransferableResource() {
  TRACE_EVENT0("blink", "CanvasResource::PrepareTransferableResource");

  auto resource = viz::TransferableResource::Make(
      resource_->GetSharedImage(), resource_->GetTransferableResourceSource(),
      resource_->sync_token());

  resource.hdr_metadata = resource_->GetHdrMetadata();

  // When the compositor returns an accelerated resource, it provides a sync
  // token to allow subsequent accelerated raster operations to properly
  // sequence their usage of the resource within the GPU service. However, if
  // the compositor is accelerated but raster is *not*, sync tokens are not
  // sufficient for ordering: We must instead ensure that the compositor's
  // GPU-side reads have actually *completed* before the compositor returns the
  // resource for reuse, as after that point subsequent raster operations will
  // start writing to the resource via the CPU without any subsequent
  // synchronization with the GPU service.
  if (!resource_->UsesAcceleratedRaster() &&
      resource_->CreatesAcceleratedTransferableResources()) {
    resource.synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }

  return resource;
}

void ExportedCanvasResource::SetOriginClean(bool clean) {
  origin_clean_ = clean;
}

}  // namespace blink
