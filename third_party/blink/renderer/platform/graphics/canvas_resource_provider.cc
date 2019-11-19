// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

// * Renders to a Skia RAM-backed bitmap.
// * Mailboxing is not supported : cannot be directly composited.
class CanvasResourceProviderBitmap : public CanvasResourceProvider {
 public:
  CanvasResourceProviderBitmap(
      const IntSize& size,
      SkFilterQuality filter_quality,
      const CanvasColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(kBitmap,
                               size,
                               /*msaa_sample_count=*/0,
                               filter_quality,
                               color_params,
                               /*is_origin_top_left=*/true,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderBitmap() override = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }
  bool SupportsDirectCompositing() const override { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    return nullptr;  // Does not support direct compositing
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::Snapshot");
    return SnapshotInternal();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::CreateSkSurface");

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRaster(info, ColorParams().GetSkSurfaceProps());
  }
};

// * Renders to a shared memory bitmap.
// * Uses SharedBitmaps to pass frames directly to the compositor.
class CanvasResourceProviderSharedBitmap : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderSharedBitmap(
      const IntSize& size,
      SkFilterQuality filter_quality,
      const CanvasColorParams& color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     filter_quality,
                                     color_params,
                                     nullptr,  // context_provider_wrapper
                                     std::move(resource_dispatcher)) {
    DCHECK(ResourceDispatcher());
    type_ = kSharedBitmap;
  }
  ~CanvasResourceProviderSharedBitmap() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    CanvasColorParams color_params = ColorParams();
    if (!IsBitmapFormatSupported(color_params.TransferableResourceFormat())) {
      // If the rendering format is not supported, downgrate to 8-bits.
      // TODO(junov): Should we try 12-12-12-12 and 10-10-10-2?
      color_params.SetCanvasPixelFormat(kRGBA8CanvasPixelFormat);
    }

    return CanvasResourceSharedBitmap::Create(Size(), color_params,
                                              CreateWeakPtr(), FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource)
      return nullptr;

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.GetSkImage()->isTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSkImage());

    return output_resource;
  }
};

// * Renders to a SharedImage, which manages memory internally.
// * Layers are overlay candidates.
class CanvasResourceProviderSharedImage : public CanvasResourceProvider {
 public:
  CanvasResourceProviderSharedImage(
      const IntSize& size,
      unsigned msaa_sample_count,
      SkFilterQuality filter_quality,
      const CanvasColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left,
      bool is_overlay_candidate,
      bool maybe_single_buffered,
      bool is_accelerated)
      : CanvasResourceProvider(
            kSharedImage,
            size,
            msaa_sample_count,
            filter_quality,
            // TODO(khushalsagar): The software path seems to be assuming N32
            // somewhere in the later pipeline but for offscreen canvas only.
            CanvasColorParams(color_params.ColorSpace(),
                              color_params.PixelFormat(),
                              color_params.GetOpacityMode(),
                              is_accelerated ? CanvasForceRGBA::kForced
                                             : CanvasForceRGBA::kNotForced),
            is_origin_top_left,
            std::move(context_provider_wrapper),
            std::move(resource_dispatcher)),
        is_overlay_candidate_(is_overlay_candidate),
        maybe_single_buffered_(maybe_single_buffered),
        is_accelerated_(is_accelerated) {
    resource_ = NewOrRecycledResource();
    if (resource_)
      EnsureWriteAccess();
  }

  ~CanvasResourceProviderSharedImage() override {}

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return is_accelerated_; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override {
    return maybe_single_buffered_;
  }
  GLuint GetBackingTextureHandleForOverwrite() override {
    DCHECK(is_accelerated_);

    if (IsGpuContextLost())
      return 0u;

    FlushGrContext();
    WillDraw();
    return resource()->GetTextureIdForWriteAccess();
  }
  GLenum GetBackingTextureTarget() const override {
    return resource_->TextureTarget();
  }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateResource");
    if (IsGpuContextLost())
      return nullptr;

