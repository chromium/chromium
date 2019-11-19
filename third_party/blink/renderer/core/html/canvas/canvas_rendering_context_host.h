// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/image_encode_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CanvasColorParams;
class CanvasRenderingContext;
class CanvasResource;
class CanvasResourceDispatcher;
class FontSelector;
class KURL;
class StaticBitmapImage;

class CORE_EXPORT CanvasRenderingContextHost : public CanvasResourceHost,
                                               public CanvasImageSource,
                                               public GarbageCollectedMixin {
 public:
  enum HostType {
    kNone,
    kCanvasHost,
    kOffscreenCanvasHost,
  };
  CanvasRenderingContextHost(HostType host_type);

  void RecordCanvasSizeToUMA(const IntSize&);

  virtual void DetachContext() = 0;

  virtual void DidDraw(const FloatRect& rect) = 0;
  virtual void DidDraw() = 0;

  virtual void PreFinalizeFrame() = 0;
  virtual void PostFinalizeFrame() = 0;
  virtual bool PushFrame(scoped_refptr<CanvasResource> frame,
                         const SkIRect& damage_rect) = 0;
  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;
  virtual const IntSize& Size() const = 0;
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
  virtual unsigned GetMSAASampleCountFor2dContext() const = 0;

  virtual bool IsNeutered() const { return false; }

  virtual void Commit(scoped_refptr<CanvasResource> canvas_resource,
                      const SkIRect& damage_rect);

  bool IsPaintable() const;

  // Required by template functions in WebGLRenderingContextBase
  int width() const { return Size().Width(); }
  int height() const { return Size().Height(); }

  // Partial CanvasResourceHost implementation
  void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const final;
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      AccelerationHint hint) final;
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      AccelerationHint hint) override;

  bool Is3d() const;
  bool Is2d() const;
  CanvasColorParams ColorParams() const;

  // For deferred canvases this will have the side effect of drawing recorded
  // commands in order to finalize the frame.
  ScriptPromise convertToBlob(ScriptState*,
                              const ImageEncodeOptions*,
                              ExceptionState&);

  // blink::CanvasImageSource
  bool IsOffscreenCanvas() const override;

 protected:
  ~CanvasRenderingContextHost() override {}

  scoped_refptr<StaticBitmapImage> CreateTransparentImage(const IntSize&) const;

  bool did_fail_to_create_resource_provider_ = false;
  bool did_record_canvas_size_to_uma_ = false;
  HostType host_type_ = kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
