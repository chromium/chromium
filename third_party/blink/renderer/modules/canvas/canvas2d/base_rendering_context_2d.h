// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_fill_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_smoothing_quality.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/cached_color.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/forward.h"  // IWYU pragma: keep (blink::Visitor)
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

// IWYU pragma: no_include "third_party/blink/renderer/platform/heap/visitor.h"

enum class SkPathFillType;
class SkPixmap;
struct SkSamplingOptions;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gfx {
class Rect;
class Vector2d;
}  // namespace gfx

namespace ui {
class ColorProvider;
}  // namespace ui

namespace v8 {
class Isolate;
class Value;
template <class T>
class Local;
class String;
}  // namespace v8

namespace blink {

MODULES_EXPORT BASE_DECLARE_FEATURE(kDisableCanvasOverdrawOptimization);

class BeginLayerOptions;
class CanvasGradient;
class CanvasImageSource;
class Canvas2dGPUTransferOption;
class CanvasPattern;
class CanvasRenderingContextHost;
class CanvasResourceProvider;
class DOMMatrix;
class DOMMatrixInit;
class ExceptionState;
class ExecutionContext;
class Font;
class FontSelector;
class GPUTexture;
class HTMLCanvasElement;
class Image;
class ImageData;
class ImageDataSettings;
class MemoryManagedPaintRecorder;
class Mesh2DVertexBuffer;
class Mesh2DUVBuffer;
class Mesh2DIndexBuffer;
class OffscreenCanvas;
class Path;
class Path2D;
class ScriptState;
class SimpleFontData;
class TextMetrics;
class V8GPUTextureFormat;
class V8UnionCanvasFilterOrString;
struct V8CanvasStyle;
enum class CanvasOps;
enum class ColorParseResult;
enum RespectImageOrientationEnum : uint8_t;
template <typename T>
class NotShared;
class V8CanvasFontStretch;
class V8CanvasTextRendering;

class MODULES_EXPORT BaseRenderingContext2D : public CanvasPath {
 public:
  static constexpr unsigned kFallbackToCPUAfterReadbacks = 2;

  // Try to restore context 4 times in the event that the context is lost. If
  // the context is unable to be restored after 4 attempts, we discard the
  // backing storage of the context and allocate a new one.
  static const unsigned kMaxTryRestoreContextAttempts = 4;

  BaseRenderingContext2D(const BaseRenderingContext2D&) = delete;
  BaseRenderingContext2D& operator=(const BaseRenderingContext2D&) = delete;

  ~BaseRenderingContext2D() override;

  v8::Local<v8::Value> strokeStyle(ScriptState* script_state) const;
  void setStrokeStyle(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      ExceptionState& exception_state);

  v8::Local<v8::Value> fillStyle(ScriptState* script_state) const;
  void setFillStyle(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    ExceptionState& exception_state);

  double lineWidth() const;
  void setLineWidth(double);

  String lineCap() const;
  void setLineCap(const String&);

  String lineJoin() const;
  void setLineJoin(const String&);

  double miterLimit() const;
  void setMiterLimit(double);

  const Vector<double>& getLineDash() const;
  void setLineDash(const Vector<double>&);

  double lineDashOffset() const;
  void setLineDashOffset(double);

  virtual double shadowOffsetX() const;
  virtual void setShadowOffsetX(double);

  virtual double shadowOffsetY() const;
  virtual void setShadowOffsetY(double);

  virtual double shadowBlur() const;
  virtual void setShadowBlur(double);

  String shadowColor() const;
  void setShadowColor(const String&);

  // Alpha value that goes from 0 to 1.
  double globalAlpha() const;
  void setGlobalAlpha(double);

  String globalCompositeOperation() const;
  void setGlobalCompositeOperation(const String&);

  const V8UnionCanvasFilterOrString* filter() const;
  void setFilter(ScriptState*, const V8UnionCanvasFilterOrString* input);

  void save();
  void restore(ExceptionState& exception_state);
  // Push state on state stack and creates bitmap for subsequent draw ops.
  void beginLayer(ScriptState* script_state) {
    beginLayerImpl(script_state, /*options=*/nullptr,
                   /*exception_state=*/nullptr);
  }
  void beginLayer(ScriptState* script_state,
                  const BeginLayerOptions* options,
                  ExceptionState& exception_state) {
    beginLayerImpl(script_state, options, &exception_state);
  }
  // Pop state stack if top state was pushed by beginLayer, restore state and draw the bitmap.
  void endLayer(ExceptionState& exception_state);
  int LayerCount() const { return layer_count_; }
  virtual void reset();  // Called by the javascript interface
  void ResetInternal();  // Called from within blink

  void scale(double sx, double sy);
  void rotate(double angle_in_radians);
  void translate(double tx, double ty);
  void transform(double m11,
                 double m12,
                 double m21,
                 double m22,
                 double dx,
                 double dy);
  void setTransform(double m11,
                    double m12,
                    double m21,
                    double m22,
                    double dx,
                    double dy);
  void setTransform(DOMMatrixInit*, ExceptionState&);
  virtual DOMMatrix* getTransform();
  virtual void resetTransform();

  void beginPath();

  void fill();
  void fill(const V8CanvasFillRule& winding);
  void fill(Path2D*);
  void fill(Path2D*, const V8CanvasFillRule& winding);
  void stroke();
  void stroke(Path2D*);
  void clip(const V8CanvasFillRule& winding =
                V8CanvasFillRule(V8CanvasFillRule::Enum::kNonzero));
  void clip(Path2D*,
            const V8CanvasFillRule& winding =
                V8CanvasFillRule(V8CanvasFillRule::Enum::kNonzero));

