// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "cc/paint/paint_record.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_fill_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_smoothing_quality.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_2d_recorder_context.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/forward.h"  // IWYU pragma: keep (blink::Visitor)
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/skia_conversions.h"

// IWYU pragma: no_include "third_party/blink/renderer/platform/heap/visitor.h"

class SkCanvas;
class SkPixmap;
class SkSurface;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gfx {
class Rect;
class Vector2d;
}  // namespace gfx

namespace blink {

class Canvas2DResourceProvider;
class CanvasContextCreationAttributesCore;
class CanvasImageProvider;
class CanvasRenderingContext2DSettings;
class DrawElementImageOptions;
class ExceptionState;
class ImageData;
class ImageDataSettings;
class TextCluster;
class TextClusterOptions;
class TextMetrics;
class V8CanvasFontStretch;
class V8CanvasTextRendering;
class V8CanvasTextAlign;
class V8CanvasTextBaseline;
class V8CanvasDirection;
class V8CanvasFontKerning;
class V8CanvasFontVariantCaps;
class V8UnionElementOrElementImage;
enum class PredefinedColorSpace;
enum class RasterMode;

class MODULES_EXPORT BaseRenderingContext2D
    : public CanvasRenderingContext,
      public Canvas2DRecorderContext,
      public MemoryManagedPaintRecorder::Client,
      public FlushForImageObserver,
      public CanvasMemoryDumpClient,
      public WebGraphicsContext3DProviderWrapper::DestructionObserver {
 public:
  // MemoryManagedPaintRecorder::Client implementation.
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;
  void RecordingCleared() override;

  // FlushForImageObserver implementation.
  void OnFlushForImage(cc::PaintImage::ContentId content_id) override;

  using Canvas2DRecorderContext::Recorder;
  const MemoryManagedPaintRecorder* Recorder() const final;

  using Canvas2DRecorderContext::GetPaintCanvas;
  const MemoryManagedPaintCanvas* GetPaintCanvas() const final;

  bool clear_frame() const { return clear_frame_; }
  void set_clear_frame(bool clear_frame) { clear_frame_ = clear_frame; }

  size_t max_recorded_op_bytes() const { return max_recorded_op_bytes_; }
  size_t max_pinned_image_bytes() const { return max_pinned_image_bytes_; }

  static constexpr unsigned kFallbackToCPUAfterReadbacks = 2;

  // Try to restore context 4 times in the event that the context is lost. If
  // the context is unable to be restored after 4 attempts, we discard the
  // backing storage of the context and allocate a new one.
  static const unsigned kMaxTryRestoreContextAttempts = 4;

  // After context lost, it waits `kTryRestoreContextInterval` before start the
  // restore the context. This wait needs to be long enough to avoid spamming
  // the GPU process with retry attempts and short enough to provide decent UX.
  // It's currently set to 500ms.
  static constexpr base::TimeDelta kTryRestoreContextInterval =
      base::Milliseconds(500);

  BaseRenderingContext2D(const BaseRenderingContext2D&) = delete;
  BaseRenderingContext2D& operator=(const BaseRenderingContext2D&) = delete;

  ~BaseRenderingContext2D() override;

  void ResetInternal() override;

  CanvasRenderingContext2DSettings* getContextAttributes() const;

  ImageData* createImageData(ImageData*, ExceptionState&) const;
  ImageData* createImageData(int sw, int sh, ExceptionState&) const;
  ImageData* createImageData(int sw,
                             int sh,
                             ImageDataSettings*,
                             ExceptionState&) const;

  // For deferred canvases this will have the side effect of drawing recorded
  // commands in order to finalize the frame
  ImageData* getImageData(int sx, int sy, int sw, int sh, ExceptionState&);
  ImageData* getImageData(int sx,
                          int sy,
                          int sw,
                          int sh,
                          ImageDataSettings*,
                          ExceptionState&);
  virtual ImageData* getImageDataInternal(int sx,
                                          int sy,
                                          int sw,
                                          int sh,
                                          ImageDataSettings*,
                                          ExceptionState&);

  void putImageData(ImageData*, int dx, int dy, ExceptionState&);
  void putImageData(ImageData*,
                    int dx,
                    int dy,
                    int dirty_x,
                    int dirty_y,
                    int dirty_width,
                    int dirty_height,
                    ExceptionState&);

  virtual bool CanCreateBacking() = 0;
  virtual bool InitializeBacking() = 0;
  bool HasBacking() const;
  virtual bool IsBackingValid() const;
  virtual void ResetBacking();

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason) override = 0;

  String lang() const;
  void setLang(const String&);

  V8CanvasDirection direction() const;
  void setDirection(const V8CanvasDirection);

