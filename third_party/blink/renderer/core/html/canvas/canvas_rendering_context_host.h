// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_

#include "base/byte_size.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/ukm_parameters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
}

namespace blink {

class CanvasRenderingContext;
class CanvasResource;
class CanvasResourceDispatcher;
class ComputedStyle;
class KURL;
class LayoutLocale;
class PlainTextPainter;
class StaticBitmapImage;
class UniqueFontSelector;

enum class RasterModeHint {
  kPreferGPU,
  kPreferCPU,
};

class CORE_EXPORT CanvasRenderingContextHost
    : public GarbageCollectedMixin,
      public CanvasResourceProvider::Delegate,
      public CanvasImageSource,
      public ImageBitmapSource {
 public:
  enum class HostType {
    kNone,
    kCanvasHost,
    kOffscreenCanvasHost,
  };
  CanvasRenderingContextHost(HostType host_type, const gfx::Size& size);
  void Trace(Visitor* visitor) const override;

  void RecordCanvasSizeToUMA();

  virtual void DetachContext() = 0;

  virtual void DidDraw(const SkIRect& rect) = 0;
  void DidDraw() { DidDraw(SkIRect::MakeWH(width(), height())); }

  virtual void PostFinalizeFrame(FlushReason) = 0;
  void NotifyCachesOfSwitchingFrame();
  virtual bool PushFrame(scoped_refptr<CanvasResource>&& frame,
                         const SkIRect& damage_rect) = 0;
  virtual bool OriginClean() const = 0;
  virtual void SetOriginTainted() = 0;
  virtual CanvasRenderingContext* RenderingContext() const = 0;
  virtual CanvasResourceDispatcher* GetOrCreateResourceDispatcher() = 0;
  virtual void DiscardResourceDispatcher() = 0;

  virtual ExecutionContext* GetTopExecutionContext() const = 0;
  virtual DispatchEventResult HostDispatchEvent(Event*) = 0;
  virtual const KURL& GetExecutionContextUrl() const = 0;

  void UpdateMemoryUsage();
  base::ByteSize GetMemoryUsage() const { return externally_allocated_memory_; }

  // Initialize the indicated cc::Layer with the HTMLCanvasElement's CSS
  // properties. This is a no-op if `this` is not an HTMLCanvasElement.
  virtual void InitializeLayerWithCSSProperties(cc::Layer* layer) {}

  // If WebGL1 is disabled by enterprise policy or command line switch.
  virtual bool IsWebGL1Enabled() const = 0;
  // If WebGL2 is disabled by enterprise policy or command line switch.
  virtual bool IsWebGL2Enabled() const = 0;
  // If WebGL is temporarily blocked because WebGL contexts were lost one or
  // more times, in particular, via the GL_ARB_robustness extension.
  virtual bool IsWebGLBlocked() const = 0;
  virtual void SetContextCreationWasBlocked() {}

  // The ComputedStyle argument is optional. Use it if you already have the
  // computed style for the host. If nullptr is passed, the style will be
  // computed within the method.
  virtual TextDirection GetTextDirection(const ComputedStyle*) = 0;
  virtual const LayoutLocale* GetLocale() const = 0;
  virtual UniqueFontSelector* GetFontSelector() = 0;

  virtual bool ShouldAccelerate2dContext() const = 0;

  virtual UkmParameters GetUkmParameters() = 0;

  bool IsValidImageSize() const;
  bool IsPaintable() const;

  virtual bool LowLatencyEnabled() const { return false; }

  virtual void SetTransferToGPUTextureWasInvoked() {}

  // Required by template functions in WebGLRenderingContextBase
  int width() const { return Size().width(); }
  int height() const { return Size().height(); }

  // Partial CanvasResourceProvider::Delegate implementation
  void InitializeForRecording(cc::PaintCanvas*) const final;

  virtual void PageVisibilityChanged();

  bool IsWebGL() const;
  bool IsWebGPU() const;
  bool IsRenderingContext2D() const;
  bool IsImageBitmapRenderingContext() const;

  SkAlphaType GetRenderingContextAlphaType() const;
  viz::SharedImageFormat GetRenderingContextFormat() const;
  gfx::ColorSpace GetRenderingContextColorSpace() const;
  PlainTextPainter& GetPlainTextPainter();

  // Actual RasterMode used for rendering 2d primitives.
  RasterMode GetRasterModeForCanvas2D() const;

  virtual bool IsPageVisible() const = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void ClearCanvas2DLayerTexture() {}

  // blink::CanvasImageSource
  bool IsOffscreenCanvas() const override;
  bool IsAccelerated() const override;

  // ImageBitmapSource implementation
  ImageBitmapSourceStatus CheckUsability() const override;

  gfx::Size Size() const { return size_; }

  bool ShouldTryToUseGpuRaster() const;
  void SetPreferred2DRasterMode(RasterModeHint);

  virtual void DiscardResources() = 0;

 protected:
  ~CanvasRenderingContextHost() override;

  scoped_refptr<StaticBitmapImage> CreateTransparentImage() const;

  bool ContextHasOpenLayers(const CanvasRenderingContext*) const;

  Member<PlainTextPainter> plain_text_painter_;
  Member<UniqueFontSelector> unique_font_selector_;
  gfx::Size size_;

 private:

  bool did_record_canvas_size_to_uma_ = false;
  HostType host_type_ = HostType::kNone;
  RasterModeHint preferred_2d_raster_mode_ = RasterModeHint::kPreferCPU;

  // GPU Memory Management
  base::ByteSize externally_allocated_memory_;
  // NO_UNIQUE_ADDRESS allows making this member empty in production.
  NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase external_memory_accounter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_HOST_H_