  bool isPointInPath(const double x,
                     const double y,
                     const V8CanvasFillRule& winding =
                         V8CanvasFillRule(V8CanvasFillRule::Enum::kNonzero));
  bool isPointInPath(Path2D*,
                     const double x,
                     const double y,
                     const V8CanvasFillRule& winding =
                         V8CanvasFillRule(V8CanvasFillRule::Enum::kNonzero));
  bool isPointInStroke(const double x, const double y);
  bool isPointInStroke(Path2D*, const double x, const double y);

  void clearRect(double x, double y, double width, double height);
  void fillRect(double x, double y, double width, double height);
  void strokeRect(double x, double y, double width, double height);

  // https://github.com/Igalia/explainers/blob/main/canvas-formatted-text/html-in-canvas.md
  void placeElement(Element* element,
                    double x,
                    double y,
                    ExceptionState& exception_state);
  void drawImage(const V8CanvasImageSource* image_source,
                 double x,
                 double y,
                 ExceptionState& exception_state);
  void drawImage(const V8CanvasImageSource* image_source,
                 double x,
                 double y,
                 double width,
                 double height,
                 ExceptionState& exception_state);
  void drawImage(const V8CanvasImageSource* image_source,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState& exception_state);
  void drawImage(CanvasImageSource*,
                 double sx,
                 double sy,
                 double sw,
                 double sh,
                 double dx,
                 double dy,
                 double dw,
                 double dh,
                 ExceptionState&);

  CanvasGradient* createLinearGradient(double x0,
                                       double y0,
                                       double x1,
                                       double y1);
  CanvasGradient* createRadialGradient(double x0,
                                       double y0,
                                       double r0,
                                       double x1,
                                       double y1,
                                       double r1,
                                       ExceptionState&);
  CanvasGradient* createConicGradient(double startAngle,
                                      double centerX,
                                      double centerY);
  CanvasPattern* createPattern(const V8CanvasImageSource* image_source,
                               const String& repetition_type,
                               ExceptionState& exception_state);
  CanvasPattern* createPattern(CanvasImageSource*,
                               const String& repetition_type,
                               ExceptionState&);

  Mesh2DVertexBuffer* createMesh2DVertexBuffer(NotShared<DOMFloat32Array>,
                                               ExceptionState&);
  Mesh2DUVBuffer* createMesh2DUVBuffer(NotShared<DOMFloat32Array>,
                                       ExceptionState&);
  Mesh2DIndexBuffer* createMesh2DIndexBuffer(NotShared<DOMUint16Array>,
                                             ExceptionState&);
  void drawMesh(const Mesh2DVertexBuffer* vertex_buffer,
                const Mesh2DUVBuffer* uv_buffer,
                const Mesh2DIndexBuffer* index_buffer,
                const V8CanvasImageSource* image,
                ExceptionState&);

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

  bool imageSmoothingEnabled() const;
  void setImageSmoothingEnabled(bool);
  V8ImageSmoothingQuality imageSmoothingQuality() const;
  void setImageSmoothingQuality(const V8ImageSmoothingQuality&);

  // Transfers a canvas' existing back-buffer to a GPUTexture for use in a
  // WebGPU pipeline. The canvas' image can be used as a texture, or the texture
  // can be bound as a color attachment and modified. After its texture is
  // transferred, the canvas will be reset into an empty, freshly-initialized
  // state.
  GPUTexture* transferToGPUTexture(const Canvas2dGPUTransferOption*,
                                   ExceptionState& exception_state);

  // Replaces the canvas' back-buffer texture with the passed-in GPUTexture.
  // The GPUTexture immediately becomes inaccessible to WebGPU.
  // A GPUValidationError will occur if the GPUTexture is used after
  // `transferBackFromGPUTexture` is called.
  void transferBackFromGPUTexture(ExceptionState& exception_state);

  // Returns the format of the GPUTexture that `transferToGPUTexture` will
  // return. This is useful if you need to create the WebGPU render pipeline
  // before `transferToGPUTexture` is first called.
  V8GPUTextureFormat getTextureFormat() const;

  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;

  virtual int Width() const = 0;
  virtual int Height() const = 0;

  bool IsAccelerated() const;
  virtual bool CanCreateCanvas2dResourceProvider() const = 0;

  virtual RespectImageOrientationEnum RespectImageOrientation() const = 0;

  // Returns the color to use as the current color for operations that identify
  // the current color.
  virtual Color GetCurrentColor() const = 0;

  virtual cc::PaintCanvas* GetOrCreatePaintCanvas() = 0;
  virtual const cc::PaintCanvas* GetPaintCanvas() const = 0;
  cc::PaintCanvas* GetPaintCanvas() {
    return const_cast<cc::PaintCanvas*>(
        const_cast<const BaseRenderingContext2D*>(this)->GetPaintCanvas());
  }

  // Returns the paint ops recorder this context uses. Can be `nullptr` if no
  // recorder is available.
  virtual const MemoryManagedPaintRecorder* Recorder() const = 0;
  MemoryManagedPaintRecorder* Recorder() {
    return const_cast<MemoryManagedPaintRecorder*>(
        const_cast<const BaseRenderingContext2D*>(this)->Recorder());
  }

