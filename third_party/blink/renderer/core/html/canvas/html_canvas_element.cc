/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include <math.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_draw_listener.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/base/resource/scale_factor.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// These values come from the WhatWG spec.
constexpr int kDefaultCanvasWidth = 300;
constexpr int kDefaultCanvasHeight = 150;

// A default value of quality argument for toDataURL and toBlob
// It is in an invalid range (outside 0.0 - 1.0) so that it will not be
// misinterpreted as a user-input value
constexpr int kUndefinedQualityValue = -1.0;
constexpr int kMinimumAccelerated2dCanvasSize = 128 * 129;

}  // namespace

HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(html_names::kCanvasTag, document),
      ExecutionContextLifecycleObserver(GetExecutionContext()),
      PageVisibilityObserver(document.GetPage()),
      CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kCanvasHost,
          {document.UkmRecorder(), document.UkmSourceID()}),
      size_(kDefaultCanvasWidth, kDefaultCanvasHeight),
      context_creation_was_blocked_(false),
      ignore_reset_(false),
      origin_clean_(true),
      surface_layer_bridge_(nullptr),
      externally_allocated_memory_(0) {
  UseCounter::Count(document, WebFeature::kHTMLCanvasElement);
  GetDocument().IncrementNumberOfCanvases();
}

HTMLCanvasElement::~HTMLCanvasElement() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -externally_allocated_memory_);
}

void HTMLCanvasElement::Dispose() {
  if (OffscreenCanvasFrame()) {
    ReleaseOffscreenCanvasFrame();
  }
  // It's possible that the placeholder frame has been disposed but its ID still
  // exists. Make sure that it gets unregistered here
  UnregisterPlaceholderCanvas();

  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  frame_dispatcher_ = nullptr;

  if (context_) {
    UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.HasRendered", bool(ResourceProvider()));
    if (context_->Host()) {
      UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.IsComposited",
                            context_->IsComposited());
      context_->DetachHost();
    }
    context_ = nullptr;
  }

  if (canvas2d_bridge_) {
    canvas2d_bridge_->SetCanvasResourceHost(nullptr);
    canvas2d_bridge_ = nullptr;
  }

  if (surface_layer_bridge_) {
    // Observer has to be cleared out at this point. Otherwise the
    // SurfaceLayerBridge may call back into the observer which is undefined
    // behavior. In the worst case, the dead canvas element re-adds itself into
    // a data structure which may crash at a later point in time. See
    // https://crbug.com/976577.
    surface_layer_bridge_->ClearObserver();
  }
}

void HTMLCanvasElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kWidthAttr ||
      params.name == html_names::kHeightAttr)
    Reset();
  HTMLElement::ParseAttribute(params);
}

LayoutObject* HTMLCanvasElement::CreateLayoutObject(const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  if (GetExecutionContext() &&
      GetExecutionContext()->CanExecuteScripts(kNotAboutToExecuteScript)) {
    // Allocation of a layout object indicates that the canvas doesn't
    // have display:none set, so is conceptually being displayed.
    if (context_) {
      context_->SetIsBeingDisplayed(style.Visibility() ==
                                    EVisibility::kVisible);
    }
    return new LayoutHTMLCanvas(this);
  }
  return HTMLElement::CreateLayoutObject(style, legacy);
}

Node::InsertionNotificationRequest HTMLCanvasElement::InsertedInto(
    ContainerNode& node) {
  SetIsInCanvasSubtree(true);
  return HTMLElement::InsertedInto(node);
}

void HTMLCanvasElement::setHeight(unsigned value,
                                  ExceptionState& exception_state) {
  if (IsOffscreenCanvasRegistered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot resize canvas after call to transferControlToOffscreen().");
    return;
  }
  SetUnsignedIntegralAttribute(html_names::kHeightAttr, value,
                               kDefaultCanvasHeight);
}

void HTMLCanvasElement::setWidth(unsigned value,
                                 ExceptionState& exception_state) {
  if (IsOffscreenCanvasRegistered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot resize canvas after call to transferControlToOffscreen().");
    return;
  }
  SetUnsignedIntegralAttribute(html_names::kWidthAttr, value,
                               kDefaultCanvasWidth);
}

void HTMLCanvasElement::SetSize(const IntSize& new_size) {
  if (new_size == Size())
    return;
  ignore_reset_ = true;
  SetIntegralAttribute(html_names::kWidthAttr, new_size.Width());
  SetIntegralAttribute(html_names::kHeightAttr, new_size.Height());
  ignore_reset_ = false;
  Reset();
}

HTMLCanvasElement::ContextFactoryVector&
HTMLCanvasElement::RenderingContextFactories() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ContextFactoryVector, context_factories,
                      (CanvasRenderingContext::kMaxValue));
  return context_factories;
}

CanvasRenderingContextFactory* HTMLCanvasElement::GetRenderingContextFactory(
    int type) {
  DCHECK_LE(type, CanvasRenderingContext::kMaxValue);
  return RenderingContextFactories()[type].get();
}

void HTMLCanvasElement::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::ContextType type =
      rendering_context_factory->GetContextType();
  DCHECK_LE(type, CanvasRenderingContext::kMaxValue);
  DCHECK(!RenderingContextFactories()[type]);
  RenderingContextFactories()[type] = std::move(rendering_context_factory);
}

void HTMLCanvasElement::RecordIdentifiabilityMetric(
    IdentifiableSurface surface,
    IdentifiableToken value) const {
  blink::IdentifiabilityMetricBuilder(GetDocument().UkmSourceID())
      .Set(surface, value)
      .Record(GetDocument().UkmRecorder());
}

