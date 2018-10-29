// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_OFFSCREENCANVAS_OFFSCREEN_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_OFFSCREENCANVAS_OFFSCREEN_CANVAS_H_

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_encode_options.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CanvasContextCreationAttributesCore;
class CanvasResourceProvider;
class ImageBitmap;
#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
class
    OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrWebGL2ComputeRenderingContext;
typedef OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrWebGL2ComputeRenderingContext
    OffscreenRenderingContext;
#else
class
    OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext;
typedef OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext
    OffscreenRenderingContext;
#endif

class CORE_EXPORT OffscreenCanvas final
    : public EventTargetWithInlineData,
      public ImageBitmapSource,
      public CanvasRenderingContextHost,
      public CanvasResourceDispatcherClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(OffscreenCanvas);
  USING_PRE_FINALIZER(OffscreenCanvas, Dispose);

 public:
  static OffscreenCanvas* Create(unsigned width, unsigned height);
  ~OffscreenCanvas() override;
  void Dispose();

  bool IsOffscreenCanvas() const override { return true; }
  // IDL attributes
  unsigned width() const { return size_.Width(); }
  unsigned height() const { return size_.Height(); }
  void setWidth(unsigned);
  void setHeight(unsigned);

  // CanvasResourceDispatcherClient
  void BeginFrame() override;

  // API Methods
  ImageBitmap* transferToImageBitmap(ScriptState*, ExceptionState&);

  const IntSize& Size() const override { return size_; }
  void SetSize(const IntSize&);
  void RecordTransfer();

  void SetPlaceholderCanvasId(DOMNodeId canvas_id);
  DOMNodeId PlaceholderCanvasId() const { return placeholder_canvas_id_; }
  bool HasPlaceholderCanvas() const;
  bool IsNeutered() const override { return is_neutered_; }
  void SetNeutered();
  CanvasRenderingContext* GetCanvasRenderingContext(
      ExecutionContext*,
      const String&,
      const CanvasContextCreationAttributesCore&);

  static void RegisterRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);

  bool OriginClean() const override;
  void SetOriginTainted() override { origin_clean_ = false; }
  // TODO(crbug.com/630356): apply the flag to WebGL context as well
  void SetDisableReadingFromCanvasTrue() {
    disable_reading_from_canvas_ = true;
  }

  CanvasResourceProvider* GetOrCreateResourceProvider();
  void DiscardResourceProvider() override;

  void SetFrameSinkId(uint32_t client_id, uint32_t sink_id) {
    client_id_ = client_id;
    sink_id_ = sink_id;
  }
  uint32_t ClientId() const { return client_id_; }
  uint32_t SinkId() const { return sink_id_; }

  // CanvasRenderingContextHost implementation.
  void FinalizeFrame() override {}
  void DetachContext() override { context_ = nullptr; }
  CanvasRenderingContext* RenderingContext() const override { return context_; }
  void PushFrameIfNeeded();
  void PushFrame(scoped_refptr<CanvasResource> frame,
                 const SkIRect& damage_rect) override;
  void DidDraw(const FloatRect&) override;
  void DidDraw() override;
  void Commit(scoped_refptr<CanvasResource> bitmap_image,
              const SkIRect& damage_rect) override;
  bool ShouldAccelerate2dContext() const override;
  unsigned GetMSAASampleCountFor2dContext() const override { return 0; }
  CanvasResourceDispatcher* GetOrCreateResourceDispatcher() override;

  // Partial CanvasResourceHost implementation
  void NotifyGpuContextLost() override {}
  void SetNeedsCompositingUpdate() override {}
  // TODO(fserb): Merge this with HTMLCanvasElement::UpdateMemoryUsage
  void UpdateMemoryUsage() override;
  SkFilterQuality FilterQuality() const override {
    return kLow_SkFilterQuality;  // TODO(crbug.com/856654)
  }

  // EventTarget implementation
  const AtomicString& InterfaceName() const final {
    return EventTargetNames::OffscreenCanvas;
  }
  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }

  const KURL& GetExecutionContextUrl() const override {
    return GetExecutionContext()->Url();
  }

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const final;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  base::Optional<IntRect>,
                                  const ImageBitmapOptions&) final;

  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) final;
  bool WouldTaintOrigin(const SecurityOrigin*) const final {
    return !origin_clean_;
  }
  FloatSize ElementSize(const FloatSize& default_object_size) const final {
    return FloatSize(width(), height());
  }
  bool IsOpaque() const final;
  bool IsAccelerated() const final;

  DispatchEventResult HostDispatchEvent(Event* event) override {
    return DispatchEvent(*event);
  }

  bool IsWebGL1Enabled() const override { return true; }
  bool IsWebGL2Enabled() const override { return true; }
  bool IsWebGLBlocked() const override { return false; }

  FontSelector* GetFontSelector() override;

  void Trace(blink::Visitor*) override;

 private:
  int32_t memory_usage_ = 0;

  friend class OffscreenCanvasTest;
  explicit OffscreenCanvas(const IntSize&);
  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& RenderingContextFactories();
  static CanvasRenderingContextFactory* GetRenderingContextFactory(int);

  Member<CanvasRenderingContext> context_;
  WeakMember<ExecutionContext> execution_context_;

  DOMNodeId placeholder_canvas_id_ = kInvalidDOMNodeId;

  IntSize size_;
  bool is_neutered_ = false;

  bool origin_clean_ = true;
  bool disable_reading_from_canvas_ = false;

  std::unique_ptr<CanvasResourceDispatcher> frame_dispatcher_;

  SkIRect current_frame_damage_rect_;

  bool needs_matrix_clip_restore_ = false;
  bool needs_push_frame_ = false;

  // cc::FrameSinkId is broken into two integer components as this can be used
  // in transfer of OffscreenCanvas across threads
  // If this object is not created via
  // HTMLCanvasElement.transferControlToOffscreen(),
  // then the following members would remain as initialized zero values.
  uint32_t client_id_ = 0;
  uint32_t sink_id_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_OFFSCREENCANVAS_OFFSCREEN_CANVAS_H_