  // Called when about to draw. When this is called GetPaintCanvas() has already
  // been called and returned a non-null value.
  virtual void WillDraw(const SkIRect& dirty_rect,
                        CanvasPerformanceMonitor::DrawType) = 0;

  virtual sk_sp<PaintFilter> StateGetFilter() = 0;
  void SnapshotStateForFilter();

  virtual CanvasRenderingContextHost* GetCanvasRenderingContextHost() const {
    return nullptr;
  }

  ExecutionContext* GetTopExecutionContext() const override = 0;

  void ValidateStateStack(const cc::PaintCanvas* canvas = nullptr) const {
#if DCHECK_IS_ON()
    ValidateStateStackImpl(canvas);
#endif
  }

  virtual bool HasAlpha() const = 0;

  virtual bool IsDesynchronized() const {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  virtual bool isContextLost() const = 0;

  virtual void WillDrawImage(CanvasImageSource*) const {}

  void RestoreMatrixClipStack(cc::PaintCanvas*) const;

  String direction() const;
  void setDirection(const String&);

  String textAlign() const;
  void setTextAlign(const String&);

  String textBaseline() const;
  void setTextBaseline(const String&);

  String letterSpacing() const;
  void setLetterSpacing(const String&);

  String wordSpacing() const;
  void setWordSpacing(const String&);

  V8CanvasTextRendering textRendering() const;
  void setTextRendering(const V8CanvasTextRendering&);

  String textRenderingAsString() const;
  void setTextRenderingAsString(const String&);

  String fontKerning() const;
  void setFontKerning(const String&);

  V8CanvasFontStretch fontStretch() const;
  void setFontStretch(const V8CanvasFontStretch&);

  String fontStretchAsString() const;
  void setFontStretchAsString(const String&);

  String fontVariantCaps() const;
  void setFontVariantCaps(const String&);

  String font() const;
  void setFont(const String& new_font);

  void fillText(const String& text, double x, double y);
  void fillText(const String& text, double x, double y, double max_width);
  void strokeText(const String& text, double x, double y);
  void strokeText(const String& text, double x, double y, double max_width);
  TextMetrics* measureText(const String& text);

  void Trace(Visitor*) const override;

  enum DrawCallType {
    kStrokePath = 0,
    kFillPath,
    kDrawVectorImage,
    kDrawBitmapImage,
    kFillText,
    kStrokeText,
    kFillRect,
    kStrokeRect,
    kDrawCallTypeCount  // used to specify the size of storage arrays
  };

  enum PathFillType {
    kColorFillType,
    kLinearGradientFillType,
    kRadialGradientFillType,
    kPatternFillType,
    kPathFillTypeCount  // used to specify the size of storage arrays
  };

  enum class GPUFallbackToCPUScenario {
    // Used for UMA histogram, do not change enum item values.
    kLargePatternDrawnToGPU = 0,
    kGetImageData = 1,

    kMaxValue = kGetImageData
  };

  enum class OverdrawOp {
    // Must remain in sync with CanvasOverdrawOp defined in
    // tools/metrics/histograms/enums.xml
    //
    // Note: Several enum values are now obsolete because the use cases they
    // covered were removed because they had low incidence rates in real-world
    // web content.

    kNone = 0,  // Not used in histogram

    kTotal = 1,  // Counts total number of overdraw optimization hits.

    // Ops. These are mutually exclusive for a given overdraw hit.
    kClearRect = 2,
    // kFillRect = 3,  // Removed due to low incidence
    // kPutImageData = 4,  // Removed due to low incidence
    kDrawImage = 5,
    kContextReset = 6,
    // kClearForSrcBlendMode = 7,  // Removed due to low incidence

    // Modifiers
    kHasTransform = 9,
    // kSourceOverBlendMode = 10,  // Removed due to low incidence
    // kClearBlendMode = 11,  // Removed due to low incidence
    kHasClip = 12,
    kHasClipAndTransform = 13,

    kMaxValue = kHasClipAndTransform,
  };

  struct UsageCounters {
    int num_draw_calls[kDrawCallTypeCount];  // use DrawCallType enum as index
    float bounding_box_perimeter_draw_calls[kDrawCallTypeCount];
    float bounding_box_area_draw_calls[kDrawCallTypeCount];
    float bounding_box_area_fill_type[kPathFillTypeCount];
    int num_non_convex_fill_path_calls;
    float non_convex_fill_path_area;
    int num_radial_gradients;
    int num_linear_gradients;
    int num_patterns;
    int num_draw_with_complex_clips;
    int num_blurred_shadows;
    float bounding_box_area_times_shadow_blur_squared;
    float bounding_box_perimeter_times_shadow_blur_squared;
    int num_filters;
    int num_get_image_data_calls;
    float area_get_image_data_calls;
    int num_put_image_data_calls;
    float area_put_image_data_calls;
    int num_clear_rect_calls;
    int num_draw_focus_calls;
    int num_frames_since_reset;

    UsageCounters();
  };

  const UsageCounters& GetUsage();
  HeapTaskRunnerTimer<BaseRenderingContext2D>
      dispatch_context_lost_event_timer_;
  HeapTaskRunnerTimer<BaseRenderingContext2D>
      dispatch_context_restored_event_timer_;
  HeapTaskRunnerTimer<BaseRenderingContext2D> try_restore_context_event_timer_;
  unsigned try_restore_context_attempt_count_ = 0;

 protected:
  explicit BaseRenderingContext2D(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual HTMLCanvasElement* HostAsHTMLCanvasElement() const;
  virtual OffscreenCanvas* HostAsOffscreenCanvas() const;
  virtual FontSelector* GetFontSelector() const;
  const Font& AccessFont(HTMLCanvasElement* canvas);

  void WillUseCurrentFont() const;
  virtual bool WillSetFont() const;
  virtual bool ResolveFont(const String& new_font) = 0;
  virtual bool CurrentFontResolvedAndUpToDate() const;

  ALWAYS_INLINE CanvasRenderingContext2DState& GetState() const {
    return *state_stack_.back();
  }

  bool ComputeDirtyRect(const gfx::RectF& local_bounds, SkIRect*);
  bool ComputeDirtyRect(const gfx::RectF& local_bounds,
                        const SkIRect& transformed_clip_bounds,
                        SkIRect*);

  template <OverdrawOp CurrentOverdrawOp,
            typename DrawFunc,
            typename DrawCoversClipBoundsFunc>
  void Draw(const DrawFunc&,
            const DrawCoversClipBoundsFunc&,
            const gfx::RectF& bounds,
            CanvasRenderingContext2DState::PaintType,
            CanvasRenderingContext2DState::ImageType,
            CanvasPerformanceMonitor::DrawType);

  void InflateStrokeRect(gfx::RectF&) const;

  void UnwindStateStack();

  // Return the default color space to be used for calls to GetImageData or
  // CreateImageData.
  virtual PredefinedColorSpace GetDefaultImageDataColorSpace() const = 0;

  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  virtual scoped_refptr<StaticBitmapImage> GetImage(FlushReason) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  void CheckOverdraw(const cc::PaintFlags*,
                     CanvasRenderingContext2DState::ImageType,
                     BaseRenderingContext2D::OverdrawOp overdraw_op);

  HeapVector<Member<CanvasRenderingContext2DState>> state_stack_;
  unsigned max_state_stack_depth_ = 1;
  // Counts how many states have been pushed with BeginLayer.
  int layer_count_ = 0;
  AntiAliasingMode clip_antialiasing_;

  virtual void FinalizeFrame(FlushReason) {}

  float GetFontBaseline(const SimpleFontData&) const;
  virtual void DispatchContextLostEvent(TimerBase*);
  virtual void DispatchContextRestoredEvent(TimerBase*);
  virtual void TryRestoreContextEvent(TimerBase*) {}

  static const char kDefaultFont[];
  static const char kInheritDirectionString[];
  static const char kRtlDirectionString[];
  static const char kLtrDirectionString[];
  static const char kAutoKerningString[];
  static const char kNormalKerningString[];
  static const char kNoneKerningString[];
  static const char kNormalVariantString[];
  static const char kUltraCondensedString[];
  static const char kExtraCondensedString[];
  static const char kCondensedString[];
  static const char kSemiCondensedString[];
  static const char kNormalStretchString[];
  static const char kSemiExpandedString[];
  static const char kExpandedString[];
  static const char kExtraExpandedString[];
  static const char kUltraExpandedString[];
  static const char kSmallCapsVariantString[];
  static const char kAllSmallCapsVariantString[];
  static const char kPetiteVariantString[];
  static const char kAllPetiteVariantString[];
  static const char kUnicaseVariantString[];
  static const char kTitlingCapsVariantString[];
  static const char kAutoRendering[];
  static const char kOptimizeSpeedRendering[];
  static const char kOptimizeLegibilityRendering[];
  static const char kGeometricPrecisionRendering[];
  virtual void DisableAcceleration() {}

  // Override to prematurely disable acceleration because of a readback.
  // BaseRenderingContext2D automatically disables acceleration after a number
  // of readbacks, this can be overridden to disable acceleration earlier than
  // would typically happen.
  virtual bool ShouldDisableAccelerationBecauseOfReadback() const {
    return false;
  }

  virtual bool IsPaint2D() const { return false; }
  void WillOverwriteCanvas(OverdrawOp);

  void SetColorScheme(mojom::blink::ColorScheme color_scheme) {
    if (color_scheme == color_scheme_) {
      return;
    }

    color_cache_.clear();
    color_scheme_ = color_scheme;
  }

  // Returns the color provider stored in the Page via the Document.
  const ui::ColorProvider* GetColorProvider() const;

  // Returns if the current Document is within installed WebApp scope.
  bool IsInWebAppScope() const;

  bool context_restorable_{true};
  CanvasRenderingContext::LostContextMode context_lost_mode_{
      CanvasRenderingContext::kNotLostContext};

  // TODO(issues.chromium.org/issues/349835587): Add an observer to know if the
  // element is detached and then remove it.
  HeapHashMap<WeakMember<Element>, scoped_refptr<CanvasDeferredPaintRecord>>
      placed_elements_;

 private:
  void FillImpl(SkPathFillType winding_rule);
  void FillPathImpl(Path2D* dom_path, SkPathFillType winding_rule);

  void DrawTextInternal(const String& text,
                        double x,
                        double y,
                        CanvasRenderingContext2DState::PaintType paint_type,
                        double* max_width = nullptr);

  // Returns the color from a string. This may return a cached value as well
  // as updating the cache (if possible).
  bool ExtractColorFromV8StringAndUpdateCache(v8::Isolate* isolate,
                                              v8::Local<v8::String> v8_string,
                                              ExceptionState& exception_state,
                                              Color& color);

  CanvasRenderingContext2DState::SaveType SaveLayerForState(
      const CanvasRenderingContext2DState& state,
      sk_sp<PaintFilter> layer_filter,
      cc::PaintCanvas& canvas);

  void beginLayerImpl(ScriptState* script_state,
                      const BeginLayerOptions* options,
                      ExceptionState* exception_state);
  void AddLayerFilterUserCount(const V8CanvasFilterInput*);

  // Pops from the top of the state stack, inverts transform, restores the
  // PaintCanvas, and validates the state stack. Helper for Restore and
  // EndLayer.
  void PopAndRestore(cc::PaintCanvas& canvas);

  void ValidateStateStackImpl(const cc::PaintCanvas* canvas = nullptr) const;

  bool ShouldDrawImageAntialiased(const gfx::RectF& dest_rect) const;

  void SetTransform(const AffineTransform&);

  AffineTransform GetTransform() const override;

  bool StateHasFilter();

  // When the canvas is stroked or filled with a pattern, which is assumed to
  // have a transparent background, the shadow needs to be applied with
  // DropShadowPaintFilter for kNonOpaqueImageType
  // Used in Draw and CompositedDraw to avoid the shadow offset being modified
  // by the transformation matrix
  ALWAYS_INLINE bool ShouldUseDropShadowPaintFilter(
      CanvasRenderingContext2DState::PaintType paint_type,
      CanvasRenderingContext2DState::ImageType image_type) const {
    return (paint_type == CanvasRenderingContext2DState::kFillPaintType ||
            paint_type == CanvasRenderingContext2DState::kStrokePaintType) &&
           image_type == CanvasRenderingContext2DState::kNonOpaqueImage;
  }

  bool BlendModeRequiresCompositedDraw(
      const CanvasRenderingContext2DState& state) const;

  ALWAYS_INLINE bool ShouldUseCompositedDraw(
      CanvasRenderingContext2DState::PaintType paint_type,
      CanvasRenderingContext2DState::ImageType image_type) {
    const CanvasRenderingContext2DState& state = GetState();
    if (BlendModeRequiresCompositedDraw(state)) {
      return true;
    }
    if (StateHasFilter())
      return true;
    if (state.ShouldDrawShadows() &&
        ShouldUseDropShadowPaintFilter(paint_type, image_type))
      return true;
    return false;
  }

  void ResetAlphaIfNeeded(cc::PaintCanvas* c,
                          SkBlendMode blend_mode,
                          const gfx::RectF* bounds = nullptr);

  // `paint_canvas` is null if this function is called asynchronously.
  template <OverdrawOp CurrentOverdrawOp,
            typename DrawFunc,
            typename DrawCoversClipBoundsFunc>
  void DrawInternal(cc::PaintCanvas* paint_canvas,
                    const DrawFunc&,
                    const DrawCoversClipBoundsFunc&,
                    const gfx::RectF& bounds,
                    CanvasRenderingContext2DState::PaintType,
                    CanvasRenderingContext2DState::ImageType,
                    const SkIRect& clip_bounds,
                    CanvasPerformanceMonitor::DrawType);

  void DrawPathInternal(const CanvasPath&,
                        CanvasRenderingContext2DState::PaintType,
                        SkPathFillType,
                        cc::UsePaintCache);
  void DrawImageInternal(cc::PaintCanvas*,
                         CanvasImageSource*,
                         Image*,
                         const gfx::RectF& src_rect,
                         const gfx::RectF& dst_rect,
                         const SkSamplingOptions&,
                         const cc::PaintFlags*);
  void ClipInternal(const Path&,
                    const V8CanvasFillRule& winding_rule,
                    cc::UsePaintCache);

  bool IsPointInPathInternal(const Path&,
                             const double x,
                             const double y,
                             const V8CanvasFillRule& winding_rule);
  bool IsPointInStrokeInternal(const Path&, const double x, const double y);

  static bool IsFullCanvasCompositeMode(SkBlendMode);

  template <typename DrawFunc>
  void CompositedDraw(const DrawFunc&,
                      cc::PaintCanvas*,
                      CanvasRenderingContext2DState::PaintType,
                      CanvasRenderingContext2DState::ImageType);

  template <typename T>
  bool ValidateRectForCanvas(T x, T y, T width, T height);

  template <typename T>
  void AdjustRectForCanvas(T& x, T& y, T& width, T& height);

  bool RectContainsTransformedRect(const gfx::RectF&, const SkIRect&) const;
  // Sets the origin to be tainted by the content of the canvas, such
  // as a cross-origin image. This is as opposed to some other reason
  // such as tainting from a filter applied to the canvas.
  void SetOriginTaintedByContent();

  void PutByteArray(const SkPixmap& source,
                    const gfx::Rect& source_rect,
                    const gfx::Vector2d& dest_offset);
  virtual bool IsCanvas2DBufferValid() const {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  virtual std::optional<cc::PaintRecord> FlushCanvas(FlushReason) = 0;

  // Only call if identifiability_study_helper_.ShouldUpdateBuilder() returns
  // true.
  void IdentifiabilityUpdateForStyleUnion(const V8CanvasStyle& style);

  RespectImageOrientationEnum RespectImageOrientationInternal(
      CanvasImageSource*);

  // Updates the identifiability study before changing stroke or fill styles.
  void UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
      const V8CanvasStyle& v8_style,
      CanvasOps op);
  void UpdateIdentifiabilityStudyBeforeSettingStrokeOrFill(
      v8::Local<v8::String> v8_string,
      CanvasOps op);

  // Parses the string as a color and returns the result of parsing.
  ColorParseResult ParseColorOrCurrentColor(const String& color_string,
                                            Color& color) const;

  cc::PaintFlags GetClearFlags() const;

  bool origin_tainted_by_content_ = false;
  cc::UsePaintCache path2d_use_paint_cache_;
  int num_readbacks_performed_ = 0;
  unsigned read_count_ = 0;
  mojom::blink::ColorScheme color_scheme_ = mojom::blink::ColorScheme::kLight;
  // Cache of recently used colors. Maintains LRU semantics.
  HeapLinkedHashSet<Member<CachedColor>, CachedColorTraits> color_cache_;
  Member<GPUTexture> webgpu_access_texture_ = nullptr;
  std::unique_ptr<CanvasResourceProvider> resource_provider_from_webgpu_access_;
};

namespace {

// Blend modes that require compositing with layers when shadows are drawn.
ALWAYS_INLINE bool BlendModeRequiresLayersForShadows(SkBlendMode blendMode) {
  return blendMode == SkBlendMode::kDstOver ||
         blendMode == SkBlendMode::kPlus ||
         blendMode == SkBlendMode::kMultiply ||
         blendMode == SkBlendMode::kXor || blendMode == SkBlendMode::kOverlay ||
         blendMode == SkBlendMode::kDarken ||
         blendMode == SkBlendMode::kLighten ||
         blendMode == SkBlendMode::kColorDodge ||
         blendMode == SkBlendMode::kColorBurn ||
         blendMode == SkBlendMode::kHardLight ||
         blendMode == SkBlendMode::kSoftLight ||
         blendMode == SkBlendMode::kDifference ||
         blendMode == SkBlendMode::kExclusion ||
         blendMode == SkBlendMode::kHue ||
         blendMode == SkBlendMode::kSaturation ||
         blendMode == SkBlendMode::kColor ||
         blendMode == SkBlendMode::kLuminosity;
}

ALWAYS_INLINE bool BlendModeDoesntPreserveOpaqueDestinationAlpha(
    SkBlendMode blendMode) {
  return blendMode == SkBlendMode::kSrc || blendMode == SkBlendMode::kSrcIn ||
         blendMode == SkBlendMode::kDstIn ||
         blendMode == SkBlendMode::kSrcOut ||
         blendMode == SkBlendMode::kDstOut ||
         blendMode == SkBlendMode::kSrcATop ||
         blendMode == SkBlendMode::kDstATop || blendMode == SkBlendMode::kXor ||
         blendMode == SkBlendMode::kModulate;
}

}  // namespace

ALWAYS_INLINE bool BaseRenderingContext2D::BlendModeRequiresCompositedDraw(
    const CanvasRenderingContext2DState& state) const {
  SkBlendMode blend_mode = state.GlobalComposite();
  // Blend modes that require CompositedDraw in every case.
  if (IsFullCanvasCompositeMode(blend_mode)) {
    return true;
  }
  // Blend modes that require CompositedDraw if shadows are drawn.
  return state.ShouldDrawShadows() &&
         BlendModeRequiresLayersForShadows(blend_mode);
}

ALWAYS_INLINE void BaseRenderingContext2D::ResetAlphaIfNeeded(
    cc::PaintCanvas* c,
    SkBlendMode blend_mode,
    const gfx::RectF* bounds) {
  // TODO(skbug.com/14239): This would be unnecessary if skia had something
  // like glColorMask that could be used to prevent the destination alpha from
  // being modified.
  if (!HasAlpha() &&
      BlendModeDoesntPreserveOpaqueDestinationAlpha(blend_mode)) {
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kDstOver);
    flags.setColor(SK_ColorBLACK);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    SkRect alpha_bounds;
    if (!c->getLocalClipBounds(&alpha_bounds)) {
      return;
    }
    if (bounds) {
      alpha_bounds.intersect(gfx::RectFToSkRect(*bounds));
    }
    c->drawRect(alpha_bounds, flags);
  }
}

ALWAYS_INLINE void BaseRenderingContext2D::CheckOverdraw(
    const cc::PaintFlags* flags,
    CanvasRenderingContext2DState::ImageType image_type,
    BaseRenderingContext2D::OverdrawOp overdraw_op) {
  if (base::FeatureList::IsEnabled(kDisableCanvasOverdrawOptimization))
      [[unlikely]] {
    return;
  }

  // Note on performance: because this method is inlined, all conditional
  // branches on arguments that are static at the call site can be optimized-out
  // by the compiler.
  if (overdraw_op == OverdrawOp::kNone)
    return;

  cc::PaintCanvas* c = GetPaintCanvas();
  if (!c) [[unlikely]] {
    return;
  }

  // Overdraw in layers is not currently supported. We would need to be able to
  // drop draw ops in the current layer only, which is not currently possible.
  if (layer_count_ != 0) {
    return;
  }

  if (overdraw_op == OverdrawOp::kDrawImage) {  // static branch
    if (flags->getBlendMode() != SkBlendMode::kSrcOver || flags->getLooper() ||
        flags->getImageFilter() || !flags->isOpaque() ||
        image_type == CanvasRenderingContext2DState::kNonOpaqueImage)
        [[unlikely]] {
      return;
    }
  }

  if (overdraw_op == OverdrawOp::kClearRect ||
      overdraw_op == OverdrawOp::kDrawImage) {  // static branch
    if (GetState().HasComplexClip()) [[unlikely]] {
      return;
    }

    SkIRect sk_i_bounds;
    if (!c->getDeviceClipBounds(&sk_i_bounds)) [[unlikely]] {
      return;
    }
    SkRect device_rect = SkRect::Make(sk_i_bounds);
    const SkImageInfo& image_info = c->imageInfo();
    if (!device_rect.contains(SkRect::MakeWH(image_info.width(),
                                             image_info.height()))) [[likely]] {
      return;
    }
  }

  WillOverwriteCanvas(overdraw_op);
}

template <BaseRenderingContext2D::OverdrawOp CurrentOverdrawOp,
          typename DrawFunc,
          typename DrawCoversClipBoundsFunc>
void BaseRenderingContext2D::DrawInternal(
    cc::PaintCanvas* paint_canvas,
    const DrawFunc& draw_func,
    const DrawCoversClipBoundsFunc& draw_covers_clip_bounds,
    const gfx::RectF& bounds,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type,
    const SkIRect& clip_bounds,
    CanvasPerformanceMonitor::DrawType draw_type) {
  if (!paint_canvas) [[unlikely]] {
    // This is the async draw case.
    paint_canvas = GetPaintCanvas();
    if (!paint_canvas) {
      return;
    }
  }
  const CanvasRenderingContext2DState& state = GetState();
  SkBlendMode global_composite = state.GlobalComposite();
  if (ShouldUseCompositedDraw(paint_type, image_type)) {
    WillDraw(clip_bounds, draw_type);
    CompositedDraw(draw_func, paint_canvas, paint_type, image_type);
    ResetAlphaIfNeeded(paint_canvas, global_composite);
  } else if (global_composite == SkBlendMode::kSrc) {
    // Takes care of CheckOverdraw()
    paint_canvas->clear(HasAlpha() ? SkColors::kTransparent : SkColors::kBlack);
    const cc::PaintFlags* flags =
        state.GetFlags(paint_type, kDrawForegroundOnly, image_type);
    WillDraw(clip_bounds, draw_type);
    draw_func(paint_canvas, flags);
    ResetAlphaIfNeeded(paint_canvas, global_composite, &bounds);
  } else {
    SkIRect dirty_rect;
    if (ComputeDirtyRect(bounds, clip_bounds, &dirty_rect)) {
      const cc::PaintFlags* flags =
          state.GetFlags(paint_type, kDrawShadowAndForeground, image_type);
      if (paint_type != CanvasRenderingContext2DState::kStrokePaintType &&
          draw_covers_clip_bounds(clip_bounds)) {
        // Because CurrentOverdrawOp is a template argument the following branch
        // is optimized-out at compile time.
        if (CurrentOverdrawOp != OverdrawOp::kNone) {
          CheckOverdraw(flags, image_type, CurrentOverdrawOp);
        }
      }
      WillDraw(dirty_rect, draw_type);
      draw_func(paint_canvas, flags);
      ResetAlphaIfNeeded(paint_canvas, global_composite, &bounds);
    }
  }
  if (paint_canvas->NeedsFlush()) [[unlikely]] {
    // This happens if draw_func called flush() on the PaintCanvas. The flush
    // cannot be performed inside the scope of draw_func because it would break
    // the logic of CompositedDraw.
    FlushCanvas(FlushReason::kVolatileSourceImage);
  }
}

template <BaseRenderingContext2D::OverdrawOp CurrentOverdrawOp,
          typename DrawFunc,
          typename DrawCoversClipBoundsFunc>
void BaseRenderingContext2D::Draw(
    const DrawFunc& draw_func,
    const DrawCoversClipBoundsFunc& draw_covers_clip_bounds,
    const gfx::RectF& bounds,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type,
    CanvasPerformanceMonitor::DrawType draw_type) {
  if (!IsTransformInvertible()) [[unlikely]] {
    return;
  }

  SkIRect clip_bounds;
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas || !paint_canvas->getDeviceClipBounds(&clip_bounds))
    return;