void HTMLCanvasElement::IdentifiabilityReportWithDigest(
    IdentifiableToken canvas_contents_token) const {
  if (IdentifiabilityStudySettings::Get()->ShouldSample(
          blink::IdentifiableSurface::Type::kCanvasReadback)) {
    RecordIdentifiabilityMetric(
        blink::IdentifiableSurface::FromTypeAndToken(
            blink::IdentifiableSurface::Type::kCanvasReadback,
            IdentifiabilityInputDigest(context_)),
        canvas_contents_token.ToUkmMetricValue());
  }
}

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContext(
    const String& type,
    const CanvasContextCreationAttributesCore& attributes) {
  auto* old_contents_cc_layer = ContentsCcLayer();
  auto* result = GetCanvasRenderingContextInternal(type, attributes);

  if (ContentsCcLayer() != old_contents_cc_layer)
    OnContentsCcLayerChanged();
  return result;
}

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContextInternal(
    const String& type,
    const CanvasContextCreationAttributesCore& attributes) {
  CanvasRenderingContext::ContextType context_type =
      CanvasRenderingContext::ContextTypeFromId(type);

  // Unknown type.
  if (context_type == CanvasRenderingContext::kContextTypeUnknown) {
    return nullptr;
  }

  // Log the aliased context type used.
  if (!context_) {
    if (IdentifiabilityStudySettings::Get()->IsWebFeatureAllowed(
          blink::WebFeature::kCanvasRenderingContext)) {
      RecordIdentifiabilityMetric(
          IdentifiableSurface::FromTypeAndToken(
              blink::IdentifiableSurface::Type::kWebFeature,
              blink::WebFeature::kCanvasRenderingContext),
          context_type);
    }
    UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.ContextType", context_type);
  }

  context_type =
      CanvasRenderingContext::ResolveContextTypeAliases(context_type);

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(context_type);
  if (!factory)
    return nullptr;

  // FIXME - The code depends on the context not going away once created, to
  // prevent JS from seeing a dangling pointer. So for now we will disallow the
  // context from being changed once it is created.
  if (context_) {
    if (context_->GetContextType() == context_type)
      return context_.Get();

    factory->OnError(this,
                     "Canvas has an existing context of a different type");
    return nullptr;
  }

  // If this context is cross-origin, it should prefer to use the low-power GPU
  LocalFrame* frame = GetDocument().GetFrame();
  CanvasContextCreationAttributesCore recomputed_attributes = attributes;
  if (frame && frame->IsCrossOriginToMainFrame())
    recomputed_attributes.power_preference = "low-power";

  context_ = factory->Create(this, recomputed_attributes);
  if (!context_)
    return nullptr;

  // Since the |context_| is created, free the transparent image,
  // |transparent_image_| created for this canvas if it exists.
  if (transparent_image_.get()) {
    transparent_image_.reset();
  }

  context_creation_was_blocked_ = false;

  probe::DidCreateCanvasContext(&GetDocument());

  if (Is3d())
    UpdateMemoryUsage();

  LayoutObject* layout_object = GetLayoutObject();
  if (layout_object) {
    const ComputedStyle* style = GetComputedStyle();
    if (style) {
      context_->SetIsBeingDisplayed(style->Visibility() ==
                                    EVisibility::kVisible);
    }

    if (IsRenderingContext2D() && !context_->CreationAttributes().alpha) {
      // In the alpha false case, canvas is initially opaque, so we need to
      // trigger an invalidation.
      DidDraw();
    }
  }

  if (context_->CreationAttributes().desynchronized) {
    CreateLayer();
    SetNeedsUnbufferedInputEvents(true);
    frame_dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        nullptr, surface_layer_bridge_->GetFrameSinkId().client_id(),
        surface_layer_bridge_->GetFrameSinkId().sink_id(),
        CanvasResourceDispatcher::kInvalidPlaceholderCanvasId, size_);
    // We don't actually need the begin frame signal when in low latency mode,
    // but we need to subscribe to it or else dispatching frames will not work.
    frame_dispatcher_->SetNeedsBeginFrame(GetPage()->IsPageVisible());

    UseCounter::Count(GetDocument(), WebFeature::kHTMLCanvasElementLowLatency);
  }

  // A 2D context does not know before lazy creation whether or not it is
  // direct composited. The Canvas2DLayerBridge will handle this
  if (!IsRenderingContext2D())
    SetNeedsCompositingUpdate();

  return context_.Get();
}

ScriptPromise HTMLCanvasElement::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions* options,
    ExceptionState& exception_state) {
  return CanvasRenderingContextHost::convertToBlob(script_state, options,
                                                   exception_state, context_);
}

bool HTMLCanvasElement::ShouldBeDirectComposited() const {
  return (context_ && context_->IsComposited()) || (!!surface_layer_bridge_);
}

bool HTMLCanvasElement::IsAccelerated() const {
  return context_ && context_->IsAccelerated();
}

bool HTMLCanvasElement::IsWebGL1Enabled() const {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;
  Settings* settings = frame->GetSettings();
  return settings && settings->GetWebGL1Enabled();
}

bool HTMLCanvasElement::IsWebGL2Enabled() const {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;
  Settings* settings = frame->GetSettings();
  return settings && settings->GetWebGL2Enabled();
}

bool HTMLCanvasElement::IsWebGLBlocked() const {
  Document& document = GetDocument();
  bool blocked = false;
  mojo::Remote<mojom::blink::GpuDataManager> gpu_data_manager;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      gpu_data_manager.BindNewPipeAndPassReceiver());
  gpu_data_manager->Are3DAPIsBlockedForUrl(document.Url(), &blocked);
  return blocked;
}

void HTMLCanvasElement::DidDraw(const FloatRect& rect) {
  if (rect.IsEmpty())
    return;
  if (GetLayoutObject() && GetLayoutObject()->PreviousVisibilityVisible() &&
      GetDocument().GetPage())
    GetDocument().GetPage()->Animator().SetHasCanvasInvalidation();
  canvas_is_clear_ = false;
  if (GetLayoutObject() && !LowLatencyEnabled())
    GetLayoutObject()->SetShouldCheckForPaintInvalidation();
  if (IsRenderingContext2D() && context_->ShouldAntialias() && GetPage() &&
      GetPage()->DeviceScaleFactorDeprecated() > 1.0f) {
    FloatRect inflated_rect = rect;
    inflated_rect.Inflate(1);
    dirty_rect_.Unite(inflated_rect);
  } else {
    dirty_rect_.Unite(rect);
  }
  if (IsRenderingContext2D() && canvas2d_bridge_)
    canvas2d_bridge_->DidDraw(rect);
}

void HTMLCanvasElement::DidDraw() {
  DidDraw(FloatRect(0, 0, Size().Width(), Size().Height()));
}

void HTMLCanvasElement::PreFinalizeFrame() {
  RecordCanvasSizeToUMA(size_);

  // PreFinalizeFrame indicates the end of a script task that may have rendered
  // into the canvas, now is a good time to unlock cache entries.
  auto* resource_provider = ResourceProvider();
  if (resource_provider)
    resource_provider->ReleaseLockedImages();

  // Low-latency 2d canvases produce their frames after the resource gets single
  // buffered.
  if (LowLatencyEnabled() && !dirty_rect_.IsEmpty() &&
      GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
    // TryEnableSingleBuffering() the first time we FinalizeFrame().  This is
    // a nop if already single buffered or if single buffering is unsupported.
    ResourceProvider()->TryEnableSingleBuffering();
  }
}

void HTMLCanvasElement::PostFinalizeFrame() {
  if (LowLatencyEnabled() && !dirty_rect_.IsEmpty() &&
      GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    const scoped_refptr<CanvasResource> canvas_resource =
        ResourceProvider()->ProduceCanvasResource();
    const FloatRect src_rect(0, 0, Size().Width(), Size().Height());
    dirty_rect_.Intersect(src_rect);
    const IntRect int_dirty = EnclosingIntRect(dirty_rect_);
    const SkIRect damage_rect = SkIRect::MakeXYWH(
        int_dirty.X(), int_dirty.Y(), int_dirty.Width(), int_dirty.Height());
    const bool needs_vertical_flip = !RenderingContext()->IsOriginTopLeft();
    frame_dispatcher_->DispatchFrame(std::move(canvas_resource), start_time,
                                     damage_rect, needs_vertical_flip,
                                     IsOpaque());
    dirty_rect_ = FloatRect();
  }

  // If the canvas is visible, notifying listeners is taken care of in
  // DoDeferredPaintInvalidation(), which allows the frame to be grabbed prior
  // to compositing, which is critically important because compositing may clear
  // the canvas's image. (e.g. WebGL context with preserveDrawingBuffer=false).
  // If the canvas is not visible, DoDeferredPaintInvalidation will not get
  // called, so we need to take care of business here.
  if (!did_notify_listeners_for_current_frame_)
    NotifyListenersCanvasChanged();
  did_notify_listeners_for_current_frame_ = false;
}

