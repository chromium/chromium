// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/byte_size.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_record.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/scoped_raster_timer.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class AnimatedImageFrameIndexMap;
class PaintCanvas;
class SkiaPaintCanvas;
}  // namespace cc

namespace gfx {
class ColorSpace;
struct HDRMetadata;
class Size;
}  // namespace gfx

namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event

namespace blink {

class CanvasImageProvider;
class CanvasRenderingContext2D;
class CanvasResourceProviderDelegate;
class OffscreenCanvasRenderingContext2D;

// Renders canvas2D ops to a Skia RAM-backed bitmap. Mailboxing is not
// supported : cannot be directly composited. For usage by (Offscreen)Canvas2D
// as a last-case resort when it is not possible to create
// Canvas2DResourceProvider.
class PLATFORM_EXPORT Canvas2DBitmapProvider final
    : public CanvasMemoryDumpClient,
      public MemoryManagedPaintRecorder::Client,
      public ScopedRasterTimer::Host,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver {
 public:
  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DBitmapProvider> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata,
      CanvasResourceProviderDelegate* delegate = nullptr);

  static std::unique_ptr<Canvas2DBitmapProvider> CreateForTesting(
      gfx::Size size,
      const Canvas2DColorParams& color_params);

  ~Canvas2DBitmapProvider() override;

  bool IsValid() const { return GetSkSurface(); }
  bool IsGpuContextLost() const { return true; }
  void SetDelegate(CanvasResourceProviderDelegate* delegate) {
    delegate_ = delegate;
  }
  scoped_refptr<StaticBitmapImage> Snapshot(
      ImageOrientation = ImageOrientationEnum::kDefault);
  void ReleaseImageProviderImages();

  void SetAnimatedImageFrameIndexes(
      scoped_refptr<const cc::AnimatedImageFrameIndexMap>);

  void RasterRecord(cc::PaintRecord last_recording);
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);

  viz::SharedImageFormat GetSharedImageFormat() const { return format_; }
  const gfx::ColorSpace& GetColorSpace() const { return color_space_; }
  const gfx::HDRMetadata& GetHdrMetadata() const { return hdr_metadata_; }
  SkAlphaType GetAlphaType() const { return alpha_type_; }
  gfx::Size Size() const { return size_; }
  base::ByteSize EstimatedSizeInBytes() const {
    return base::ByteSize(format_.EstimatedSizeInBytes(size_));
  }

  size_t max_recorded_op_bytes() const { return max_recorded_op_bytes_; }
  size_t max_pinned_image_bytes() const { return max_pinned_image_bytes_; }
  bool clear_frame() const { return clear_frame_; }
  void set_clear_frame(bool clear_frame) { clear_frame_ = clear_frame; }

  const MemoryManagedPaintRecorder& Recorder() const { return *recorder_; }
  MemoryManagedPaintRecorder& Recorder() { return *recorder_; }
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder();
  void SetRecorder(std::unique_ptr<MemoryManagedPaintRecorder> recorder);
  void RestoreBackBuffer(const cc::PaintImage&);

 private:
  friend class CanvasRenderingContext2D;
  friend class OffscreenCanvasRenderingContext2D;

  Canvas2DBitmapProvider(gfx::Size size,
                         viz::SharedImageFormat format,
                         SkAlphaType alpha_type,
                         const gfx::ColorSpace& color_space,
                         const gfx::HDRMetadata& hdr_metadata,
                         CanvasResourceProviderDelegate* delegate);

  // Should only be called from static Create*() methods.
  // TODO(crbug.com/352263194): Eliminate this method by inlining its body at
  // callsites.
  void ClearAtCreation();

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;
  size_t GetSize() const override;

  // MemoryManagedPaintRecorder::Client implementation.
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;
  void RecordingCleared() override;

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;

  void ApplyAnimatedImageFrameIndexesForId(SkCanvas* canvas, uint32_t id);

  SkSurfaceProps GetSkSurfaceProps() const;
  SkSurface* GetSkSurface() const;
  sk_sp<SkSurface> CreateSkSurface() const;

  CanvasImageProvider* GetOrCreateSWCanvasImageProvider();

  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  gfx::Size size_;
  viz::SharedImageFormat format_;
  SkAlphaType alpha_type_;
  gfx::ColorSpace color_space_;
  gfx::HDRMetadata hdr_metadata_;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
  size_t max_recorded_op_bytes_;
  size_t max_pinned_image_bytes_;
  raw_ptr<CanvasResourceProviderDelegate> delegate_ = nullptr;
  mutable sk_sp<SkSurface> surface_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  bool clear_frame_ = true;

  // Even though this is a bitmap provider, it may be called upon to rasterize a
  // texture-backed resource, and that resource must be bound to a gpu context
  // for the current thread.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