  if (GetState().IsFilterUnresolved()) [[unlikely]] {
    // Resolving a filter requires allocating garbage-collected objects.
    DrawInternal<CurrentOverdrawOp, DrawFunc, DrawCoversClipBoundsFunc>(
        nullptr, draw_func, draw_covers_clip_bounds, bounds, paint_type,
        image_type, clip_bounds, draw_type);
  } else {
    DrawInternal<CurrentOverdrawOp, DrawFunc, DrawCoversClipBoundsFunc>(
        paint_canvas, draw_func, draw_covers_clip_bounds, bounds, paint_type,
        image_type, clip_bounds, draw_type);
  }
}

template <typename DrawFunc>
void BaseRenderingContext2D::CompositedDraw(
    const DrawFunc& draw_func,
    cc::PaintCanvas* c,
    CanvasRenderingContext2DState::PaintType paint_type,
    CanvasRenderingContext2DState::ImageType image_type) {
  // Due to the complexity of composited draw operations, we need to grant an
  // exception to allow multi-pass rendereing, and state finalization
  // operations to proceed between the time when a flush is requested by
  // draw_func and when the flush request is fulfilled in DrawInternal. We
  // cannot fulfill flush request in the middle of a composited draw because
  // it would break the rendering behavior.
  // It is safe to cast 'c' to RecordPaintCanvas below because we know that
  // CanvasResourceProvider always creates PaintCanvases of that type.
  // TODO(junov): We could pass 'c' as a RecordingPaintCanvas in order to
  // eliminate the static_cast.  This would require changing a lot of plumbing
  // and fixing virtual methods that have non-virtual overloads.
  cc::RecordPaintCanvas::DisableFlushCheckScope disable_flush_check_scope(
      static_cast<cc::RecordPaintCanvas*>(c));

  sk_sp<PaintFilter> canvas_filter = StateGetFilter();
  const CanvasRenderingContext2DState& state = GetState();
  DCHECK(ShouldUseCompositedDraw(paint_type, image_type));
  SkM44 ctm = c->getLocalToDevice();
  c->setMatrix(SkM44());
  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(state.GlobalComposite());
  if (state.ShouldDrawShadows()) {
    // unroll into two independently composited passes if drawing shadows
    cc::PaintFlags shadow_flags =
        *state.GetFlags(paint_type, kDrawShadowOnly, image_type);
    if (canvas_filter ||
        ShouldUseDropShadowPaintFilter(paint_type, image_type)) {
      cc::PaintFlags foreground_flags =
          *state.GetFlags(paint_type, kDrawForegroundOnly, image_type);
      shadow_flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
          sk_make_sp<ComposePaintFilter>(foreground_flags.getImageFilter(),
                                         shadow_flags.getImageFilter()),
          canvas_filter));
      // Resetting the alpha of the shadow layer, to avoid the alpha being
      // applied twice.
      shadow_flags.setAlphaf(1.0f);
      // Saving the shadow layer before setting the matrix, so the shadow offset
      // does not get modified by the transformation matrix
      shadow_flags.setBlendMode(state.GlobalComposite());
      c->saveLayer(shadow_flags);
      foreground_flags.setBlendMode(SkBlendMode::kSrcOver);
      c->setMatrix(ctm);
      draw_func(c, &foreground_flags);
    } else {
      DCHECK(IsFullCanvasCompositeMode(state.GlobalComposite()) ||
             BlendModeRequiresCompositedDraw(state));
      c->saveLayer(composite_flags);
      shadow_flags.setBlendMode(SkBlendMode::kSrcOver);
      c->setMatrix(ctm);
      draw_func(c, &shadow_flags);
    }
    c->restore();
  }

  composite_flags.setImageFilter(std::move(canvas_filter));
  c->saveLayer(composite_flags);
  cc::PaintFlags foreground_flags =
      *state.GetFlags(paint_type, kDrawForegroundOnly, image_type);
  foreground_flags.setBlendMode(SkBlendMode::kSrcOver);
  c->setMatrix(ctm);
  draw_func(c, &foreground_flags);
  c->restore();
  c->setMatrix(ctm);
}

