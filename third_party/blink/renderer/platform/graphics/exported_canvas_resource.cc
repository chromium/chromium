// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

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

  scoped_refptr<StaticBitmapImage> Bitmap() override;
  const gfx::HDRMetadata& GetHdrMetadata() const override {
    return hdr_metadata_;
  }

 private:
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

scoped_refptr<StaticBitmapImage> ExternalCanvasResource::Bitmap() {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Bitmap");

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
      [](scoped_refptr<ExternalCanvasResource> resource,
         const gpu::SyncToken& sync_token, bool is_lost) {
        // Do nothing but hold onto the refptr.
      },
      base::RetainedRef(this));

  scoped_refptr<StaticBitmapImage> image =
      AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
          GetSharedImage(), sync_token(), alpha_type_, hdr_metadata_,
          context_provider_wrapper_, owning_thread_ref_,
          owning_thread_task_runner_, std::move(release_callback));
  return image;
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
                                                       provider)) {
  CHECK(resource_);
  CHECK(!resource_->is_cross_thread());
}

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<CanvasResourceSharedImage> resource)
    : resource_(std::move(resource)) {
  CHECK(resource_);
  CHECK(!resource_->is_cross_thread());
}

ExportedCanvasResource::~ExportedCanvasResource() {
  // We PostTask ExportedCanvasResource to the CanvasResource owning thread, but
  // if the thread gets destroyed before, destructor will still run, but we
  // can't tell CanvasResource to recycle.
  if (!resource_->is_cross_thread()) {
    CanvasResource::DropRefOnOwningThread(std::move(resource_));
  }
}

// static
void ExportedCanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<ExportedCanvasResource>&& exported_resource) {
  if (!exported_resource) {
    return;
  }

  if (exported_resource->resource_->is_cross_thread()) {
    auto& owning_thread_task_runner =
        exported_resource->resource_->owning_thread_task_runner_;
    owning_thread_task_runner->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(exported_resource)));
  }
}

gfx::Size ExportedCanvasResource::Size() const {
  return resource_->GetSharedImage()->size();
}

bool ExportedCanvasResource::OriginClean() const {
  return resource_->OriginClean();
}

scoped_refptr<StaticBitmapImage> ExportedCanvasResource::Bitmap() {
  return resource_->Bitmap();
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
  resource_->SetOriginClean(clean);
}

}  // namespace blink