void HTMLCanvasElement::DisableAcceleration(
    std::unique_ptr<Canvas2DLayerBridge>
        unaccelerated_bridge_used_for_testing) {
  // Create and configure an unaccelerated Canvas2DLayerBridge.
  std::unique_ptr<Canvas2DLayerBridge> bridge;
  if (unaccelerated_bridge_used_for_testing)
    bridge = std::move(unaccelerated_bridge_used_for_testing);
  else
    bridge = Create2DLayerBridge(RasterMode::kCPU);

  if (bridge && canvas2d_bridge_)
    ReplaceExisting2dLayerBridge(std::move(bridge));

  // We must force a paint invalidation on the canvas even if it's
  // content did not change because it layer was destroyed.
  DidDraw();
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::SetNeedsCompositingUpdate() {
  Element::SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::DoDeferredPaintInvalidation() {
  DCHECK(!dirty_rect_.IsEmpty());
  if (LowLatencyEnabled()) {
    // Low latency canvas handles dirty propagation in FinalizeFrame();
    return;
  }
  LayoutBox* layout_box = GetLayoutBox();

  FloatRect content_rect;
  if (layout_box) {
    if (layout_box->IsLayoutReplaced()) {
      content_rect =
          FloatRect(ToLayoutReplaced(layout_box)->ReplacedContentRect());
    } else {
      content_rect = FloatRect(layout_box->PhysicalContentBoxRect());
    }
  }

  if (IsRenderingContext2D()) {
    FloatRect src_rect(0, 0, Size().Width(), Size().Height());
    dirty_rect_.Intersect(src_rect);

    FloatRect invalidation_rect;
    if (layout_box) {
      FloatRect mapped_dirty_rect =
          MapRect(dirty_rect_, src_rect, content_rect);
      if (context_->IsComposited()) {
        // Composited 2D canvases need the dirty rect to be expressed relative
        // to the content box, as opposed to the layout box.
        mapped_dirty_rect.MoveBy(-content_rect.Location());
      }
      invalidation_rect = mapped_dirty_rect;
    } else {
      invalidation_rect = dirty_rect_;
    }

    if (dirty_rect_.IsEmpty())
      return;

    if (canvas2d_bridge_)
      canvas2d_bridge_->DoPaintInvalidation(invalidation_rect);
  }

  if (context_ && HasImageBitmapContext() && context_->CcLayer())
    context_->CcLayer()->SetNeedsDisplay();

  NotifyListenersCanvasChanged();
  did_notify_listeners_for_current_frame_ = true;

  // Propagate the |dirty_rect_| accumulated so far to the compositor
  // before restarting with a blank dirty rect.
  // Canvas content updates do not need to be propagated as
  // paint invalidations if the canvas is composited separately, since
  // the canvas contents are sent separately through a texture layer.
  if (layout_box && (!context_ || !context_->IsComposited())) {
    // If the content box is larger than |src_rect|, the canvas's image is
    // being stretched, so we need to account for color bleeding caused by the
    // interpolation filter.
    FloatRect src_rect(0, 0, Size().Width(), Size().Height());
    if (content_rect.Width() > src_rect.Width() ||
        content_rect.Height() > src_rect.Height()) {
      dirty_rect_.Inflate(0.5);
    }

    dirty_rect_.Intersect(src_rect);
    PhysicalRect mapped_dirty_rect(
        EnclosingIntRect(MapRect(dirty_rect_, src_rect, content_rect)));
    layout_box->InvalidatePaintRectangle(mapped_dirty_rect);
  }
  dirty_rect_ = FloatRect();

  DCHECK(dirty_rect_.IsEmpty());
}

void HTMLCanvasElement::Reset() {
  if (ignore_reset_)
    return;

  dirty_rect_ = FloatRect();

  bool had_resource_provider = HasResourceProvider();

  unsigned w = 0;
  AtomicString value = FastGetAttribute(html_names::kWidthAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, w) ||
      w > 0x7fffffffu) {
    w = kDefaultCanvasWidth;
  }

  unsigned h = 0;
  value = FastGetAttribute(html_names::kHeightAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, h) ||
      h > 0x7fffffffu) {
    h = kDefaultCanvasHeight;
  }

  if (IsRenderingContext2D()) {
    context_->Reset();
    origin_clean_ = true;
  }

  IntSize old_size = Size();
  IntSize new_size(w, h);

  // If the size of an existing buffer matches, we can just clear it instead of
  // reallocating.  This optimization is only done for 2D canvases for now.
  if (had_resource_provider && old_size == new_size && IsRenderingContext2D()) {
    if (!canvas_is_clear_) {
      canvas_is_clear_ = true;
      if (canvas2d_bridge_)
        canvas2d_bridge_->ClearFrame();
      context_->ClearRect(0, 0, width(), height());
    }
    return;
  }

  SetSurfaceSize(new_size);

  if (Is3d() && old_size != Size())
    context_->Reshape(width(), height());

  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (layout_object->IsCanvas()) {
      if (old_size != Size()) {
        ToLayoutHTMLCanvas(layout_object)->CanvasSizeChanged();
        if (GetDocument().GetSettings()->GetAcceleratedCompositingEnabled())
          GetLayoutBox()->ContentChanged(kCanvasChanged);
      }
      if (had_resource_provider)
        layout_object->SetShouldDoFullPaintInvalidation();
    }
  }
}

bool HTMLCanvasElement::PaintsIntoCanvasBuffer() const {
  if (OffscreenCanvasFrame())
    return false;
  DCHECK(context_);
  if (!context_->IsComposited())
    return true;
  auto* settings = GetDocument().GetSettings();
  if (settings && settings->GetAcceleratedCompositingEnabled())
    return false;

  return true;
}

void HTMLCanvasElement::NotifyListenersCanvasChanged() {
  if (listeners_.size() == 0)
    return;

  if (!OriginClean()) {
    listeners_.clear();
    return;
  }

  bool listener_needs_new_frame_capture = false;
  for (const CanvasDrawListener* listener : listeners_) {
    if (listener->NeedsNewFrame())
      listener_needs_new_frame_capture = true;
  }

  if (listener_needs_new_frame_capture) {
    SourceImageStatus status;
    scoped_refptr<StaticBitmapImage> source_image =
        GetSourceImageForCanvasInternal(&status);
    if (status != kNormalSourceImageStatus)
      return;
    for (CanvasDrawListener* listener : listeners_) {
      if (listener->NeedsNewFrame()) {
        // Here we need to use the SharedGpuContext as some of the images may
        // have been originated with other contextProvider, but we internally
        // need a context_provider that has a RasterInterface available.
        listener->SendNewFrame(source_image,
                               SharedGpuContext::ContextProviderWrapper());
      }
    }
  }
}

