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

#include "base/macros.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
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

constexpr const char* kSRGBCanvasColorSpaceName = "srgb";
constexpr const char* kRec2020CanvasColorSpaceName = "rec2020";
constexpr const char* kP3CanvasColorSpaceName = "p3";

constexpr const char* kRGBA8CanvasPixelFormatName = "uint8";
constexpr const char* kBGRA8CanvasPixelFormatName = "uint8";
constexpr const char* kF16CanvasPixelFormatName = "float16";

class CORE_EXPORT CanvasRenderingContext : public ScriptWrappable,
                                           public Thread::TaskObserver {
  USING_PRE_FINALIZER(CanvasRenderingContext, Dispose);

 public:
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
    kContextGPUPresent = 8,
    kContextTypeUnknown = 9,
    kMaxValue = kContextTypeUnknown,
  };

  // Correspond to CanvasRenderingAPI defined in
  // tools/metrics/histograms/enums.xml
  enum CanvasRenderingAPI {
    k2D = 0,
    kWebgl = 1,
    kWebgl2 = 2,
    kBitmaprenderer = 3,
    kWebgpu = 4,
  };

  void RecordUKMCanvasRenderingAPI(CanvasRenderingAPI canvasRenderingAPI);

  static ContextType ContextTypeFromId(const String& id);
  static ContextType ResolveContextTypeAliases(ContextType);

  CanvasRenderingContextHost* Host() const { return host_; }

  WTF::String ColorSpaceAsString() const;
  WTF::String PixelFormatAsString() const;

  const CanvasColorParams& CanvasRenderingContextColorParams() const {
    return color_params_;
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
  // TODO(fserb): remove SetCanvasGetContextResult.
  virtual void SetCanvasGetContextResult(RenderingContext&) { NOTREACHED(); }
  virtual void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) {
    NOTREACHED();
  }
  virtual bool IsPaintable() const = 0;
  virtual void DidDraw(const SkIRect& dirty_rect);
  virtual void DidDraw();

  // Return true if the content is updated.
  virtual bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) {
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

  // This method gets called at the end of script tasks that modified
  // the contents of the canvas (called didDraw). It marks the completion
  // of a presentable frame.
  virtual void FinalizeFrame() {}

  // Thread::TaskObserver implementation
  void DidProcessTask(const base::PendingTask&) override;
  void WillProcessTask(const base::PendingTask&, bool) final {}

  // Canvas2D-specific interface
  virtual bool IsRenderingContext2D() const { return false; }
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
  virtual bool Is3d() const { return false; }
  virtual bool UsingSwapChain() const { return false; }
  virtual void SetFilterQuality(SkFilterQuality) { NOTREACHED(); }
  virtual void Reshape(int width, int height) { NOTREACHED(); }
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

  // OffscreenCanvas-specific methods
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

 protected:
  CanvasRenderingContext(CanvasRenderingContextHost*,
                         const CanvasContextCreationAttributesCore&);

 private:
  void Dispose();

  Member<CanvasRenderingContextHost> host_;
  CanvasColorParams color_params_;
  CanvasContextCreationAttributesCore creation_attributes_;

  void StartListeningForDidProcessTask();
  void StopListeningForDidProcessTask();
  bool listening_for_did_process_task_ = false;

  DISALLOW_COPY_AND_ASSIGN(CanvasRenderingContext);
};

}  // namespace blink

#endif
