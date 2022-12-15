// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/ukm_parameters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/size.h"

class SkColorInfo;

namespace blink {

class CanvasRenderingContext;
class CanvasResource;
class CanvasResourceDispatcher;
class FontSelector;
class ImageEncodeOptions;
class KURL;
class ScriptState;
class StaticBitmapImage;

class CORE_EXPORT CanvasRenderingContextHost : public CanvasResourceHost,
                                               public CanvasImageSource,
                                               public GarbageCollectedMixin {
 public:
  enum class HostType {
    kNone,
    kCanvasHost,
    kOffscreenCanvasHost,
  };
  explicit CanvasRenderingContextHost(HostType host_type);

  void RecordCanvasSizeToUMA(const gfx::Size&);

  virtual void DetachContext() = 0;

  virtual void DidDraw(const SkIRect& rect) = 0;
  void DidDraw() { DidDraw(SkIRect::MakeWH(width(), height())); }

  virtual void PreFinalizeFrame() = 0;
  virtual void PostFinalizeFrame() = 0;
  virtual bool PushFrame(scoped_refptr<CanvasResource>&& frame,
                         const SkIRect& damage_rect) = 0;
  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;
  virtual const gfx::Size& Size() const = 0;
  virtual CanvasRenderingContext* RenderingContext() const = 0;
  virtual CanvasResourceDispatcher* GetOrCreateResourceDispatcher() = 0;

  virtual ExecutionContext* GetTopExecutionContext() const = 0;
  virtual DispatchEventResult HostDispatchEvent(Event*) = 0;
  virtual const KURL& GetExecutionContextUrl() const = 0;

  // If WebGL1 is disabled by enterprise policy or command line switch.
  virtual bool IsWebGL1Enabled() const = 0;
  // If WebGL2 is disabled by enterprise policy or command line switch.
  virtual bool IsWebGL2Enabled() const = 0;
  // If WebGL is temporarily blocked because WebGL contexts were lost one or
  // more times, in particular, via the GL_ARB_robustness extension.
  virtual bool IsWebGLBlocked() const = 0;
  virtual void SetContextCreationWasBlocked() {}

  virtual FontSelector* GetFontSelector() = 0;

  virtual bool ShouldAccelerate2dContext() const = 0;

  virtual void Commit(scoped_refptr<CanvasResource>&& canvas_resource,
                      const SkIRect& damage_rect);

  virtual UkmParameters GetUkmParameters() = 0;

  // For deferred canvases this will have the side effect of drawing recorded
  // commands in order to finalize the frame.
  ScriptPromise convertToBlob(ScriptState*,
                              const ImageEncodeOptions*,
                              ExceptionState&,
                              const CanvasRenderingContext* const context);

  bool IsPaintable() const;

  bool PrintedInCurrentTask() const final;

  // Required by template functions in WebGLRenderingContextBase
  int width() const { return Size().width(); }
  int height() const { return Size().height(); }

  // Partial CanvasResourceHost implementation
  void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const final;
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) final;
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) override;

  bool IsWebGL() const;
  bool IsWebGPU() const;
  bool IsRenderingContext2D() const;
  bool IsImageBitmapRenderingContext() const;

  // Returns an SkColorInfo that best represents the canvas rendering context's
  // contents.
  SkColorInfo GetRenderingContextSkColorInfo() const;

  // blink::CanvasImageSource
  bool IsOffscreenCanvas() const override;

 protected:
  ~CanvasRenderingContextHost() override = default;

  scoped_refptr<StaticBitmapImage> CreateTransparentImage(
      const gfx::Size&) const;

  void CreateCanvasResourceProvider2D(RasterModeHint hint);
  void CreateCanvasResourceProviderWebGL();
  void CreateCanvasResourceProviderWebGPU();

  // Computes the digest that corresponds to the "input" of this canvas,
  // including the context type, and if applicable, canvas digest, and taint
  // bits.
  IdentifiableToken IdentifiabilityInputDigest(
      const CanvasRenderingContext* const context) const;

  bool did_fail_to_create_resource_provider_ = false;
  bool did_record_canvas_size_to_uma_ = false;
  HostType host_type_ = HostType::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