    bool allow_concurrent_read_write_access = maybe_single_buffered_;
    return CanvasResourceSharedImage::Create(
        Size(), ContextProviderWrapper(), CreateWeakPtr(), FilterQuality(),
        ColorParams(), is_overlay_candidate_, IsOriginTopLeft(),
        allow_concurrent_read_write_access, is_accelerated_);
  }

  void NotifyTexParamsModified(const CanvasResource* resource) override {
    if (!is_accelerated_)
      return;

    if (resource_.get() == resource) {
      DCHECK(!current_resource_has_write_access_);
      // Note that the call below is guarenteed to not issue any GPU work for
      // the backend texture since we ensure that all skia work on the resource
      // is issued before releasing write access.
      surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess)
          .glTextureParametersModified();
    }
  }

 protected:
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderSharedImage::ProduceCanvasResource");
    if (IsGpuContextLost())
      return nullptr;

    // Its important to end read access and ref the resource before the WillDraw
    // call below. Since it relies on resource ref-count to trigger
    // copy-on-write and asserts that we only have write access when the
    // provider has the only ref to the resource, to ensure there are no other
    // readers.
    EndWriteAccess();
    scoped_refptr<CanvasResource> resource = resource_;
    resource->SetFilterQuality(FilterQuality());
    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters. This issue is Android-WebView specific: crbug.com/585250.
      WillDraw();
    }

    return resource;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::Snapshot");
    if (!IsValid())
      return nullptr;

    // We don't need to EndWriteAccess here since that's required to make the
    // rendering results visible on the GpuMemoryBuffer while we return cpu
    // memory, rendererd to by skia, here.
    if (!is_accelerated_)
      return SnapshotInternal();

    if (!cached_snapshot_) {
      EndWriteAccess();
      cached_snapshot_ = resource_->Bitmap();
    }

    DCHECK(cached_snapshot_);
    DCHECK(!current_resource_has_write_access_);
    return cached_snapshot_;
  }

  void WillDraw() override {
    DCHECK(resource_);

    if (IsGpuContextLost())
      return;

    // Since the resource will be updated, the cached snapshot is no longer
    // valid. Note that it is important to release this reference here to not
    // trigger copy-on-write below from the resource ref in the snapshot.
    // Note that this is valid for single buffered mode also, since while the
    // resource/mailbox remains the same, the snapshot needs an updated sync
    // token for these writes.
    cached_snapshot_.reset();

    // We don't need to do copy-on-write for the resource here since writes to
    // the GMB are deferred until it needs to be dispatched to the display
    // compositor via ProduceCanvasResource.
    if (is_accelerated_ && DoCopyOnWrite()) {
      DCHECK(!current_resource_has_write_access_)
          << "Write access must be released before sharing the resource";

      auto old_resource = std::move(resource_);
      auto* old_resource_shared_image =
          static_cast<CanvasResourceSharedImage*>(old_resource.get());
      resource_ = NewOrRecycledResource();
      DCHECK(resource_);

      EnsureWriteAccess();
      if (surface_) {
        // Take read access to the outgoing resource for the skia copy below.
        if (!old_resource_shared_image->has_read_access()) {
          ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
              old_resource_shared_image->GetTextureIdForReadAccess(),
              GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
        }
        surface_->replaceBackendTexture(CreateGrTextureForResource(),
                                        GetGrSurfaceOrigin());
        surface_->flush();
        if (!old_resource_shared_image->has_read_access()) {
          ContextGL()->EndSharedImageAccessDirectCHROMIUM(
              old_resource_shared_image->GetTextureIdForReadAccess());
        }
      }
    }

    EnsureWriteAccess();
    resource()->WillDraw();
  }

  bool DoCopyOnWrite() {
    // If the canvas is single buffered, concurrent read/writes to the resource
    // are allowed. Note that we ignore the resource lost case as well since
    // that only indicates that we did not get a sync token for read/write
    // synchronization which is not a requirement for single buffered canvas.
    if (IsSingleBuffered())
      return false;

    // If the resource was lost, we can not use it for writes again.
    if (resource()->is_lost())
      return true;

    // We have the only ref to the resource which implies there are no active
    // readers.
    if (resource_->HasOneRef())
      return false;

    // Its possible to have deferred work in skia which uses this resource. Try
    // flushing once to see if that releases the read refs. We can avoid a copy
    // by queuing this work before writing to this resource.
    if (is_accelerated_)
      surface_->flush();

    return !resource_->HasOneRef();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateSkSurface");
    if (IsGpuContextLost() || !resource_)
      return nullptr;

    if (is_accelerated_) {
      return SkSurface::MakeFromBackendTexture(
          GetGrContext(), CreateGrTextureForResource(), GetGrSurfaceOrigin(),
          GetMSAASampleCount(), ColorParams().GetSkColorType(),
          ColorParams().GetSkColorSpaceForSkSurfaces(),
          ColorParams().GetSkSurfaceProps());
    }

    // For software raster path, we render into cpu memory managed internally
    // by SkSurface and copy the rendered results to the GMB before dispatching
    // it to the display compositor.
    return SkSurface::MakeRaster(resource_->CreateSkImageInfo(),
                                 ColorParams().GetSkSurfaceProps());
  }

  GrBackendTexture CreateGrTextureForResource() const {
    DCHECK(is_accelerated_);

    GrGLTextureInfo texture_info = {};
    texture_info.fID = resource()->GetTextureIdForWriteAccess();
    texture_info.fTarget = resource_->TextureTarget();
    texture_info.fFormat = ColorParams().GLSizedInternalFormat();
    return GrBackendTexture(Size().Width(), Size().Height(), GrMipMapped::kNo,
                            texture_info);
  }

  void FlushGrContext() {
    DCHECK(is_accelerated_);

    // The resource may have been imported and used in skia. Make sure any
    // operations using this resource are flushed to the underlying context.
    // Note that its not sufficient to flush the SkSurface here since it will
    // only perform a GrContext flush if that SkSurface has any pending ops. And
    // this resource may be written to or read from skia without using the
    // SkSurface here.
    if (IsGpuContextLost())
      return;
    GetGrContext()->flush();
  }

  void EnsureWriteAccess() {
    DCHECK(resource_);
    // In software mode, we don't need write access to the resource during
    // drawing since it is executed on cpu memory managed by skia. We ensure
    // exclusive access to the resource when the results are copied onto the
    // GMB in EndWriteAccess.
    DCHECK(resource_->HasOneRef() || IsSingleBuffered() || !is_accelerated_)
        << "Write access requires exclusive access to the resource";
    DCHECK(!resource()->is_cross_thread())
        << "Write access is only allowed on the owning thread";

    if (current_resource_has_write_access_ || IsGpuContextLost())
      return;

    if (is_accelerated_) {
      auto texture_id = resource()->GetTextureIdForWriteAccess();
      ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
          texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    }

    // For the non-accelerated path, we don't need a texture for writes since
    // its on the CPU, but we set this bit to know whether the GMB needs to be
    // updated.
    current_resource_has_write_access_ = true;
  }

  void EndWriteAccess() {
    DCHECK(!resource()->is_cross_thread());

    if (!current_resource_has_write_access_ || IsGpuContextLost())
      return;

    if (is_accelerated_) {
      // Issue any skia work using this resource before releasing write access.
      FlushGrContext();
      auto texture_id = resource()->GetTextureIdForWriteAccess();
      ContextGL()->EndSharedImageAccessDirectCHROMIUM(texture_id);
    } else {
      if (DoCopyOnWrite())
        resource_ = NewOrRecycledResource();
      resource()->CopyRenderingResultsToGpuMemoryBuffer(
          surface_->makeImageSnapshot());
    }

    current_resource_has_write_access_ = false;
  }

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

  const bool is_overlay_candidate_;
  const bool maybe_single_buffered_;
  const bool is_accelerated_;
  bool current_resource_has_write_access_ = false;
  scoped_refptr<CanvasResource> resource_;
  scoped_refptr<StaticBitmapImage> cached_snapshot_;
};

