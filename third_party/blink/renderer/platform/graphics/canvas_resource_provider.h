// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkSurface.h"

class GrContext;
class SkCanvas;

namespace cc {
class ImageDecodeCache;
class PaintCanvas;
}

namespace gpu {
namespace gles2 {

class GLES2Interface;

}  // namespace gles2
}  // namespace gpu

namespace blink {

class CanvasResourceDispatcher;
class WebGraphicsContext3DProviderWrapper;

// CanvasResourceProvider
//==============================================================================
//
// This is an abstract base class that encapsulates a drawable graphics
// resource.  Subclasses manage specific resource types (Gpu Textures,
// GpuMemoryBuffer, Bitmap in RAM). CanvasResourceProvider serves as an
// abstraction layer for these resource types. It is designed to serve
// the needs of Canvas2DLayerBridge, but can also be used as a general purpose
// provider of drawable surfaces for 2D rendering with skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

class PLATFORM_EXPORT CanvasResourceProvider
    : public WebGraphicsContext3DProviderWrapper::DestructionObserver {

 public:
  enum ResourceUsage {
    kSoftwareResourceUsage,
    kSoftwareCompositedResourceUsage,
    kAcceleratedResourceUsage,
    kAcceleratedCompositedResourceUsage,
  };

  enum PresentationMode {
    kDefaultPresentationMode,            // GPU Texture or shared memory bitmap
    kAllowImageChromiumPresentationMode  // Use CHROMIUM_image gl extension
  };

  enum ResourceProviderType {
    kTexture = 0,
    kBitmap = 1,
    kSharedBitmap = 2,
    kMaxValue = kSharedBitmap,
  };

  void static RecordTypeToUMA(ResourceProviderType type);

  static std::unique_ptr<CanvasResourceProvider> Create(
      const IntSize&,
      ResourceUsage,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      unsigned msaa_sample_count,
      const CanvasColorParams&,
      PresentationMode,
      base::WeakPtr<CanvasResourceDispatcher>,
      bool is_origin_top_left = true);

  // Use this method for capturing a frame that is intended to be displayed via
  // the compositor. Cases that need to acquire a snaptshot that is not destined
  // to be transfered via TransferableResource should call Snapshot() instead.
  virtual scoped_refptr<CanvasResource> ProduceFrame() = 0;
  scoped_refptr<StaticBitmapImage> Snapshot();

  // WebGraphicsContext3DProvider::DestructionObserver implementation.
  void OnContextDestroyed() override;

  cc::PaintCanvas* Canvas();
  void ReleaseLockedImages();
  void FlushSkia() const;
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void SetFilterQuality(SkFilterQuality quality) { filter_quality_ = quality; }
  const IntSize& Size() const { return size_; }
  virtual bool IsValid() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool SupportsDirectCompositing() const = 0;
  virtual bool SupportsSingleBuffering() const { return false; }
  uint32_t ContentUniqueID() const;
  CanvasResourceDispatcher* ResourceDispatcher() {
    return resource_dispatcher_.get();
  }

  // Indicates that the compositing path is single buffered, meaning that
  // ProduceFrame() return a reference to the same resource each time, which
  // implies that Producing an animation frame may overwrite the resource used
  // by the previous frame. This results in graphics updates skipping the
  // queue, thus reducing latency, but with the possible side effects of
  // tearring (in cases where the resource is scanned out directly) and
  // irregular frame rate.
  bool IsSingleBuffered() { return is_single_buffered_; }

  // Attempt to enable single buffering mode on this resource provider.  May
  // fail if the CanvasResourcePRovider subclass does not support this mode of
  // operation.
  void TryEnableSingleBuffering();

  void RecycleResource(scoped_refptr<CanvasResource>);
  void SetResourceRecyclingEnabled(bool);
  void ClearRecycledResources();
  scoped_refptr<CanvasResource> NewOrRecycledResource();

  SkSurface* GetSkSurface() const;
  bool IsGpuContextLost() const;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);
  virtual GLuint GetBackingTextureHandleForOverwrite() {
    NOTREACHED();
    return 0;
  }
  virtual void* GetPixelBufferAddressForOverwrite() {
    NOTREACHED();
    return nullptr;
  }
  void Clear();
  ~CanvasResourceProvider() override;

 protected:
  gpu::gles2::GLES2Interface* ContextGL() const;
  GrContext* GetGrContext() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper() {
    return context_provider_wrapper_;
  }
  SkFilterQuality FilterQuality() const { return filter_quality_; }
  base::WeakPtr<CanvasResourceProvider> CreateWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Called by subclasses when the backing resource has changed and resources
  // are not managed by skia, signaling that a new surface needs to be created.
  void InvalidateSurface();

  CanvasResourceProvider(const IntSize&,
                         const CanvasColorParams&,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceDispatcher>);

  // Its important to use this method for generating PaintImage wrapped canvas
  // snapshots to get a cache hit from cc's ImageDecodeCache. This method
  // ensures that the PaintImage ID for the snapshot, used for keying
  // decodes/uploads in the cache is invalidated only when the canvas contents
  // change.
  cc::PaintImage MakeImageSnapshot();

 private:
  class CanvasImageProvider : public cc::ImageProvider {
   public:
    CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                        cc::ImageDecodeCache* cache_f16,
                        const gfx::ColorSpace& target_color_space,
                        SkColorType target_color_type);
    ~CanvasImageProvider() override;

    // cc::ImageProvider implementation.
    ScopedDecodedDrawImage GetDecodedDrawImage(const cc::DrawImage&) override;

    void ReleaseLockedImages();

   private:
    void CanUnlockImage(ScopedDecodedDrawImage);
    void CleanupLockedImages();

    bool cleanup_task_pending_ = false;
    std::vector<ScopedDecodedDrawImage> locked_images_;
    cc::PlaybackImageProvider playback_image_provider_n32_;
    base::Optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

    base::WeakPtrFactory<CanvasImageProvider> weak_factory_;
  };

  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;
  virtual scoped_refptr<CanvasResource> CreateResource();
  cc::ImageDecodeCache* ImageDecodeCache();
  cc::ImageDecodeCache* ImageDecodeCacheF16();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher_;
  IntSize size_;
  CanvasColorParams color_params_;
  base::Optional<CanvasImageProvider> canvas_image_provider_;
  std::unique_ptr<cc::SkiaPaintCanvas> canvas_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init
  std::unique_ptr<SkCanvas> xform_canvas_;
  SkFilterQuality filter_quality_ = kLow_SkFilterQuality;

  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  WTF::Vector<scoped_refptr<CanvasResource>> recycled_resources_;
  bool resource_recycling_enabled_ = true;

  bool is_single_buffered_ = false;
  scoped_refptr<CanvasResource> single_buffer_;

  base::WeakPtrFactory<CanvasResourceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasResourceProvider);
};

}  // namespace blink

#endif
