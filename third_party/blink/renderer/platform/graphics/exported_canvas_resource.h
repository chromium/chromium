// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class CanvasResource;
class CanvasResourceSharedImage;
class StaticBitmapImage;

class PLATFORM_EXPORT ExportedCanvasResource
    : public ThreadSafeRefCounted<ExportedCanvasResource> {
 public:
  explicit ExportedCanvasResource(
      scoped_refptr<CanvasResourceSharedImage> resource,
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const gpu::SyncToken& sync_token,
      viz::TransferableResource::ResourceSource resource_source,
      const gfx::HDRMetadata& hdr_metadata,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> provider,
      bool accelerated_raster);

  explicit ExportedCanvasResource(
      scoped_refptr<gpu::ClientSharedImage> client_si,
      const gpu::SyncToken& sync_token,
      viz::TransferableResource::ResourceSource resource_source,
      const gfx::HDRMetadata& hdr_metadata,
      viz::ReleaseCallback callback,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> provider);
  ~ExportedCanvasResource();

  static void OnPlaceholderReleasedResource(
      scoped_refptr<ExportedCanvasResource>&& resource);

  gfx::Size Size() const;
  bool OriginClean() const;
  void SetOriginClean(bool clean);
  scoped_refptr<StaticBitmapImage> Bitmap();
  void EndDisplayCompositorAccess(gpu::SharedImageExportResult export_result,
                                  bool is_lost);
  void EndDisplayCompositorAccess(gpu::SyncToken export_result, bool is_lost);
  viz::TransferableResource PrepareTransferableResource();

  CanvasResource* GetResourceForTesting() const { return resource_.get(); }
  scoped_refptr<gpu::ClientSharedImage> GetSharedImageForTesting() {
    return GetResourceForTesting()->GetSharedImage();
  }

 private:
  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != owning_thread_ref_;
  }

  bool origin_clean_ = true;
  bool is_lost_ = false;

  scoped_refptr<CanvasResource> resource_;
  const base::PlatformThreadRef owning_thread_ref_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

  const scoped_refptr<gpu::ClientSharedImage> shared_image_;
  const gpu::SyncToken sync_token_;
  const gfx::HDRMetadata hdr_metadata_;
  const viz::TransferableResource::ResourceSource resource_source_;
  const bool accelerated_raster_;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

  const bool use_legacy_external_canvas_resource_bitmap_path_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_