// This class does nothing except answering to ProduceCanvasResource() by piping
// it to NewOrRecycledResource().  This ResourceProvider is meant to be used
// with an imported external CanvasResource, and all drawing and lifetime logic
// must be kept at a higher level.
class CanvasResourceProviderPassThrough final : public CanvasResourceProvider {
 public:
  CanvasResourceProviderPassThrough(
      const IntSize& size,
      SkFilterQuality filter_quality,
      const CanvasColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(kPassThrough,
                               size,
                               /*msaa_sample_count=*/0,
                               filter_quality,
                               color_params,
                               /*is_origin_top_left=*/true,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderPassThrough() override = default;
  bool IsValid() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    // This class has no CanvasResource to provide: this must be imported via
    // ImportResource() and kept in the parent class.
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    return NewOrRecycledResource();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    NOTREACHED();
    return nullptr;
  }
};

// * Renders to back buffer of a shared image swap chain.
// * Presents swap chain and exports front buffer mailbox to compositor to
//   support low latency mode.
// * Layers are overlay candidates.
class CanvasResourceProviderSwapChain final : public CanvasResourceProvider {
 public:
  CanvasResourceProviderSwapChain(
      const IntSize& size,
      unsigned msaa_sample_count,
      SkFilterQuality filter_quality,
      const CanvasColorParams& color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(kSwapChain,
                               size,
                               msaa_sample_count,
                               filter_quality,
                               color_params,
                               /*is_origin_top_left=*/true,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count) {
    resource_ = CanvasResourceSwapChain::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality());
  }
  ~CanvasResourceProviderSwapChain() override = default;

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  void WillDraw() override { dirty_ = true; }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::CreateResource");
    return resource_;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    DCHECK(IsSingleBuffered());
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderSwapChain::ProduceCanvasResource");
    if (!IsValid())
      return nullptr;
    if (dirty_) {
      FlushSkia();
      resource_->PresentSwapChain();
      dirty_ = false;
    }
    return resource_;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::Snapshot");
    return SnapshotInternal();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSwapChain::CreateSkSurface");
    if (IsGpuContextLost() || !resource_)
      return nullptr;