// Returns an image and the image's resolution scale factor.
static std::pair<blink::Image*, float> BrokenCanvas(float device_scale_factor) {
  if (device_scale_factor >= 2) {
    DEFINE_STATIC_REF(blink::Image, broken_canvas_hi_res,
                      (blink::Image::LoadPlatformResource(
                          IDR_BROKENCANVAS, ui::SCALE_FACTOR_200P)));
    return std::make_pair(broken_canvas_hi_res, 2);
  }

  DEFINE_STATIC_REF(blink::Image, broken_canvas_lo_res,
                    (blink::Image::LoadPlatformResource(IDR_BROKENCANVAS)));
  return std::make_pair(broken_canvas_lo_res, 1);
}

static SkFilterQuality FilterQualityFromStyle(const ComputedStyle* style) {
  if (style && style->ImageRendering() == EImageRendering::kPixelated)
    return kNone_SkFilterQuality;
  return kLow_SkFilterQuality;
}

SkFilterQuality HTMLCanvasElement::FilterQuality() const {
  if (!isConnected())
    return kLow_SkFilterQuality;

  const ComputedStyle* style = GetComputedStyle();
  if (!style) {
    GetDocument().UpdateStyleAndLayoutTreeForNode(this);
    HTMLCanvasElement* non_const_this = const_cast<HTMLCanvasElement*>(this);
    style = non_const_this->EnsureComputedStyle();
  }
  return FilterQualityFromStyle(style);
}

bool HTMLCanvasElement::LowLatencyEnabled() const {
  return !!frame_dispatcher_;
}

void HTMLCanvasElement::UpdateFilterQuality(SkFilterQuality filter_quality) {
  if (IsOffscreenCanvasRegistered())
    UpdateOffscreenCanvasFilterQuality(filter_quality);

  if (context_ && Is3d())
    context_->SetFilterQuality(filter_quality);
  else if (canvas2d_bridge_)
    canvas2d_bridge_->SetFilterQuality(filter_quality);
}

// In some instances we don't actually want to paint to the parent layer
// We still might want to set filter quality and MarkFirstContentfulPaint though
void HTMLCanvasElement::Paint(GraphicsContext& context,
                              const PhysicalRect& r,
                              bool flatten_composited_layers) {
  if (context_creation_was_blocked_ ||
      (context_ && context_->isContextLost())) {
    float device_scale_factor =
        blink::DeviceScaleFactorDeprecated(GetDocument().GetFrame());
    std::pair<Image*, float> broken_canvas_and_image_scale_factor =
        BrokenCanvas(device_scale_factor);
    Image* broken_canvas = broken_canvas_and_image_scale_factor.first;
    context.Save();
    context.FillRect(FloatRect(r), Color(), SkBlendMode::kClear);
    // Place the icon near the upper left, like the missing image icon
    // for image elements. Offset it a bit from the upper corner.
    FloatSize icon_size(broken_canvas->Size());
    FloatPoint upper_left =
        FloatPoint(r.PixelSnappedOffset()) + icon_size.ScaledBy(0.5f);
    context.DrawImage(broken_canvas, Image::kSyncDecode,
                      FloatRect(upper_left, icon_size));
    context.Restore();
    return;
  }

  // FIXME: crbug.com/438240; there is a bug with the new CSS blending and
  // compositing feature.
  if (!context_ && !OffscreenCanvasFrame())
    return;

  // If the canvas is gpu composited, it has another way of getting to screen
  if (!PaintsIntoCanvasBuffer()) {
    // For click-and-drag or printing we still want to draw
    if (!(flatten_composited_layers || GetDocument().Printing()))
      return;
  }

  if (OffscreenCanvasFrame()) {
    DCHECK(GetDocument().Printing());
    scoped_refptr<StaticBitmapImage> image_for_printing =
        OffscreenCanvasFrame()->Bitmap()->MakeUnaccelerated();
    context.DrawImage(image_for_printing.get(), Image::kSyncDecode,
                      FloatRect(PixelSnappedIntRect(r)));
    return;
  }

  PaintInternal(context, r);
}

void HTMLCanvasElement::PaintInternal(GraphicsContext& context,
                                      const PhysicalRect& r) {
  context_->PaintRenderingResultsToCanvas(kFrontBuffer);
  if (HasResourceProvider()) {
    const ComputedStyle* style = GetComputedStyle();
    // For 2D Canvas, there are two ways of render Canvas for printing:
    // display list or image snapshot. Display list allows better PDF printing
    // and we prefer this method.
    // Here are the requirements for display list to be used:
    //    1. We must have had a full repaint of the Canvas after beginprint
    //       event has been fired. Otherwise, we don't have a PaintRecord.
    //    2. CSS property 'image-rendering' must not be 'pixelated'.

    // display list rendering: we replay the last full PaintRecord, if Canvas
    // has been redraw since beginprint happened.
    if (IsPrinting() && !Is3d() && canvas2d_bridge_) {
      canvas2d_bridge_->FlushRecording();
      if (canvas2d_bridge_->getLastRecord()) {
        if (style && style->ImageRendering() != EImageRendering::kPixelated) {
          context.Canvas()->save();
          context.Canvas()->translate(r.X(), r.Y());
          context.Canvas()->scale(r.Width() / Size().Width(),
                                  r.Height() / Size().Height());
          context.Canvas()->drawPicture(canvas2d_bridge_->getLastRecord());
          context.Canvas()->restore();
          UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", true);
          return;
        }
      }
      UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", false);
    }
    // or image snapshot rendering: grab a snapshot and raster it.
    SkBlendMode composite_operator =
        !context_ || context_->CreationAttributes().alpha
            ? SkBlendMode::kSrcOver
            : SkBlendMode::kSrc;
    FloatRect src_rect = FloatRect(FloatPoint(), FloatSize(Size()));
    scoped_refptr<StaticBitmapImage> snapshot =
        canvas2d_bridge_
            ? canvas2d_bridge_->NewImageSnapshot()
            : (ResourceProvider() ? ResourceProvider()->Snapshot() : nullptr);
    if (snapshot) {
      // GraphicsContext cannot handle gpu resource serialization.
      snapshot = snapshot->MakeUnaccelerated();
      DCHECK(!snapshot->IsTextureBacked());
      context.DrawImage(snapshot.get(), Image::kSyncDecode,
                        FloatRect(PixelSnappedIntRect(r)), &src_rect,
                        style && style->HasFilterInducingProperty(),
                        composite_operator);
    }
  } else {
    // When alpha is false, we should draw to opaque black.
    if (!context_->CreationAttributes().alpha)
      context.FillRect(FloatRect(r), Color(0, 0, 0));
  }

  if (Is3d() && PaintsIntoCanvasBuffer())
    context_->MarkLayerComposited();
}

