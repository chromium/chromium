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
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

class CanvasImageSource;
class HTMLCanvasElement;
class ImageBitmap;

constexpr const char* kSRGBCanvasColorSpaceName = "srgb";
constexpr const char* kLinearRGBCanvasColorSpaceName = "linear-rgb";
constexpr const char* kRec2020CanvasColorSpaceName = "rec2020";
constexpr const char* kP3CanvasColorSpaceName = "p3";

constexpr const char* kRGBA8CanvasPixelFormatName = "uint8";
constexpr const char* kF16CanvasPixelFormatName = "float16";

class CORE_EXPORT CanvasRenderingContext : public ScriptWrappable,
                                           public Thread::TaskObserver {
  USING_PRE_FINALIZER(CanvasRenderingContext, Dispose);

 public:
  ~CanvasRenderingContext() override = default;

  // A Canvas can either be "2D" or "webgl" but never both. Requesting a context
  // with a type different from an existing will destroy the latter.
  enum ContextType {
    // Do not change assigned numbers of existing items: add new features to the
    // end of the list.
    kContext2d = 0,
    kContextExperimentalWebgl = 2,
    kContextWebgl = 3,
    kContextWebgl2 = 4,
    kContextImageBitmap = 5,
    kContextXRPresent = 6,
    kContextWebgl2Compute = 7,
    kContextTypeUnknown = 8,
    kMaxValue = kContextTypeUnknown,
  };

  static ContextType ContextTypeFromId(const String& id);
  static ContextType ResolveContextTypeAliases(ContextType);

  CanvasRenderingContextHost* Host() const { return host_; }

  WTF::String ColorSpaceAsString() const;
  WTF::String PixelFormatAsString() const;

  const CanvasColorParams& ColorParams() const { return color_params_; }

  virtual scoped_refptr<StaticBitmapImage> GetImage(AccelerationHint) const = 0;
  virtual ContextType GetContextType() const = 0;
  virtual bool IsComposited() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool IsOriginTopLeft() const {
    // Canvas contexts have the origin of coordinates on the top left corner.
    // Accelerated resources (e.g. GPU textures) have their origin of
    // coordinates in the uppper left corner.
    return !IsAccelerated();
  }
  virtual bool ShouldAntialias() const { return false; }
  virtual void SetIsHidden(bool) = 0;
  virtual bool isContextLost() const { return true; }
  // TODO(fserb): remove SetCanvasGetContextResult.
  virtual void SetCanvasGetContextResult(RenderingContext&) { NOTREACHED(); };
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

  void NeedsFinalizeFrame();

  // Thread::TaskObserver implementation
  void DidProcessTask(const base::PendingTask&) override;
  void WillProcessTask(const base::PendingTask&) final {}

  // Canvas2D-specific interface
  virtual bool Is2d() const { return false; }
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
      const LayoutPoint& location) {
    NOTREACHED();
    return HitTestCanvasResult::Create(String(), nullptr);
  }
  virtual String GetIdFromControl(const Element* element) { return String(); }
  virtual void ResetUsageTracking(){};

  // WebGL-specific interface
  virtual bool Is3d() const { return false; }
  virtual void SetFilterQuality(SkFilterQuality) { NOTREACHED(); }
  virtual void Reshape(int width, int height) { NOTREACHED(); }
  virtual void MarkLayerComposited() { NOTREACHED(); }
  virtual scoped_refptr<Uint8Array> PaintRenderingResultsToDataArray(
      SourceDrawingBuffer) {
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
  virtual void PushFrame() {}
  virtual ImageBitmap* TransferToImageBitmap(ScriptState*) { return nullptr; }

  bool WouldTaintOrigin(CanvasImageSource*, const SecurityOrigin*);
  void DidMoveToNewDocument(Document*);

  void DetachHost() { host_ = nullptr; }

  const CanvasContextCreationAttributesCore& CreationAttributes() const {
    return creation_attributes_;
  }

  void Trace(blink::Visitor*) override;
  virtual void Stop() = 0;

 protected:
  CanvasRenderingContext(CanvasRenderingContextHost*,
                         const CanvasContextCreationAttributesCore&);

 private:
  void Dispose();

  Member<CanvasRenderingContextHost> host_;
  HashSet<String> clean_urls_;
  HashSet<String> dirty_urls_;
  CanvasColorParams color_params_;
  CanvasContextCreationAttributesCore creation_attributes_;
  bool finalize_frame_scheduled_ = false;

  DISALLOW_COPY_AND_ASSIGN(CanvasRenderingContext);
};

}  // namespace blink

#endif