    DCHECK(resource_);
    GrGLTextureInfo texture_info = {};
    texture_info.fID = resource_->GetBackingTextureHandleForOverwrite();
    texture_info.fTarget = resource_->TextureTarget();
    texture_info.fFormat = ColorParams().GLSizedInternalFormat();

    auto backend_texture = GrBackendTexture(Size().Width(), Size().Height(),
                                            GrMipMapped::kNo, texture_info);

    return SkSurface::MakeFromBackendTextureAsRenderTarget(
        GetGrContext(), backend_texture, kTopLeft_GrSurfaceOrigin,
        msaa_sample_count_, ColorParams().GetSkColorType(),
        ColorParams().GetSkColorSpaceForSkSurfaces(),
        ColorParams().GetSkSurfaceProps());
  }

  const unsigned msaa_sample_count_;
  bool dirty_ = false;
  scoped_refptr<CanvasResourceSwapChain> resource_;
};

namespace {

enum class CanvasResourceType {
  kDirect3DSwapChain,
  kDirect2DSwapChain,
  kDirect3DGpuMemoryBuffer,
  kSharedBitmap,
  kBitmap,
  kSharedImage,
};

const Vector<CanvasResourceType>& GetResourceTypeFallbackList(
    CanvasResourceProvider::ResourceUsage usage) {
  static const Vector<CanvasResourceType> kSoftwareFallbackList({
      CanvasResourceType::kBitmap,
  });

  static const Vector<CanvasResourceType> kAcceleratedFallbackList({
      CanvasResourceType::kSharedImage,
      // Fallback to software
      CanvasResourceType::kBitmap,
  });

  static const Vector<CanvasResourceType> kCompositedFallbackList({
      CanvasResourceType::kSharedImage,
      CanvasResourceType::kSharedBitmap,
      // Fallback to no direct compositing support
      CanvasResourceType::kBitmap,
  });

  static const Vector<CanvasResourceType> kAcceleratedDirect2DFallbackList({
      // Needed for low latency canvas on Windows.
      CanvasResourceType::kDirect2DSwapChain,
      // The rest is equal to |kCompositedFallbackList|.
      CanvasResourceType::kSharedImage,
      CanvasResourceType::kSharedBitmap,
      CanvasResourceType::kBitmap,
  });
  DCHECK(std::equal(kAcceleratedDirect2DFallbackList.begin() + 1,
                    kAcceleratedDirect2DFallbackList.end(),
                    kCompositedFallbackList.begin(),
                    kCompositedFallbackList.end()));

  static const Vector<CanvasResourceType> kAcceleratedDirect3DFallbackList({
      // This is used with single-buffered WebGL where the resource comes
      // from an external source. The external site should take care of
      // using SharedImages since the resource will be used by the display
      // compositor.
      CanvasResourceType::kDirect3DSwapChain,
      CanvasResourceType::kDirect3DGpuMemoryBuffer,
      // The rest is equal to |kCompositedFallbackList|.
      CanvasResourceType::kSharedImage,
      CanvasResourceType::kSharedBitmap,
      CanvasResourceType::kBitmap,
  });
  DCHECK(std::equal(kAcceleratedDirect3DFallbackList.begin() + 2,
                    kAcceleratedDirect3DFallbackList.end(),
                    kCompositedFallbackList.begin(),
                    kCompositedFallbackList.end()));

  switch (usage) {
    case CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage:
      return kSoftwareFallbackList;
    case CanvasResourceProvider::ResourceUsage::
        kSoftwareCompositedResourceUsage:
      return kCompositedFallbackList;
    case CanvasResourceProvider::ResourceUsage::kAcceleratedResourceUsage:
      return kAcceleratedFallbackList;
    case CanvasResourceProvider::ResourceUsage::
        kAcceleratedCompositedResourceUsage:
      return kCompositedFallbackList;
    case CanvasResourceProvider::ResourceUsage::
        kAcceleratedDirect2DResourceUsage:
      return kAcceleratedDirect2DFallbackList;
    case CanvasResourceProvider::ResourceUsage::
        kAcceleratedDirect3DResourceUsage:
      return kAcceleratedDirect3DFallbackList;
    case CanvasResourceProvider::ResourceUsage::
        kSoftwareCompositedDirect2DResourceUsage:
      return kCompositedFallbackList;
  }
  NOTREACHED();
}

}  // unnamed namespace

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::CreateForCanvas(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    uint8_t presentation_mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left) {
  base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderUsage", usage);

  std::unique_ptr<CanvasResourceProvider> provider = Create(
      size, usage, context_provider_wrapper, msaa_sample_count, filter_quality,
      color_params, presentation_mode, resource_dispatcher, is_origin_top_left);

  if (provider && provider->IsValid()) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              provider->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  provider->type_);
  }

  return provider;
}

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::Create(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    uint8_t presentation_mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left) {
  std::unique_ptr<CanvasResourceProvider> provider;

  if (context_provider_wrapper) {
    const int max_texture_size = context_provider_wrapper->ContextProvider()
                                     ->GetCapabilities()
                                     .max_texture_size;
    if (size.Width() > max_texture_size || size.Height() > max_texture_size)
      usage = ResourceUsage::kSoftwareResourceUsage;
  }

  const Vector<CanvasResourceType>& fallback_list =
      GetResourceTypeFallbackList(usage);

#if defined(OS_ANDROID)
  // TODO(khushalsagar): Re-enable these if we're using SurfaceControl and
  // GMBs allow us to overlay these resources.
  const bool is_gpu_memory_buffer_image_allowed = false;
#else
  const bool is_gpu_memory_buffer_image_allowed =
      SharedGpuContext::IsGpuCompositingEnabled() && context_provider_wrapper &&
      (presentation_mode & kAllowImageChromiumPresentationMode) &&
      gpu::IsImageSizeValidForGpuMemoryBufferFormat(
          gfx::Size(size), color_params.GetBufferFormat()) &&
      gpu::IsImageFromGpuMemoryBufferFormatSupported(
          color_params.GetBufferFormat(),
          context_provider_wrapper->ContextProvider()->GetCapabilities());
#endif

  const bool is_swap_chain_allowed =
      SharedGpuContext::IsGpuCompositingEnabled() && context_provider_wrapper &&
      context_provider_wrapper->ContextProvider()
          ->GetCapabilities()
          .shared_image_swap_chain &&
      (presentation_mode & kAllowSwapChainPresentationMode);

  for (CanvasResourceType resource_type : fallback_list) {
    // Note: We are deliberately not using std::move() on
    // |context_provider_wrapper| and |resource_dispatcher| to ensure that the
    // pointers remain valid for the next iteration of this loop if necessary.
    switch (resource_type) {
      case CanvasResourceType::kDirect2DSwapChain:
        if (!is_swap_chain_allowed)
          continue;
        DCHECK(is_origin_top_left);
        provider = std::make_unique<CanvasResourceProviderSwapChain>(
            size, msaa_sample_count, filter_quality, color_params,
            context_provider_wrapper, resource_dispatcher);
        break;
      case CanvasResourceType::kDirect3DSwapChain:
        if (!is_swap_chain_allowed)
          continue;
        provider = std::make_unique<CanvasResourceProviderPassThrough>(
            size, filter_quality, color_params, context_provider_wrapper,
            resource_dispatcher);
        break;
      case CanvasResourceType::kDirect3DGpuMemoryBuffer:
        if (!is_gpu_memory_buffer_image_allowed)
          continue;
        provider = std::make_unique<CanvasResourceProviderPassThrough>(
            size, filter_quality, color_params, context_provider_wrapper,
            resource_dispatcher);
        break;
      case CanvasResourceType::kSharedBitmap:
        if (!resource_dispatcher)
          continue;
        provider = std::make_unique<CanvasResourceProviderSharedBitmap>(
            size, filter_quality, color_params, resource_dispatcher);
        break;
      case CanvasResourceType::kBitmap:
        provider = std::make_unique<CanvasResourceProviderBitmap>(
            size, filter_quality, color_params, context_provider_wrapper,
            resource_dispatcher);
        break;
      case CanvasResourceType::kSharedImage: {
        DCHECK_NE(usage, ResourceUsage::kSoftwareResourceUsage);

        if (!context_provider_wrapper)
          continue;

        const bool usage_wants_single_buffered =
            usage == ResourceUsage::kAcceleratedDirect2DResourceUsage ||
            usage == ResourceUsage::kAcceleratedDirect3DResourceUsage ||
            usage == ResourceUsage::kSoftwareCompositedDirect2DResourceUsage;
        const bool usage_wants_overlays =
            usage_wants_single_buffered ||
            usage == ResourceUsage::kAcceleratedCompositedResourceUsage ||
            usage == ResourceUsage::kSoftwareCompositedResourceUsage;

        // texture_storage_image is required to create shared images supporting
        // scanout usage.
        const bool can_use_overlays =
            is_gpu_memory_buffer_image_allowed &&
            context_provider_wrapper->ContextProvider()
                ->GetCapabilities()
                .texture_storage_image;

        const bool can_use_gmbs =
            is_gpu_memory_buffer_image_allowed &&
            Platform::Current()->GetGpuMemoryBufferManager();

        const bool is_overlay_candidate =
            usage_wants_overlays && can_use_overlays;
        const bool is_accelerated =
            usage == ResourceUsage::kAcceleratedResourceUsage ||
            usage == ResourceUsage::kAcceleratedCompositedResourceUsage ||
            usage == ResourceUsage::kAcceleratedDirect2DResourceUsage ||
            usage == ResourceUsage::kAcceleratedDirect3DResourceUsage;

        // If the rendering will be in software and we don't have GMB support,
        // fallback to bitmap provider type.
        if (!is_accelerated && !can_use_gmbs)
          continue;

        // TODO(khushalsagar) Single buffering requires concurrent
        // read/write access to the resource which is sub-optimal for software
        // raster since that would require concurrent access to the resource on
        // the CPU and GPU. Check if we should disable it in software mode.
        const bool maybe_single_buffered =
            usage_wants_single_buffered && can_use_gmbs;

        provider = std::make_unique<CanvasResourceProviderSharedImage>(
            size, msaa_sample_count, filter_quality, color_params,
            context_provider_wrapper, resource_dispatcher, is_origin_top_left,
            is_overlay_candidate, maybe_single_buffered, is_accelerated);
      } break;
    }
    if (!provider->IsValid())
      continue;
    return provider;
  }

  return nullptr;
}

