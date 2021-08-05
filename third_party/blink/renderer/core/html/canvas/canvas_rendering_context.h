/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_

#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

class CanvasImageSource;
class HTMLCanvasElement;
class ImageBitmap;
class
    V8UnionCanvasRenderingContext2DOrGPUCanvasContextOrImageBitmapRenderingContextOrWebGL2RenderingContextOrWebGLRenderingContext;
class
    V8UnionGPUCanvasContextOrImageBitmapRenderingContextOrOffscreenCanvasRenderingContext2DOrWebGL2RenderingContextOrWebGLRenderingContext;

class CORE_EXPORT CanvasRenderingContext
    : public ScriptWrappable,
      public ActiveScriptWrappable<CanvasRenderingContext>,
      public Thread::TaskObserver {
  USING_PRE_FINALIZER(CanvasRenderingContext, Dispose);

 public:
  CanvasRenderingContext(const CanvasRenderingContext&) = delete;
  CanvasRenderingContext& operator=(const CanvasRenderingContext&) = delete;
  ~CanvasRenderingContext() override = default;

  // A Canvas can either be "2D" or "webgl" but never both. Requesting a context
  // with a type different from an existing will destroy the latter.
  enum ContextType {
    // These values are mirrored in tools/metrics/histograms/enums.xml. Do
    // not change assigned numbers of existing items and add new features to the
    // end of the list.
    kContext2D = 0,
    kContextExperimentalWebgl = 2,
    kContextWebgl = 3,
    kContextWebgl2 = 4,
    kContextImageBitmap = 5,
    kContextXRPresent = 6,
    // WebGL2Compute used to be 7.
    kContextWebGPU = 8,  // WebGPU
    kContextTypeUnknown = 9,
    kMaxValue = kContextTypeUnknown,
  };

  // Correspond to CanvasRenderingAPI defined in
  // tools/metrics/histograms/enums.xml
  enum class CanvasRenderingAPI {
    k2D = 0,
    kWebgl = 1,
    kWebgl2 = 2,
    kBitmaprenderer = 3,
    kWebgpu = 4,
  };

  bool IsRenderingContext2D() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::k2D;
  }
  bool IsImageBitmapRenderingContext() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kBitmaprenderer;
  }
  bool IsWebGL() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgl ||
           canvas_rendering_type_ == CanvasRenderingAPI::kWebgl2;
  }
  bool IsWebGL2() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgl2;
  }
  bool IsWebGPU() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgpu;
  }

  // ActiveScriptWrappable
  // As this class inherits from ActiveScriptWrappable, as long as
  // HasPendingActivity returns true, we can ensure that the Garbage Collector
  // won't try to collect this class. This is needed specifically for the
  // offscreencanvas use case.
  bool HasPendingActivity() const override { return false; }
  ExecutionContext* GetExecutionContext() const {
    if (!Host())
      return nullptr;
    return Host()->GetTopExecutionContext();
  }

  void RecordUKMCanvasRenderingAPI();

  // This is only used in WebGL
  void RecordUKMCanvasDrawnToRenderingAPI();

  static ContextType ContextTypeFromId(
      const String& id,
      const ExecutionContext* execution_context);
  static ContextType ResolveContextTypeAliases(ContextType);

  CanvasRenderingContextHost* Host() const { return host_; }

  // TODO(https://crbug.com/1208480): This function applies only to 2D rendering
  // contexts, and should be removed.
  virtual CanvasColorParams CanvasRenderingContextColorParams() const {
    return CanvasColorParams();
  }

  virtual scoped_refptr<StaticBitmapImage> GetImage() = 0;
  virtual ContextType GetContextType() const = 0;
  virtual bool IsComposited() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool IsOriginTopLeft() const {
    // Canvas contexts have the origin of coordinates on the top left corner.
    // Accelerated resources (e.g. GPU textures) have their origin of
    // coordinates in the upper left corner.
    return !IsAccelerated();
  }
  virtual bool ShouldAntialias() const { return false; }
  // Indicates whether the entire tab is backgrounded. Passing false
  // to this method may cause some canvas context implementations to
  // aggressively discard resources, which is not desired for canvases
  // which are being rendered to, just not being displayed in the
  // page.
  virtual void SetIsInHiddenPage(bool) = 0;
  // Indicates whether the canvas is being displayed in the page;
  // i.e., doesn't have display:none, and is visible. The initial
  // value for all context types is assumed to be false; this will be
  // called when the context is first displayed.
  virtual void SetIsBeingDisplayed(bool) = 0;
  virtual bool isContextLost() const { return true; }
  // TODO(fserb): remove AsV8RenderingContext and AsV8OffscreenRenderingContext.
  virtual V8UnionCanvasRenderingContext2DOrGPUCanvasContextOrImageBitmapRenderingContextOrWebGL2RenderingContextOrWebGLRenderingContext*
  AsV8RenderingContext() {
    NOTREACHED();
    return nullptr;
  }
  virtual V8UnionGPUCanvasContextOrImageBitmapRenderingContextOrOffscreenCanvasRenderingContext2DOrWebGL2RenderingContextOrWebGLRenderingContext*
  AsV8OffscreenRenderingContext() {
    NOTREACHED();
    return nullptr;
  }
  virtual bool IsPaintable() const = 0;
  void DidDraw(CanvasPerformanceMonitor::DrawType draw_type) {
    return DidDraw(Host() ? SkIRect::MakeWH(Host()->width(), Host()->height())
                          : SkIRect::MakeEmpty(),
                   draw_type);
  }
  void DidDraw(const SkIRect& dirty_rect, CanvasPerformanceMonitor::DrawType);

  // Return true if the content is updated.
  virtual bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) {
    return false;
  }

  virtual bool CopyRenderingResultsFromDrawingBuffer(CanvasResourceProvider*,
                                                     SourceDrawingBuffer) {
    return false;
  }

  virtual cc::Layer* CcLayer() const { return nullptr; }

  enum LostContextMode {
    kNotLostContext,

    // Lost context occurred at the graphics system level.
    kRealLostContext,

    // Lost context provoked by WEBGL_lose_context.
    kWebGLLoseContextLostContext,

    // Lost context occurred due to internal implementation reasons.
    kSyntheticLostContext,
  };
  virtual void LoseContext(LostContextMode) {}
  virtual void SendContextLostEventIfNeeded() {}

  // This method gets called at the end of script tasks that modified
  // the contents of the canvas (called didDraw). It marks the completion
  // of a presentable frame.
  virtual void FinalizeFrame() {}

  // Thread::TaskObserver implementation
  void DidProcessTask(const base::PendingTask&) override;
  void WillProcessTask(const base::PendingTask&, bool) final {}

  // Canvas2D-specific interface
  virtual void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const {}
  virtual void Reset() {}
  virtual void ClearRect(double x, double y, double width, double height) {}
  virtual void DidSetSurfaceSize() {}
  virtual void SetShouldAntialias(bool) {}
  virtual unsigned HitRegionsCount() const { return 0; }
  virtual void setFont(const String&) {}
  virtual void StyleDidChange(const ComputedStyle* old_style,
                              const ComputedStyle& new_style) {}
  virtual HitTestCanvasResult* GetControlAndIdIfHitRegionExists(
      const PhysicalOffset& location) {
    NOTREACHED();
    return MakeGarbageCollected<HitTestCanvasResult>(String(), nullptr);
  }
  virtual String GetIdFromControl(const Element* element) { return String(); }
  virtual void ResetUsageTracking() {}

  // WebGL-specific interface
  virtual bool UsingSwapChain() const { return false; }
  virtual void SetFilterQuality(cc::PaintFlags::FilterQuality) { NOTREACHED(); }
  virtual void Reshape(int width, int height) {}
  virtual void MarkLayerComposited() { NOTREACHED(); }
  virtual sk_sp<SkData> PaintRenderingResultsToDataArray(SourceDrawingBuffer) {
    NOTREACHED();
    return nullptr;
  }
  virtual int ExternallyAllocatedBufferCountPerPixel() {
    NOTREACHED();
    return 0;
  }
  virtual IntSize DrawingBufferSize() const {
    NOTREACHED();
    return IntSize(0, 0);
  }

  // OffscreenCanvas-specific methods.
  virtual bool PushFrame() { return false; }
  virtual ImageBitmap* TransferToImageBitmap(ScriptState*) { return nullptr; }


  bool WouldTaintOrigin(CanvasImageSource*);
  void DidMoveToNewDocument(Document*);

  void DetachHost() { host_ = nullptr; }

  const CanvasContextCreationAttributesCore& CreationAttributes() const {
    return creation_attributes_;
  }

  void Trace(Visitor*) const override;
  virtual void Stop() = 0;

  virtual IdentifiableToken IdentifiableTextToken() const {
    // Token representing no bytes.
    return IdentifiableToken(base::span<const uint8_t>());
  }

  virtual bool IdentifiabilityEncounteredSkippedOps() const { return false; }

  virtual bool IdentifiabilityEncounteredSensitiveOps() const { return false; }

  static CanvasPerformanceMonitor& GetCanvasPerformanceMonitor();

  virtual bool IdentifiabilityEncounteredPartiallyDigestedImage() const {
    return false;
  }

 protected:
  CanvasRenderingContext(CanvasRenderingContextHost*,
                         const CanvasContextCreationAttributesCore&,
                         CanvasRenderingAPI);

 private:
  void Dispose();

  Member<CanvasRenderingContextHost> host_;
  CanvasColorParams color_params_;
  CanvasContextCreationAttributesCore creation_attributes_;

  void RenderTaskEnded();
  bool did_draw_in_current_task_ = false;

  const CanvasRenderingAPI canvas_rendering_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_