bool HTMLCanvasElement::IsPrinting() const {
  return GetDocument().BeforePrintingOrPrinting();
}

void HTMLCanvasElement::SetSurfaceSize(const IntSize& size) {
  size_ = size;
  did_fail_to_create_resource_provider_ = false;
  DiscardResourceProvider();
  if (IsRenderingContext2D() && context_->isContextLost())
    context_->DidSetSurfaceSize();
  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(size_);
}

const AtomicString HTMLCanvasElement::ImageSourceURL() const {
  return AtomicString(ToDataURLInternal(
      ImageEncoderUtils::kDefaultRequestedMimeType, 0, kFrontBuffer));
}

scoped_refptr<StaticBitmapImage> HTMLCanvasElement::Snapshot(
    SourceDrawingBuffer source_buffer) const {
  if (size_.IsEmpty())
    return nullptr;

  scoped_refptr<StaticBitmapImage> image_bitmap = nullptr;
  if (OffscreenCanvasFrame()) {  // Offscreen Canvas
    DCHECK(OffscreenCanvasFrame()->OriginClean());
    image_bitmap = OffscreenCanvasFrame()->Bitmap();
  } else if (Is3d()) {  // WebGL or WebGL2 canvas
    if (context_->CreationAttributes().premultiplied_alpha) {
      context_->PaintRenderingResultsToCanvas(source_buffer);
      if (ResourceProvider())
        image_bitmap = ResourceProvider()->Snapshot();
    } else {
      sk_sp<SkData> pixel_data =
          context_->PaintRenderingResultsToDataArray(source_buffer);
      if (pixel_data) {
        // If the accelerated canvas is too big, there is a logic in WebGL code
        // path that scales down the drawing buffer to the maximum supported
        // size. Hence, we need to query the adjusted size of DrawingBuffer.
        IntSize adjusted_size = context_->DrawingBufferSize();
        SkImageInfo info =
            SkImageInfo::Make(adjusted_size.Width(), adjusted_size.Height(),
                              kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        info = info.makeColorSpace(ColorParams().GetSkColorSpace());
        if (ColorParams().GetSkColorType() != kN32_SkColorType)
          info = info.makeColorType(kRGBA_F16_SkColorType);
        image_bitmap = StaticBitmapImage::Create(std::move(pixel_data), info);
      }
    }
  } else if (canvas2d_bridge_) {
    DCHECK(IsRenderingContext2D());
    image_bitmap = canvas2d_bridge_->NewImageSnapshot();
  } else if (context_) {  // Bitmap renderer canvas
    image_bitmap = context_->GetImage();
  }

  if (!image_bitmap)
    image_bitmap = CreateTransparentImage(size_);
  return image_bitmap;
}

String HTMLCanvasElement::ToDataURLInternal(
    const String& mime_type,
    const double& quality,
    SourceDrawingBuffer source_buffer) const {
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!IsPaintable())
    return String("data:,");

  ImageEncodingMimeType encoding_mime_type =
      ImageEncoderUtils::ToEncodingMimeType(
          mime_type, ImageEncoderUtils::kEncodeReasonToDataURL);

  scoped_refptr<StaticBitmapImage> image_bitmap = Snapshot(source_buffer);
  if (image_bitmap) {
    std::unique_ptr<ImageDataBuffer> data_buffer =
        ImageDataBuffer::Create(image_bitmap);
    if (!data_buffer)
      return String("data:,");

    String data_url = data_buffer->ToDataURL(encoding_mime_type, quality);
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
    float sqrt_pixels =
        std::sqrt(image_bitmap->width()) * std::sqrt(image_bitmap->height());
    float scaled_time_float = elapsed_time.InMicrosecondsF() /
                              (sqrt_pixels == 0 ? 1.0f : sqrt_pixels);

    // If scaled_time_float overflows as integer, CheckedNumeric will store it
    // as invalid, then ValueOrDefault will return the maximum int.
    base::CheckedNumeric<int> checked_scaled_time = scaled_time_float;
    int scaled_time_int =
        checked_scaled_time.ValueOrDefault(std::numeric_limits<int>::max());

    if (encoding_mime_type == kMimeTypePng) {
      UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToDataURLScaledDuration.PNG",
                                  scaled_time_int);
    } else if (encoding_mime_type == kMimeTypeJpeg) {
      UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToDataURLScaledDuration.JPEG",
                                  scaled_time_int);
    } else if (encoding_mime_type == kMimeTypeWebp) {
      UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToDataURLScaledDuration.WEBP",
                                  scaled_time_int);
    } else {
      // Currently we only support three encoding types.
      NOTREACHED();
    }
    IdentifiabilityReportWithDigest(IdentifiabilityBenignStringToken(data_url));
    return data_url;
  }

  return String("data:,");
}

String HTMLCanvasElement::toDataURL(const String& mime_type,
                                    const ScriptValue& quality_argument,
                                    ExceptionState& exception_state) const {
  if (!OriginClean()) {
    exception_state.ThrowSecurityError("Tainted canvases may not be exported.");
    return String();
  }

  double quality = kUndefinedQualityValue;
  if (!quality_argument.IsEmpty()) {
    v8::Local<v8::Value> v8_value = quality_argument.V8Value();
    if (v8_value->IsNumber())
      quality = v8_value.As<v8::Number>()->Value();
  }
  return ToDataURLInternal(mime_type, quality, kBackBuffer);
}

void HTMLCanvasElement::toBlob(V8BlobCallback* callback,
                               const String& mime_type,
                               const ScriptValue& quality_argument,
                               ExceptionState& exception_state) {
  if (!OriginClean()) {
    exception_state.ThrowSecurityError("Tainted canvases may not be exported.");
    return;
  }

  if (!GetExecutionContext())
    return;

  if (!IsPaintable()) {
    // If the canvas element's bitmap has no pixels
    GetDocument()
        .GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&V8BlobCallback::InvokeAndReportException,
                             WrapPersistent(callback), nullptr, nullptr));
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  double quality = kUndefinedQualityValue;
  if (!quality_argument.IsEmpty()) {
    v8::Local<v8::Value> v8_value = quality_argument.V8Value();
    if (v8_value->IsNumber())
      quality = v8_value.As<v8::Number>()->Value();
  }

  ImageEncodingMimeType encoding_mime_type =
      ImageEncoderUtils::ToEncodingMimeType(
          mime_type, ImageEncoderUtils::kEncodeReasonToBlobCallback);

  CanvasAsyncBlobCreator* async_creator = nullptr;
  scoped_refptr<StaticBitmapImage> image_bitmap = Snapshot(kBackBuffer);
  if (image_bitmap) {
    auto* options = ImageEncodeOptions::Create();
    options->setType(ImageEncodingMimeTypeName(encoding_mime_type));
    async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options,
        CanvasAsyncBlobCreator::kHTMLCanvasToBlobCallback, callback, start_time,
        GetExecutionContext(),
        UkmParameters{GetDocument().UkmRecorder(), GetDocument().UkmSourceID()},
        IdentifiabilityStudySettings::Get()->IsTypeAllowed(
            IdentifiableSurface::Type::kCanvasReadback)
            ? IdentifiabilityInputDigest(context_)
            : 0);
  }

  if (async_creator) {
    async_creator->ScheduleAsyncBlobCreation(quality);
  } else {
    GetDocument()
        .GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&V8BlobCallback::InvokeAndReportException,
                             WrapPersistent(callback), nullptr, nullptr));
  }
}