class CanvasResourceProvider::CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      const gfx::ColorSpace& target_color_space,
                      SkColorType target_color_type,
                      bool is_hardware_decode_cache);
  ~CanvasImageProvider() override = default;

  // cc::ImageProvider implementation.
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }

 private:
  void CanUnlockImage(ScopedResult);
  void CleanupLockedImages();

  bool is_hardware_decode_cache_;
  bool cleanup_task_pending_ = false;
  Vector<ScopedResult> locked_images_;
  cc::PlaybackImageProvider playback_image_provider_n32_;
  base::Optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CanvasImageProvider);
};

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache_n32,
    cc::ImageDecodeCache* cache_f16,
    const gfx::ColorSpace& target_color_space,
    SkColorType canvas_color_type,
    bool is_hardware_decode_cache)
    : is_hardware_decode_cache_(is_hardware_decode_cache),
      playback_image_provider_n32_(cache_n32,
                                   target_color_space,
                                   cc::PlaybackImageProvider::Settings()) {
  // If the image provider may require to decode to half float instead of
  // uint8, create a f16 PlaybackImageProvider with the passed cache.
  if (canvas_color_type == kRGBA_F16_SkColorType) {
    DCHECK(cache_f16);
    playback_image_provider_f16_.emplace(cache_f16, target_color_space,
                                         cc::PlaybackImageProvider::Settings());
  }
}