  V8CanvasTextAlign textAlign() const;
  void setTextAlign(const V8CanvasTextAlign);

  V8CanvasTextBaseline textBaseline() const;
  void setTextBaseline(const V8CanvasTextBaseline);

  String letterSpacing() const;
  void setLetterSpacing(const String&);

  String wordSpacing() const;
  void setWordSpacing(const String&);

  V8CanvasTextRendering textRendering() const;
  void setTextRendering(const V8CanvasTextRendering&);

  V8CanvasFontKerning fontKerning() const;
  void setFontKerning(const V8CanvasFontKerning);

  V8CanvasFontStretch fontStretch() const;
  void setFontStretch(const V8CanvasFontStretch&);

  V8CanvasFontVariantCaps fontVariantCaps() const;
  void setFontVariantCaps(const V8CanvasFontVariantCaps&);

  String font() const;
  void setFont(const String& new_font) override;

  void fillText(const String& text, double x, double y);
  void fillText(const String& text, double x, double y, double max_width);
  void strokeText(const String& text, double x, double y);
  void strokeText(const String& text, double x, double y, double max_width);
  TextMetrics* measureText(const String& text);
  // Renders a TextCluster returned by TextMetrics::getTextClusters(). If
  // possible, the align, baseline, and font from the TextCluster will be used.
  // The x and y parameters are added to the values from the TextCluster to
  // position the cluster.
  void fillTextCluster(const TextCluster* text_cluster, double x, double y);
  void fillTextCluster(const TextCluster* text_cluster,
                       double x,
                       double y,
                       const TextClusterOptions* cluster_options);
  void strokeTextCluster(const TextCluster* text_cluster, double x, double y);
  void strokeTextCluster(const TextCluster* text_cluster,
                         double x,
                         double y,
                         const TextClusterOptions* cluster_options);

  int LayerCount() const final;
  bool isContextLost() const final {
    return context_lost_mode_ != kNotLostContext;
  }

  void drawElementImage(const V8UnionElementOrElementImage* element,
                        double dx,
                        double dy,
                        const DrawElementImageOptions* options,
                        ExceptionState& exception_state);
  void drawElementImage(const V8UnionElementOrElementImage* element,
                        double dx,
                        double dy,
                        double dwidth,
                        double dheight,
                        const DrawElementImageOptions* options,
                        ExceptionState& exception_state);
  void drawElementImage(const V8UnionElementOrElementImage* element,
                        double sx,
                        double sy,
                        double swidth,
                        double sheight,
                        double dx,
                        double dy,
                        const DrawElementImageOptions* options,
                        ExceptionState& exception_state);
  void drawElementImage(const V8UnionElementOrElementImage* element,
                        double sx,
                        double sy,
                        double swidth,
                        double sheight,
                        double dx,
                        double dy,
                        double dwidth,
                        double dheight,
                        const DrawElementImageOptions* options,
                        ExceptionState& exception_state);

  void DrawElementInternal(const V8UnionElementOrElementImage* element,
                           std::optional<double> sx,
                           std::optional<double> sy,
                           std::optional<double> swidth,
                           std::optional<double> sheight,
                           double x,
                           double y,
                           std::optional<double> dwidth,
                           std::optional<double> dheight,
                           const DrawElementImageOptions* options,
                           ExceptionState& exception_state);

  scoped_refptr<const cc::AnimatedImageFrameIndexMap>
  GetAnimatedImageFrameIndexMap(uint32_t id) const override;

  void Trace(Visitor*) const override;

  // Implementing methods from CanvasRenderingContext
  bool IsOpaque() const final {
    return color_params_.GetAlphaType() == kOpaque_SkAlphaType;
  }
  bool IsPaintable() const override;
  bool Is2DCanvasAccelerated() const override;
  base::ByteSize AllocatedBufferSize() const override;
  scoped_refptr<StaticBitmapImage> PaintRenderingResultsToSnapshot(
      SourceDrawingBuffer source_buffer) override;
  void DisableAccelerationForCanvas2D() final { DisableAcceleration(); }
  void PageVisibilityChanged() override {}
  void Reset() override;

  void SetRestoreFailedCallbackForTesting(base::RepeatingClosure callback) {
    on_restore_failed_callback_for_testing_ = std::move(callback);
  }
  SkSurface* GetSoftwareSurfaceForTesting() const { return surface_.get(); }
  Canvas2DResourceProvider* GetSharedImageProviderForTesting() const {
    return GetSharedImageProvider();
  }

  HeapTaskRunnerTimer<BaseRenderingContext2D>
      dispatch_context_lost_event_timer_;
  HeapTaskRunnerTimer<BaseRenderingContext2D>
      dispatch_context_restored_event_timer_;
  HeapTaskRunnerTimer<BaseRenderingContext2D> try_restore_context_event_timer_;
  unsigned try_restore_context_attempt_count_ = 0;