void HTMLCanvasElement::AddListener(CanvasDrawListener* listener) {
  listeners_.insert(listener);
}

void HTMLCanvasElement::RemoveListener(CanvasDrawListener* listener) {
  listeners_.erase(listener);
}

bool HTMLCanvasElement::OriginClean() const {
  if (GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetDisableReadingFromCanvas()) {
    return false;
  }
  if (OffscreenCanvasFrame())
    return OffscreenCanvasFrame()->OriginClean();
  return origin_clean_;
}

bool HTMLCanvasElement::ShouldAccelerate2dContext() const {
  return ShouldAccelerate();
}

CanvasResourceDispatcher* HTMLCanvasElement::GetOrCreateResourceDispatcher() {
  // The HTMLCanvasElement override of this method never needs to 'create'
  // because the frame_dispatcher is only used in low latency mode, in which
  // case the dispatcher is created upfront.
  return frame_dispatcher_.get();
}

bool HTMLCanvasElement::PushFrame(scoped_refptr<CanvasResource> image,
                                  const SkIRect& damage_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool HTMLCanvasElement::ShouldAccelerate() const {
  if (context_ && !IsRenderingContext2D())
    return false;

  // The command line flag --disable-accelerated-2d-canvas toggles this option
  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled()) {
    return false;
  }

  // Webview crashes with accelerated small canvases TODO(crbug.com/1004304)
  if (!RuntimeEnabledFeatures::AcceleratedSmallCanvasesEnabled()) {
    base::CheckedNumeric<int> checked_canvas_pixel_count =
        Size().Width() * Size().Height();
    if (!checked_canvas_pixel_count.IsValid())
      return false;
    int canvas_pixel_count = checked_canvas_pixel_count.ValueOrDie();

    if (canvas_pixel_count < kMinimumAccelerated2dCanvasSize)
      return false;
  }

  // The following is necessary for handling the special case of canvases in
  // the dev tools overlay, which run in a process that supports accelerated
  // 2d canvas but in a special compositing context that does not.
  auto* settings = GetDocument().GetSettings();
  if (settings && !settings->GetAcceleratedCompositingEnabled())
    return false;

  // Avoid creating |contextProvider| until we're sure we want to try use it,
  // since it costs us GPU memory.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return false;

  return context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

std::unique_ptr<Canvas2DLayerBridge> HTMLCanvasElement::Create2DLayerBridge(
    RasterMode raster_mode) {
  auto surface =
      std::make_unique<Canvas2DLayerBridge>(Size(), raster_mode, ColorParams());
  if (!surface->IsValid())
    return nullptr;

  return surface;
}

void HTMLCanvasElement::SetCanvas2DLayerBridgeInternal(
    std::unique_ptr<Canvas2DLayerBridge> external_canvas2d_bridge) {
  DCHECK(IsRenderingContext2D() && !canvas2d_bridge_);
  did_fail_to_create_resource_provider_ = true;

  if (!IsValidImageSize(Size()))
    return;

  if (external_canvas2d_bridge) {
    if (external_canvas2d_bridge->IsValid())
      canvas2d_bridge_ = std::move(external_canvas2d_bridge);
  } else {
    // If the canvas meets the criteria to use accelerated-GPU rendering, and
    // the user signals that the canvas will not be read frequently through
    // getImageData, which is a slow operation with GPU, the canvas will try to
    // use accelerated-GPU rendering.
    // If any of the two conditions fails, or if the creation of accelerated
    // resource provider fails, the canvas will fallback to CPU rendering.
    UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.WillReadFrequently",
                          context_->CreationAttributes().will_read_frequently);

    if (ShouldAccelerate() &&
        !context_->CreationAttributes().will_read_frequently) {
      canvas2d_bridge_ = Create2DLayerBridge(RasterMode::kGPU);
    }
    if (!canvas2d_bridge_) {
      canvas2d_bridge_ = Create2DLayerBridge(RasterMode::kCPU);
    }
  }

  if (!canvas2d_bridge_)
    return;

  canvas2d_bridge_->SetCanvasResourceHost(this);
  bool is_being_displayed =
      GetLayoutObject() && GetComputedStyle() &&
      GetComputedStyle()->Visibility() == EVisibility::kVisible;
  canvas2d_bridge_->SetIsBeingDisplayed(is_being_displayed);

  did_fail_to_create_resource_provider_ = false;
  UpdateMemoryUsage();

  if (context_)
    SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::NotifyGpuContextLost() {
  if (IsRenderingContext2D())
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);
}