cc::ImageProvider::ScopedResult
CanvasResourceProvider::CanvasImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  // TODO(xidachen): Ensure this function works for paint worklet generated
  // images.
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double decoding.
  ImageProvider::ScopedResult scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    DCHECK(playback_image_provider_f16_);
    scoped_decoded_image =
        playback_image_provider_f16_->GetRasterContent(draw_image);
  } else {
    scoped_decoded_image =
        playback_image_provider_n32_.GetRasterContent(draw_image);
  }

  // Holding onto locked images here is a performance optimization for the
  // gpu image decode cache.  For that cache, it is expensive to lock and
  // unlock gpu discardable, and so it is worth it to hold the lock on
  // these images across multiple potential decodes.  In the software case,
  // locking in this manner makes it easy to run out of discardable memory
  // (backed by shared memory sometimes) because each per-colorspace image
  // decode cache has its own limit.  In the software case, just unlock
  // immediately and let the discardable system manage the cache logic
  // behind the scenes.
  if (!scoped_decoded_image.needs_unlock() || !is_hardware_decode_cache_) {
    return scoped_decoded_image;
  }

  constexpr int kMaxLockedImagesCount = 500;
  if (!scoped_decoded_image.decoded_image().is_budgeted() ||
      locked_images_.size() > kMaxLockedImagesCount) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedResult(decoded_draw_image,
                      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(scoped_decoded_image)));
}

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedResult image) {
  // We should early out and avoid calling this function for software decodes.
  DCHECK(is_hardware_decode_cache_);

  // Because these image decodes are being done in javascript calling into
  // canvas code, there's no obvious time to do the cleanup.  To handle this,
  // post a cleanup task to run after javascript is done running.
  if (!cleanup_task_pending_) {
    cleanup_task_pending_ = true;
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CanvasImageProvider::CleanupLockedImages,
                                  weak_factory_.GetWeakPtr()));
  }

  locked_images_.push_back(std::move(image));
}