 protected:
  std::optional<cc::PaintRecord> FlushCanvasInternal(FlushReason reason);

  void FlushIfRecordingLimitExceeded();

  void ConfigureRecorder(const gfx::Size& size, bool is_graphite);
  void ResetRecorder();

  explicit BaseRenderingContext2D(
      CanvasRenderingContextHost* canvas,
      const CanvasContextCreationAttributesCore& attrs,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual UniqueFontSelector* GetFontSelector() const;

  virtual bool WillSetFont() const;
  virtual bool ResolveFont(const String& new_font) = 0;
  virtual bool CurrentFontResolvedAndUpToDate() const;
  const LayoutLocale* LocaleFromLang();

  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y) {
    NOTREACHED();
  }
  bool WritePixelsToBacking(const SkImageInfo& orig_info,
                            const void* pixels,
                            size_t row_bytes,
                            int x,
                            int y);

  PredefinedColorSpace GetDefaultImageDataColorSpace() const final {
    return color_params_.ColorSpace();
  }

  void DispatchContextLostEvent(TimerBase*);
  void DispatchContextRestoredEvent(TimerBase*);
  void TryRestoreContextEvent(TimerBase*);
  void RestoreFromInvalidSizeIfNeeded() override;

  static const char kInheritString[];

  // Override to prematurely disable acceleration because of a readback.
  // BaseRenderingContext2D automatically disables acceleration after a number
  // of readbacks, this can be overridden to disable acceleration earlier than
  // would typically happen.
  virtual bool ShouldDisableAccelerationBecauseOfReadback() const {
    return false;
  }

  virtual void DidFlushRecording(const cc::PaintRecord& recording,
                                 bool clear_frame,
                                 FlushReason reason) {}

  Canvas2DResourceProvider* GetSharedImageProvider() const;
  void CreateSharedImageProvider(
      RasterMode raster_mode,
      gpu::SharedImageUsageSet shared_image_usage_flags);
  void CreateSharedImageProviderForSoftwareCompositor();
  void SetSharedImageProviderForTesting(
      std::unique_ptr<Canvas2DResourceProvider> provider);
  void CreateSoftwareSurface();
  void RecordBackingHistograms();
  scoped_refptr<StaticBitmapImage> Snapshot();

  bool context_restorable_{true};

 private:
  void UpdateRecordingLimits(bool is_graphite);
  virtual bool IsHibernating() const { return false; }
  virtual void EnableAccelerationIfPossible() {}
  void DrawTextInternal(const String& text,
                        double x,
                        double y,
                        CanvasRenderingContext2DState::PaintType paint_type,
                        V8CanvasTextAlign::Enum align,
                        V8CanvasTextBaseline::Enum baseline,
                        unsigned run_start,
                        unsigned run_end,
                        double* max_width = nullptr,
                        const Font* cluster_font = nullptr);

  void PutByteArray(const SkPixmap& source,
                    const gfx::Rect& source_rect,
                    const gfx::Vector2d& dest_offset);

  void WillUseCurrentFont() const;

  scoped_refptr<StaticBitmapImage> UnacceleratedSnapshot();

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;
  size_t GetSize() const override;

  // WebGraphicsContext3DProviderWrapper::DestructionObserver implementation.
  void OnContextDestroyed() override;

  void RasterRecordToSoftwareSurface(cc::PaintRecord last_recording);
  void ApplyAnimatedImageFrameIndexesForId(SkCanvas* canvas, uint32_t id);
  CanvasImageProvider* GetOrCreateSWCanvasImageProvider();

  Canvas2DColorParams color_params_;
  std::unique_ptr<Canvas2DResourceProvider> shared_image_provider_;
  sk_sp<SkSurface> surface_;
  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  // Even when using a software surface, it may be called upon to
  // rasterize a texture-backed resource, and that resource must be bound to a
  // gpu context for the current thread.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  cc::PaintImage::Id sw_snapshot_paint_image_id_ = cc::PaintImage::kInvalidId;
  cc::PaintImage::ContentId sw_snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t sw_snapshot_sk_image_id_ = 0u;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
  bool clear_frame_ = true;
  size_t max_recorded_op_bytes_ = 0;
  size_t max_pinned_image_bytes_ = 0;
  int num_readbacks_performed_ = 0;
  unsigned read_count_ = 0;
  base::RepeatingClosure on_restore_failed_callback_for_testing_;
  Vector<scoped_refptr<const cc::AnimatedImageFrameIndexMap>>
      animated_image_frame_index_maps_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