void HTMLCanvasElement::Trace(Visitor* visitor) const {
  visitor->Trace(listeners_);
  visitor->Trace(context_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  HTMLElement::Trace(visitor);
}

Canvas2DLayerBridge* HTMLCanvasElement::GetOrCreateCanvas2DLayerBridge() {
  DCHECK(IsRenderingContext2D());
  if (!canvas2d_bridge_ && !did_fail_to_create_resource_provider_) {
    SetCanvas2DLayerBridgeInternal(nullptr);
    if (did_fail_to_create_resource_provider_ && !Size().IsEmpty())
      context_->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
  }
  return canvas2d_bridge_.get();
}

void HTMLCanvasElement::SetResourceProviderForTesting(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    std::unique_ptr<Canvas2DLayerBridge> bridge,
    const IntSize& size) {
  DiscardResourceProvider();
  SetIntegralAttribute(html_names::kWidthAttr, size.Width());
  SetIntegralAttribute(html_names::kHeightAttr, size.Height());
  SetCanvas2DLayerBridgeInternal(std::move(bridge));
  ReplaceResourceProvider(std::move(resource_provider));
}

void HTMLCanvasElement::DiscardResourceProvider() {
  canvas2d_bridge_.reset();
  CanvasResourceHost::DiscardResourceProvider();
  dirty_rect_ = FloatRect();
}

void HTMLCanvasElement::PageVisibilityChanged() {
  bool hidden = !GetPage()->IsPageVisible();
  SetSuspendOffscreenCanvasAnimation(hidden);

  if (!context_)
    return;

  context_->SetIsInHiddenPage(hidden);
  if (hidden && Is3d())
    DiscardResourceProvider();
}

void HTMLCanvasElement::ContextDestroyed() {
  if (context_)
    context_->Stop();
}

bool HTMLCanvasElement::StyleChangeNeedsDidDraw(
    const ComputedStyle* old_style,
    const ComputedStyle& new_style) {
  // It will only need to redraw for a style change, if the new imageRendering
  // is different than the previous one, and only if one of the two ir
  // pixelated.
  return old_style &&
         old_style->ImageRendering() != new_style.ImageRendering() &&
         (old_style->ImageRendering() == EImageRendering::kPixelated ||
          new_style.ImageRendering() == EImageRendering::kPixelated);
}

void HTMLCanvasElement::StyleDidChange(const ComputedStyle* old_style,
                                       const ComputedStyle& new_style) {
  UpdateFilterQuality(FilterQualityFromStyle(&new_style));
  if (context_)
    context_->StyleDidChange(old_style, new_style);
  if (StyleChangeNeedsDidDraw(old_style, new_style))
    DidDraw();
}

void HTMLCanvasElement::LayoutObjectDestroyed() {
  // If the canvas has no layout object then it definitely isn't being
  // displayed any more.
  if (context_)
    context_->SetIsBeingDisplayed(false);
}

void HTMLCanvasElement::DidMoveToNewDocument(Document& old_document) {
  SetExecutionContext(GetExecutionContext());
  SetPage(GetDocument().GetPage());
  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLCanvasElement::WillDrawImageTo2DContext(CanvasImageSource* source) {
  if (SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() &&
      source->IsAccelerated() && GetOrCreateCanvas2DLayerBridge() &&
      !canvas2d_bridge_->IsAccelerated() && ShouldAccelerate()) {
    std::unique_ptr<Canvas2DLayerBridge> surface =
        Create2DLayerBridge(RasterMode::kGPU);
    if (surface) {
      ReplaceExisting2dLayerBridge(std::move(surface));
      SetNeedsCompositingUpdate();
    }
  }
}

scoped_refptr<Image> HTMLCanvasElement::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const FloatSize&) {
  return GetSourceImageForCanvasInternal(status);
}

scoped_refptr<StaticBitmapImage>
HTMLCanvasElement::GetSourceImageForCanvasInternal(SourceImageStatus* status) {
  if (!width() || !height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }

  if (!IsPaintable()) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  if (OffscreenCanvasFrame()) {
    *status = kNormalSourceImageStatus;
    return OffscreenCanvasFrame()->Bitmap();
  }

  if (!context_) {
    scoped_refptr<StaticBitmapImage> result = GetTransparentImage();
    *status = result ? kNormalSourceImageStatus : kInvalidSourceImageStatus;
    return result;
  }

  if (HasImageBitmapContext()) {
    *status = kNormalSourceImageStatus;
    scoped_refptr<StaticBitmapImage> result = context_->GetImage();
    if (!result)
      result = GetTransparentImage();
    *status = result ? kNormalSourceImageStatus : kInvalidSourceImageStatus;
    return result;
  }

  scoped_refptr<StaticBitmapImage> image;
  // TODO(ccameron): Canvas should produce sRGB images.
  // https://crbug.com/672299
  if (Is3d()) {
    // Because WebGL sources always require making a copy of the back buffer, we
    // use paintRenderingResultsToCanvas instead of getImage in order to keep a
    // cached copy of the backing in the canvas's resource provider.
    RenderingContext()->PaintRenderingResultsToCanvas(kBackBuffer);
    if (ResourceProvider())
      image = ResourceProvider()->Snapshot();
    else
      image = GetTransparentImage();
  } else {
    image = RenderingContext()->GetImage();
    if (!image)
      image = GetTransparentImage();
  }

  if (image)
    *status = kNormalSourceImageStatus;
  else
    *status = kInvalidSourceImageStatus;
  return image;
}

bool HTMLCanvasElement::WouldTaintOrigin() const {
  return !OriginClean();
}

FloatSize HTMLCanvasElement::ElementSize(
    const FloatSize&,
    const RespectImageOrientationEnum) const {
  if (context_ && HasImageBitmapContext()) {
    scoped_refptr<Image> image = context_->GetImage();
    if (image)
      return FloatSize(image->width(), image->height());
    return FloatSize(0, 0);
  }
  if (OffscreenCanvasFrame())
    return FloatSize(OffscreenCanvasFrame()->Size());
  return FloatSize(width(), height());
}

IntSize HTMLCanvasElement::BitmapSourceSize() const {
  return IntSize(width(), height());
}

ScriptPromise HTMLCanvasElement::CreateImageBitmap(
    ScriptState* script_state,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      exception_state);
}

void HTMLCanvasElement::SetOffscreenCanvasResource(
    scoped_refptr<CanvasResource> image,
    unsigned resource_id) {
  OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(std::move(image),
                                                         resource_id);
  SetSize(OffscreenCanvasFrame()->Size());
  NotifyListenersCanvasChanged();
}

bool HTMLCanvasElement::IsOpaque() const {
  return context_ && !context_->CreationAttributes().alpha;
}

bool HTMLCanvasElement::IsSupportedInteractiveCanvasFallback(
    const Element& element) {
  if (!element.IsDescendantOf(this))
    return false;

  // An element is a supported interactive canvas fallback element if it is one
  // of the following:
  // https://html.spec.whatwg.org/C/#supported-interactive-canvas-fallback-element

  // An a element that represents a hyperlink and that does not have any img
  // descendants.
  if (IsA<HTMLAnchorElement>(element))
    return !Traversal<HTMLImageElement>::FirstWithin(element);

  // A button element
  if (IsA<HTMLButtonElement>(element))
    return true;

  // An input element whose type attribute is in one of the Checkbox or Radio
  // Button states.  An input element that is a button but its type attribute is
  // not in the Image Button state.
  if (auto* input_element = DynamicTo<HTMLInputElement>(element)) {
    if (input_element->type() == input_type_names::kCheckbox ||
        input_element->type() == input_type_names::kRadio ||
        input_element->IsTextButton()) {
      return true;
    }
  }

  // A select element with a "multiple" attribute or with a display size greater
  // than 1.
  if (auto* select_element = DynamicTo<HTMLSelectElement>(element)) {
    if (select_element->IsMultiple() || select_element->size() > 1)
      return true;
  }

  // An option element that is in a list of options of a select element with a
  // "multiple" attribute or with a display size greater than 1.
  const auto* parent_select =
      IsA<HTMLOptionElement>(element)
          ? DynamicTo<HTMLSelectElement>(element.parentNode())
          : nullptr;

  if (parent_select &&
      (parent_select->IsMultiple() || parent_select->size() > 1))
    return true;

  // An element that would not be interactive content except for having the
  // tabindex attribute specified.
  if (element.FastHasAttribute(html_names::kTabindexAttr))
    return true;

  // A non-interactive table, caption, thead, tbody, tfoot, tr, td, or th
  // element.
  if (IsA<HTMLTableElement>(element) ||
      element.HasTagName(html_names::kCaptionTag) ||
      element.HasTagName(html_names::kTheadTag) ||
      element.HasTagName(html_names::kTbodyTag) ||
      element.HasTagName(html_names::kTfootTag) ||
      element.HasTagName(html_names::kTrTag) ||
      element.HasTagName(html_names::kTdTag) ||
      element.HasTagName(html_names::kThTag))
    return true;

  return false;
}