void CanvasResourceProvider::CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

CanvasResourceProvider::CanvasResourceProvider(
    const ResourceProviderType& type,
    const IntSize& size,
    unsigned msaa_sample_count,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    bool is_origin_top_left,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
    : type_(type),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_dispatcher_(resource_dispatcher),
      size_(size),
      msaa_sample_count_(msaa_sample_count),
      filter_quality_(filter_quality),
      color_params_(color_params),
      is_origin_top_left_(is_origin_top_left),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  if (context_provider_wrapper_)
    context_provider_wrapper_->AddObserver(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_)
    surface_ = CreateSkSurface();
  return surface_.get();
}

void CanvasResourceProvider::InitializePaintCanvas() {
  // Since canvas_ has a reference to canvas_image_provider_, canvas must be
  // deleted before the image_provider.
  canvas_ = nullptr;
  canvas_image_provider_ = nullptr;

  // Create an ImageDecodeCache for half float images only if the canvas is
  // using half float back storage.
  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (ColorParams().GetSkColorType() == kRGBA_F16_SkColorType)
    cache_f16 = ImageDecodeCacheF16();
  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      ImageDecodeCacheRGBA8(), cache_f16, gfx::ColorSpace::CreateSRGB(),
      color_params_.GetSkColorType(), use_hardware_decode_cache());

  cc::SkiaPaintCanvas::ContextFlushes context_flushes;
  if (IsAccelerated() &&
      !ContextProviderWrapper()
           ->ContextProvider()
           ->GetGpuFeatureInfo()
           .IsWorkaroundEnabled(gpu::DISABLE_2D_CANVAS_AUTO_FLUSH)) {
    context_flushes.enable =
        canvas_heuristic_parameters::kEnableGrContextFlushes;
    context_flushes.max_draws_before_flush =
        canvas_heuristic_parameters::kMaxDrawsBeforeContextFlush;
  }
  canvas_ = std::make_unique<cc::SkiaPaintCanvas>(GetSkSurface()->getCanvas(),
                                                  canvas_image_provider_.get(),
                                                  context_flushes);
}