template <typename T>
bool BaseRenderingContext2D::ValidateRectForCanvas(T x,
                                                   T y,
                                                   T width,
                                                   T height) {
  return (std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
          std::isfinite(height) && (width || height));
}

template <typename T>
void BaseRenderingContext2D::AdjustRectForCanvas(T& x,
                                                 T& y,
                                                 T& width,
                                                 T& height) {
  if (width < 0) {
    width = -width;
    x -= width;
  }

  if (height < 0) {
    height = -height;
    y -= height;
  }
}

ALWAYS_INLINE void BaseRenderingContext2D::SetTransform(
    const AffineTransform& matrix) {
  GetState().SetTransform(matrix);
  SetIsTransformInvertible(matrix.IsInvertible());
}

ALWAYS_INLINE bool BaseRenderingContext2D::IsFullCanvasCompositeMode(
    SkBlendMode op) {
  // See 4.8.11.1.3 Compositing
  // CompositeSourceAtop and CompositeDestinationOut are not listed here as the
  // platforms already implement the specification's behavior.
  return op == SkBlendMode::kSrcIn || op == SkBlendMode::kSrcOut ||
         op == SkBlendMode::kDstIn || op == SkBlendMode::kDstATop;
}

ALWAYS_INLINE bool BaseRenderingContext2D::StateHasFilter() {
  const CanvasRenderingContext2DState& state = GetState();
  if (state.IsFilterUnresolved()) [[unlikely]] {
    return !!StateGetFilter();
  }
  // The fast path avoids the virtual call overhead of StateGetFilter
  return state.IsFilterResolved();
}

ALWAYS_INLINE bool BaseRenderingContext2D::ComputeDirtyRect(
    const gfx::RectF& local_rect,
    const SkIRect& transformed_clip_bounds,
    SkIRect* dirty_rect) {
  DCHECK(dirty_rect);
  const CanvasRenderingContext2DState& state = GetState();
  gfx::RectF canvas_rect = state.GetTransform().MapRect(local_rect);

  if (!state.ShadowColor().IsFullyTransparent()) [[unlikely]] {
    gfx::RectF shadow_rect(canvas_rect);
    shadow_rect.Offset(state.ShadowOffset());
    shadow_rect.Outset(ClampTo<float>(state.ShadowBlur()));
    canvas_rect.Union(shadow_rect);
  }

  gfx::RectFToSkRect(canvas_rect).roundOut(dirty_rect);
  if (!dirty_rect->intersect(transformed_clip_bounds)) [[unlikely]] {
    return false;
  }

  return true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_BASE_RENDERING_CONTEXT_2D_H_