HitTestCanvasResult* HTMLCanvasElement::GetControlAndIdIfHitRegionExists(
    const PhysicalOffset& location) {
  if (IsRenderingContext2D())
    return context_->GetControlAndIdIfHitRegionExists(location);
  return MakeGarbageCollected<HitTestCanvasResult>(String(), nullptr);
}

String HTMLCanvasElement::GetIdFromControl(const Element* element) {
  if (context_)
    return context_->GetIdFromControl(element);
  return String();
}

void HTMLCanvasElement::CreateLayer() {
  DCHECK(!surface_layer_bridge_);
  LocalFrame* frame = GetDocument().GetFrame();
  // We do not design transferControlToOffscreen() for frame-less HTML canvas.
  if (frame) {
    surface_layer_bridge_ = std::make_unique<::blink::SurfaceLayerBridge>(
        frame->GetPage()->GetChromeClient().GetFrameSinkId(frame),
        ::blink::SurfaceLayerBridge::ContainsVideo::kNo, this,
        base::DoNothing());
    // Creates a placeholder layer first before Surface is created.
    surface_layer_bridge_->CreateSolidColorLayer();
    // This may cause the canvas to be composited.
    SetNeedsCompositingUpdate();
  }
}

void HTMLCanvasElement::OnWebLayerUpdated() {
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::RegisterContentsLayer(cc::Layer* layer) {
  OnContentsCcLayerChanged();
}

void HTMLCanvasElement::UnregisterContentsLayer(cc::Layer* layer) {
  OnContentsCcLayerChanged();
}

FontSelector* HTMLCanvasElement::GetFontSelector() {
  return GetDocument().GetStyleEngine().GetFontSelector();
}

void HTMLCanvasElement::UpdateMemoryUsage() {
  int non_gpu_buffer_count = 0;
  int gpu_buffer_count = 0;

  if (!IsRenderingContext2D() && !Is3d())
    return;
  if (ResourceProvider()) {
    non_gpu_buffer_count++;
    if (IsAccelerated()) {
      // The number of internal GPU buffers vary between one (stable
      // non-displayed state) and three (triple-buffered animations).
      // Adding 2 is a pessimistic but relevant estimate.
      // Note: These buffers might be allocated in GPU memory.
      gpu_buffer_count += 2;
    }
  }

  if (Is3d())
    non_gpu_buffer_count += context_->ExternallyAllocatedBufferCountPerPixel();

  const int bytes_per_pixel = ColorParams().BytesPerPixel();

  intptr_t gpu_memory_usage = 0;
  if (gpu_buffer_count) {
    // Switch from cpu mode to gpu mode
    base::CheckedNumeric<intptr_t> checked_usage =
        gpu_buffer_count * bytes_per_pixel;
    checked_usage *= width();
    checked_usage *= height();
    gpu_memory_usage =
        checked_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());
  }

  // Recomputation of externally memory usage computation is carried out
  // in all cases.
  base::CheckedNumeric<intptr_t> checked_usage =
      non_gpu_buffer_count * bytes_per_pixel;
  checked_usage *= width();
  checked_usage *= height();
  checked_usage += gpu_memory_usage;
  intptr_t externally_allocated_memory =
      checked_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());
  // Subtracting two intptr_t that are known to be positive will never
  // underflow.
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      externally_allocated_memory - externally_allocated_memory_);
  externally_allocated_memory_ = externally_allocated_memory;
}

void HTMLCanvasElement::ReplaceExisting2dLayerBridge(
    std::unique_ptr<Canvas2DLayerBridge> new_layer_bridge) {
  scoped_refptr<StaticBitmapImage> image;
  std::unique_ptr<Canvas2DLayerBridge> old_layer_bridge;
  if (canvas2d_bridge_) {
    image = canvas2d_bridge_->NewImageSnapshot();
    // image can be null if allocation failed in which case we should just
    // abort the surface switch to retain the old surface which is still
    // functional.
    if (!image)
      return;
    old_layer_bridge = std::move(canvas2d_bridge_);
    // Removing connection between old_layer_bridge and CanvasResourceHost;
    // otherwise, the CanvasResourceHost checks for the old one before
    // assigning the new canvas layer bridge.
    old_layer_bridge->SetCanvasResourceHost(nullptr);
  }
  ReplaceResourceProvider(nullptr);
  canvas2d_bridge_ = std::move(new_layer_bridge);
  canvas2d_bridge_->SetCanvasResourceHost(this);

  // If PaintCanvas cannot be get from the new layer bridge, revert the
  // replacement.
  cc::PaintCanvas* canvas = canvas2d_bridge_->GetPaintCanvas();
  if (!canvas) {
    if (old_layer_bridge) {
      canvas2d_bridge_ = std::move(old_layer_bridge);
      canvas2d_bridge_->SetCanvasResourceHost(this);
    }
    return;
  }

  // After validating paint canvas on new layer bridge, removes the clip from
  // the canvas. Since clips is automatically applied to paint canvas, the image
  // already contains clip and the image needs to be drawn before the clip stack
  // is re-applied, it needs to remove clip from canvas and restore it after the
  // image is drawn.
  canvas->restoreToCount(1);
  canvas->save();

  // TODO(jochin): Consider using ResourceProvider()->RestoreBackBuffer() here
  // to avoid all of this clip stack manipulation.
  if (image)
    canvas2d_bridge_->DrawFullImage(image->PaintImageForCurrentFrame());

  RestoreCanvasMatrixClipStack(canvas);
  canvas2d_bridge_->DidRestoreCanvasMatrixClipStack(canvas);

  UpdateMemoryUsage();
}

CanvasResourceProvider* HTMLCanvasElement::GetOrCreateCanvasResourceProvider(
    RasterModeHint hint) {
  if (IsRenderingContext2D())
    return GetOrCreateCanvas2DLayerBridge()->GetOrCreateResourceProvider();

  return CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider(hint);
}

bool HTMLCanvasElement::HasImageBitmapContext() const {
  if (!context_)
    return false;
  CanvasRenderingContext::ContextType type = context_->GetContextType();
  return (type == CanvasRenderingContext::kContextImageBitmap);
}

scoped_refptr<StaticBitmapImage> HTMLCanvasElement::GetTransparentImage() {
  if (!transparent_image_ || transparent_image_.get()->Size() != Size())
    transparent_image_ = CreateTransparentImage(Size());
  return transparent_image_;
}

cc::Layer* HTMLCanvasElement::ContentsCcLayer() const {
  if (surface_layer_bridge_)
    return surface_layer_bridge_->GetCcLayer();
  if (context_ && context_->IsComposited())
    return context_->CcLayer();
  return nullptr;
}

void HTMLCanvasElement::OnContentsCcLayerChanged() {
  // We need to repaint the layer because the foreign layer display item may
  // appear, disappear or change.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      GetLayoutObject() && GetLayoutObject()->HasLayer())
    GetLayoutBoxModelObject()->Layer()->SetNeedsRepaint();
}

RespectImageOrientationEnum HTMLCanvasElement::RespectImageOrientation() const {
  return LayoutObject::ShouldRespectImageOrientation(GetLayoutObject());
}

}  // namespace blink