cc::PaintCanvas* CanvasResourceProvider::Canvas() {
  WillDraw();

  if (!canvas_) {
    TRACE_EVENT0("blink", "CanvasResourceProvider::Canvas");
    DCHECK(!canvas_image_provider_);
    InitializePaintCanvas();
  }
  return canvas_.get();
}

void CanvasResourceProvider::OnContextDestroyed() {
  if (canvas_image_provider_) {
    DCHECK(canvas_);
    canvas_->reset_image_provider();
    canvas_image_provider_.reset();
  }
}

void CanvasResourceProvider::ReleaseLockedImages() {
  if (canvas_image_provider_)
    canvas_image_provider_->ReleaseLockedImages();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::SnapshotInternal() {
  if (!IsValid())
    return nullptr;

  auto paint_image = MakeImageSnapshot();
  if (paint_image.GetSkImage()->isTextureBacked() && ContextProviderWrapper()) {
    return StaticBitmapImage::Create(paint_image.GetSkImage(),
                                     ContextProviderWrapper());
  }
  return StaticBitmapImage::Create(std::move(paint_image));
}

cc::PaintImage CanvasResourceProvider::MakeImageSnapshot() {
  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (!sk_image)
    return cc::PaintImage();

  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  return PaintImageBuilder::WithDefault()
      .set_id(snapshot_paint_image_id_)
      .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
      .TakePaintImage();
}

gpu::gles2::GLES2Interface* CanvasResourceProvider::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

gpu::raster::RasterInterface* CanvasResourceProvider::RasterInterface() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->RasterInterface();
}

GrContext* CanvasResourceProvider::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

void CanvasResourceProvider::FlushSkia() const {
  GetSkSurface()->flush();
}

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "CanvasResourceProvider::WritePixels");

  DCHECK(IsValid());
  return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels, row_bytes,
                                                  x, y);
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  DCHECK(IsValid());
  if (color_params_.GetOpacityMode() == kOpaque)
    Canvas()->clear(SK_ColorBLACK);
  else
    Canvas()->clear(SK_ColorTRANSPARENT);
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheRGBA8() {

  if (use_hardware_decode_cache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kN32_SkColorType);
  }

  return &Image::SharedCCDecodeCache(kN32_SkColorType);
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheF16() {

  if (use_hardware_decode_cache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kRGBA_F16_SkColorType);
  }
  return &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource> resource) {
  // We don't want to keep an arbitrary large number of canvases.
  if (canvas_resources_.size() >
      canvas_heuristic_parameters::kMaxRecycledCanvasResources)
    return;

  // Need to check HasOneRef() because if there are outstanding references to
  // the resource, it cannot be safely recycled.
  if (resource->HasOneRef() && resource_recycling_enabled_ &&
      !is_single_buffered_) {
    canvas_resources_.push_back(std::move(resource));
  }
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

void CanvasResourceProvider::ClearRecycledResources() {
  canvas_resources_.clear();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (canvas_resources_.IsEmpty())
    canvas_resources_.push_back(CreateResource());

  if (IsSingleBuffered()) {
    DCHECK_EQ(canvas_resources_.size(), 1u);
    return canvas_resources_.back();
  }

  scoped_refptr<CanvasResource> resource = std::move(canvas_resources_.back());
  canvas_resources_.pop_back();
  return resource;
}

void CanvasResourceProvider::TryEnableSingleBuffering() {
  if (IsSingleBuffered() || !SupportsSingleBuffering())
    return;
  is_single_buffered_ = true;
  ClearRecycledResources();
}

bool CanvasResourceProvider::ImportResource(
    scoped_refptr<CanvasResource> resource) {
  if (!IsSingleBuffered() || !SupportsSingleBuffering())
    return false;
  canvas_resources_.clear();
  canvas_resources_.push_back(std::move(resource));
  return true;
}

}  // namespace blink
