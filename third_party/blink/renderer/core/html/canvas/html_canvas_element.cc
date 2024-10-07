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

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_to_video_frame_copier.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// These two constants determine if a newly created canvas starts with
// acceleration disabled. Specifically:
// 1. More than `kDisableAccelerationThreshold` canvases have been created.
// 2. The percent of canvases with acceleration disabled is >=
//    `kDisableAccelerationPercent`.
constexpr unsigned kDisableAccelerationThreshold = 100;
constexpr unsigned kDisableAccelerationPercent = 95;

BASE_FEATURE(kOneCopyCanvasCapture,
             "OneCopyCanvasCapture",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for not requesting continuous begin frame for low latency canvas.
BASE_FEATURE(kLowLatencyCanvasNoBeginFrameKillSwitch,
             "LowLatencyCanvasNoBeginFrameKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// These values come from the WhatWG spec.
constexpr int kDefaultCanvasWidth = 300;
constexpr int kDefaultCanvasHeight = 150;

// A default value of quality argument for toDataURL and toBlob
// It is in an invalid range (outside 0.0 - 1.0) so that it will not be
// misinterpreted as a user-input value
constexpr int kUndefinedQualityValue = -1.0;
constexpr int kMinimumAccelerated2dCanvasSize = 128 * 129;

// A default size used for canvas memory allocation when canvas size is greater
// than 2^20.
constexpr uint32_t kMaximumCanvasSize = 2 << 20;

// Tracks whether canvases should start out with acceleration disabled.
class DisabledAccelerationCounterSupplement final
    : public GarbageCollected<DisabledAccelerationCounterSupplement>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  static DisabledAccelerationCounterSupplement& From(Document& d) {
    DisabledAccelerationCounterSupplement* supplement =
        Supplement<Document>::From<DisabledAccelerationCounterSupplement>(d);
    if (!supplement) {
      supplement =
          MakeGarbageCollected<DisabledAccelerationCounterSupplement>(d);
      ProvideTo(d, supplement);
    }
    return *supplement;
  }

  explicit DisabledAccelerationCounterSupplement(Document& d)
      : Supplement<Document>(d) {}

  // Called when acceleration has been disabled on a canvas.
  void IncrementDisabledCount() {
    ++acceleration_disabled_count_;
    UpdateAccelerationDisabled();
  }

  // Returns true if canvas acceleration should be disabled.
  bool ShouldDisableAcceleration() {
    UpdateAccelerationDisabled();
    return acceleration_disabled_;
  }

 private:
  void UpdateAccelerationDisabled() {
    if (acceleration_disabled_) {
      return;
    }
    if (acceleration_disabled_count_ < kDisableAccelerationThreshold) {
      return;
    }
    if (acceleration_disabled_count_ * 100 /
            GetSupplementable()->GetNumberOfCanvases() >=
        kDisableAccelerationPercent) {
      acceleration_disabled_ = true;
    }
  }

  // Number of canvases with acceleration disabled.
  unsigned acceleration_disabled_count_ = 0;
  bool acceleration_disabled_ = false;
};

// static
const char DisabledAccelerationCounterSupplement::kSupplementName[] =
    "DisabledAccelerationCounterSupplement";

}  // namespace

HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(html_names::kCanvasTag, document),
      ExecutionContextLifecycleObserver(GetExecutionContext()),
      PageVisibilityObserver(document.GetPage()),
      CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kCanvasHost,
          gfx::Size(kDefaultCanvasWidth, kDefaultCanvasHeight)),
      context_creation_was_blocked_(false),
      ignore_reset_(false),
      origin_clean_(true),
      surface_layer_bridge_(nullptr),
      externally_allocated_memory_(0) {
  UseCounter::Count(document, WebFeature::kHTMLCanvasElement);
  // Create DisabledAccelerationCounterSupplement now, as it may be needed at a
  // time when garbage collected objects can not be created.
  DisabledAccelerationCounterSupplement::From(GetDocument());
  GetDocument().IncrementNumberOfCanvases();
  auto* execution_context = GetExecutionContext();
  if (execution_context) {
    CanvasResourceTracker::For(execution_context->GetIsolate())
        ->Add(this, execution_context);
  }
  SetHasCustomStyleCallbacks();
}

HTMLCanvasElement::~HTMLCanvasElement() {
  if (externally_allocated_memory_ > 0) {
    external_memory_accounter_.Decrease(v8::Isolate::GetCurrent(),
                                        externally_allocated_memory_);
  }
}

void HTMLCanvasElement::Dispose() {
  disposing_ = true;
  // We need to record metrics before we dispose of anything
  if (context_)
    UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.HasRendered", bool(ResourceProvider()));

  // It's possible that the placeholder frame has been disposed but its ID still
  // exists. Make sure that it gets unregistered here
  UnregisterPlaceholderCanvas();

  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  frame_dispatcher_ = nullptr;
  DiscardResourceProvider();

  if (context_) {
    if (context_->Host())
      context_->DetachHost();
    context_ = nullptr;
  }

  if (canvas2d_bridge_) {
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

void HTMLCanvasElement::ColorSchemeMayHaveChanged() {
  if (context_) {
    context_->ColorSchemeMayHaveChanged();
  }
}

void HTMLCanvasElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kWidthAttr ||
      params.name == html_names::kHeightAttr) {
    Reset();
  }
  HTMLElement::ParseAttribute(params);
}

LayoutObject* HTMLCanvasElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (GetExecutionContext() &&
      GetExecutionContext()->CanExecuteScripts(kNotAboutToExecuteScript)) {
    // Allocation of a layout object indicates that the canvas doesn't
    // have display:none set, so is conceptually being displayed.
    bool is_displayed = GetLayoutObject() && style_is_visible_;
    SetIsDisplayed(is_displayed);
    return MakeGarbageCollected<LayoutHTMLCanvas>(this);
  }
  return HTMLElement::CreateLayoutObject(style);
}

Node::InsertionNotificationRequest HTMLCanvasElement::InsertedInto(
    ContainerNode& node) {
  SetIsInCanvasSubtree(true);
  ColorSchemeMayHaveChanged();
  return HTMLElement::InsertedInto(node);
}

bool HTMLCanvasElement::SizeChangesAreAllowed(ExceptionState& exception_state) {
  if (IsOffscreenCanvasRegistered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot resize canvas after call to transferControlToOffscreen().");
    return false;
  }
  return true;
}

void HTMLCanvasElement::setHeight(unsigned value,
                                  ExceptionState& exception_state) {
  if (SizeChangesAreAllowed(exception_state)) {
    SetUnsignedIntegralAttribute(html_names::kHeightAttr, value,
                                 kDefaultCanvasHeight);
  }
}

void HTMLCanvasElement::setWidth(unsigned value,
                                 ExceptionState& exception_state) {
  if (SizeChangesAreAllowed(exception_state)) {
    SetUnsignedIntegralAttribute(html_names::kWidthAttr, value,
                                 kDefaultCanvasWidth);
  }
}

void HTMLCanvasElement::SetSize(gfx::Size new_size) {
  if (new_size == Size())
    return;
  ignore_reset_ = true;
  SetIntegralAttribute(html_names::kWidthAttr, new_size.width());
  SetIntegralAttribute(html_names::kHeightAttr, new_size.height());
  ignore_reset_ = false;
  Reset();
}

HTMLCanvasElement::ContextFactoryVector&
HTMLCanvasElement::RenderingContextFactories() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(
      ContextFactoryVector, context_factories,
      (static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::kMaxValue) +
       1));
  return context_factories;
}

CanvasRenderingContextFactory* HTMLCanvasElement::GetRenderingContextFactory(
    int rendering_api) {
  DCHECK_LE(
      rendering_api,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::kMaxValue));
  return RenderingContextFactories()[rendering_api].get();
}

void HTMLCanvasElement::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::CanvasRenderingAPI rendering_api =
      rendering_context_factory->GetRenderingAPI();
  DCHECK_LE(rendering_api,
            CanvasRenderingContext::CanvasRenderingAPI::kMaxValue);
  DCHECK(!RenderingContextFactories()[static_cast<int>(rendering_api)]);
  RenderingContextFactories()[static_cast<int>(rendering_api)] =
      std::move(rendering_context_factory);
}

void HTMLCanvasElement::RecordIdentifiabilityMetric(
    IdentifiableSurface surface,
    IdentifiableToken value) const {
  blink::IdentifiabilityMetricBuilder(GetDocument().UkmSourceID())
      .Add(surface, value)
      .Record(GetDocument().UkmRecorder());
}

void HTMLCanvasElement::IdentifiabilityReportWithDigest(
    IdentifiableToken canvas_contents_token) const {
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
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

  Document& doc = GetDocument();
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kCanvasRenderingContext)) {
    IdentifiabilityMetricBuilder(doc.UkmSourceID())
        .Add(IdentifiableSurface::FromTypeAndToken(
                 IdentifiableSurface::Type::kCanvasRenderingContext,
                 CanvasRenderingContext::RenderingAPIFromId(type)),
             !!result)
        .Record(doc.UkmRecorder());
  }

  if (attributes.color_space != PredefinedColorSpace::kSRGB)
    UseCounter::Count(doc, WebFeature::kCanvasUseColorSpace);

  if (ContentsCcLayer() != old_contents_cc_layer)
    SetNeedsCompositingUpdate();

  return result;
}

bool HTMLCanvasElement::IsPageVisible() {
  return GetPage() && GetPage()->IsPageVisible();
}

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContextInternal(
    const String& type,
    const CanvasContextCreationAttributesCore& attributes) {
  CanvasRenderingContext::CanvasRenderingAPI rendering_api =
      CanvasRenderingContext::RenderingAPIFromId(type);

  // Unknown type.
  if (rendering_api == CanvasRenderingContext::CanvasRenderingAPI::kUnknown) {
    return nullptr;
  }

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(static_cast<int>(rendering_api));
  if (!factory)
    return nullptr;

  // FIXME - The code depends on the context not going away once created, to
  // prevent JS from seeing a dangling pointer. So for now we will disallow the
  // context from being changed once it is created.
  if (context_) {
    if (context_->GetRenderingAPI() == rendering_api)
      return context_.Get();

    factory->OnError(this,
                     "Canvas has an existing context of a different type");
    return nullptr;
  }

  // Tell the debugger about the attempt to create a canvas context
  // even if it will fail, to ease debugging.
  probe::DidCreateCanvasContext(&GetDocument());

  // If this context is cross-origin, it should prefer to use the low-power GPU
  LocalFrame* frame = GetDocument().GetFrame();
  CanvasContextCreationAttributesCore recomputed_attributes = attributes;
  if (frame && frame->IsCrossOriginToOutermostMainFrame()) {
    recomputed_attributes.power_preference =
        CanvasContextCreationAttributesCore::PowerPreference::kLowPower;
  }

  context_ = factory->Create(this, recomputed_attributes);
  if (!context_)
    return nullptr;

  if (IsWebGL() || IsWebGPU() || IsImageBitmapRenderingContext()) {
    context_->SetFilterQuality(FilterQuality());
  }
  context_->RecordUKMCanvasRenderingAPI();
  context_->RecordUMACanvasRenderingAPI();
  // Since the |context_| is created, free the transparent image,
  // |transparent_image_| created for this canvas if it exists.
  if (transparent_image_.get()) {
    transparent_image_.reset();
  }

  context_creation_was_blocked_ = false;

  if (IsWebGL())
    UpdateMemoryUsage();

  LayoutObject* layout_object = GetLayoutObject();
  if (layout_object) {
    if (IsRenderingContext2D() && !context_->CreationAttributes().alpha) {
      // In the alpha false case, canvas is initially opaque, so we need to
      // trigger an invalidation.
      DidDraw();
    }
  }

  if (context_->CreationAttributes().desynchronized) {
    if (!CreateLayer())
      return nullptr;
    SetNeedsUnbufferedInputEvents(true);
    frame_dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        nullptr, GetDocument().GetTaskRunner(TaskType::kInternalDefault),
        GetPage()
            ->GetPageScheduler()
            ->GetAgentGroupScheduler()
            .CompositorTaskRunner(),
        surface_layer_bridge_->GetFrameSinkId().client_id(),
        surface_layer_bridge_->GetFrameSinkId().sink_id(),
        CanvasResourceDispatcher::kInvalidPlaceholderCanvasId, Size());
    if (!base::FeatureList::IsEnabled(
            kLowLatencyCanvasNoBeginFrameKillSwitch)) {
      // We don't actually need the begin frame signal when in low latency mode,
      // but we need to subscribe to it or else dispatching frames will not
      // work.
      frame_dispatcher_->SetNeedsBeginFrame(IsPageVisible());
    }

    UseCounter::Count(GetDocument(), WebFeature::kHTMLCanvasElementLowLatency);
  }

  // A 2D context does not know before lazy creation whether or not it is
  // direct composited. The Canvas2DLayerBridge will handle this
  if (!IsRenderingContext2D())
    SetNeedsCompositingUpdate();

  SetOpacityMode(GetRenderingContextSkColorInfo().isOpaque() ? kOpaque
                                                             : kNonOpaque);

  return context_.Get();
}

void HTMLCanvasElement::configureHighDynamicRange(
    const CanvasHighDynamicRangeOptions* options,
    ExceptionState& exception_state) {
  gfx::HDRMetadata hdr_metadata;
  ParseCanvasHighDynamicRangeOptions(options, hdr_metadata);

  if (IsOffscreenCanvasRegistered()) {
    // TODO(https://crbug.com/1274220): Implement HDR support for offscreen
    // canvas.
    NOTIMPLEMENTED();
  }

  CanvasResourceHost::SetHdrMetadata(hdr_metadata);
  if (context_ && (IsWebGL() || IsWebGPU())) {
    context_->SetHdrMetadata(hdr_metadata);
  }
}

bool HTMLCanvasElement::ShouldBeDirectComposited() const {
  return (context_ && context_->IsComposited()) || (!!surface_layer_bridge_);
}

Settings* HTMLCanvasElement::GetSettings() const {
  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (window && window->GetFrame())
    return window->GetFrame()->GetSettings();
  return nullptr;
}

bool HTMLCanvasElement::IsWebGL1Enabled() const {
  Settings* settings = GetSettings();
  return settings && settings->GetWebGL1Enabled();
}

bool HTMLCanvasElement::IsWebGL2Enabled() const {
  Settings* settings = GetSettings();
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

void HTMLCanvasElement::SetContextCreationWasBlocked() {
  context_creation_was_blocked_ = true;
  // This canvas's cc::Layer (or whether it has one at all) has likely
  // changed, so schedule a compositing update.
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::DidDraw(const SkIRect& rect) {
  if (rect.isEmpty())
    return;

  // To avoid issuing invalidations multiple times, we can check |dirty_rect_|
  // and only issue invalidations the first time it becomes non-empty.
  if (dirty_rect_.IsEmpty()) {
    if (LayoutObject* layout_object = GetLayoutObject()) {
      if (layout_object->PreviousVisibilityVisible() &&
          GetDocument().GetPage()) {
        GetDocument().GetPage()->Animator().SetHasCanvasInvalidation();
      }
      if (!LowLatencyEnabled()) {
        layout_object->SetShouldCheckForPaintInvalidation();
      }
    }
  }

  canvas_is_clear_ = false;
  dirty_rect_.Union(gfx::Rect(gfx::SkIRectToRect(rect)));
}

void HTMLCanvasElement::PreFinalizeFrame() {
  RecordCanvasSizeToUMA();

  // Low-latency 2d canvases produce their frames after the resource gets single
  // buffered.
  if (LowLatencyEnabled() && !dirty_rect_.IsEmpty() &&
      GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
    // TryEnableSingleBuffering() the first time we FinalizeFrame().  This is
    // a nop if already single buffered or if single buffering is unsupported.
    ResourceProvider()->TryEnableSingleBuffering();
  }
}

void HTMLCanvasElement::PostFinalizeFrame(FlushReason reason) {
  if (LowLatencyEnabled() && !dirty_rect_.IsEmpty() &&
      GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    if (scoped_refptr<CanvasResource> canvas_resource =
            ResourceProvider()->ProduceCanvasResource(reason)) {
      const gfx::Rect src_rect(Size());
      dirty_rect_.Intersect(src_rect);
      const gfx::Rect int_dirty = dirty_rect_;
      const SkIRect damage_rect = SkIRect::MakeXYWH(
          int_dirty.x(), int_dirty.y(), int_dirty.width(), int_dirty.height());
      const bool needs_vertical_flip = !canvas_resource->IsOriginTopLeft();
      frame_dispatcher_->DispatchFrame(std::move(canvas_resource), start_time,
                                       damage_rect, needs_vertical_flip,
                                       IsOpaque());
    }
    dirty_rect_ = gfx::Rect();
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
    std::unique_ptr<CanvasResourceProvider> new_provider_for_testing) {
  DisabledAccelerationCounterSupplement::From(GetDocument())
      .IncrementDisabledCount();
  // Create and configure an unaccelerated Canvas2DLayerBridge.
  SetPreferred2DRasterMode(RasterModeHint::kPreferCPU);
  std::unique_ptr<Canvas2DLayerBridge> bridge = Create2DLayerBridge();

  if (bridge && canvas2d_bridge_) {
    ReplaceExisting2dLayerBridge(std::move(bridge),
                                 std::move(new_provider_for_testing));
  }

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

  gfx::RectF content_rect;
  if (layout_box) {
    if (auto* replaced = DynamicTo<LayoutReplaced>(layout_box))
      content_rect = gfx::RectF(replaced->ReplacedContentRect());
    else
      content_rect = gfx::RectF(layout_box->PhysicalContentBoxRect());
  }

  if (IsRenderingContext2D()) {
    gfx::Rect src_rect(Size());
    dirty_rect_.Intersect(src_rect);

    gfx::RectF invalidation_rect;
    if (layout_box) {
      gfx::RectF mapped_dirty_rect = gfx::MapRect(
          gfx::RectF(dirty_rect_), gfx::RectF(src_rect), content_rect);
      if (context_->IsComposited()) {
        // Composited 2D canvases need the dirty rect to be expressed relative
        // to the content box, as opposed to the layout box.
        mapped_dirty_rect.Offset(-content_rect.OffsetFromOrigin());
      }
      invalidation_rect = mapped_dirty_rect;
    } else {
      invalidation_rect = gfx::RectF(dirty_rect_);
    }

    if (dirty_rect_.IsEmpty())
      return;

    DoPaintInvalidation(gfx::ToEnclosingRect(invalidation_rect));
  }

  if (IsImageBitmapRenderingContext() && RenderingContext()->CcLayer()) {
    RenderingContext()->CcLayer()->SetNeedsDisplay();
  }

  NotifyListenersCanvasChanged();
  did_notify_listeners_for_current_frame_ = true;

  if (layout_box && !ShouldBeDirectComposited()) {
    // If the canvas is not composited, propagate the paint invalidation to
    // |layout_box| as the painted result will change.
    layout_box->SetShouldDoFullPaintInvalidation();
  }

  dirty_rect_ = gfx::Rect();
}

void HTMLCanvasElement::Reset() {
  if (ignore_reset_)
    return;

  dirty_rect_ = gfx::Rect();

  unsigned w = 0;
  AtomicString value = FastGetAttribute(html_names::kWidthAttr);
  if (value.empty() || !ParseHTMLNonNegativeInteger(value, w) ||
      w > 0x7fffffffu) {
    w = kDefaultCanvasWidth;
  }

  unsigned h = 0;
  value = FastGetAttribute(html_names::kHeightAttr);
  if (value.empty() || !ParseHTMLNonNegativeInteger(value, h) ||
      h > 0x7fffffffu) {
    h = kDefaultCanvasHeight;
  }

  if (IsRenderingContext2D()) {
    context_->Reset();
    origin_clean_ = true;
  }
  canvas_is_clear_ = true;

  gfx::Size old_size = Size();
  gfx::Size new_size(w, h);

  // If the size of an existing buffer matches, we can reuse that buffer.
  // This optimization is only done for 2D canvases for now.
  if (IsRenderingContext2D() && ResourceProvider() != nullptr &&
      old_size == new_size) {
    return;
  }

  SetSurfaceSize(new_size);

  if ((IsWebGL() && old_size != Size()) || IsWebGPU()) {
    context_->Reshape(width(), height());
  }

  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (layout_object->IsCanvas()) {
      if (old_size != Size())
        To<LayoutHTMLCanvas>(layout_object)->CanvasSizeChanged();
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

  if (!listener_needs_new_frame_capture)
    return;

  scoped_refptr<StaticBitmapImage> source_image;
  if (!copier_) {
    copier_ = std::make_unique<StaticBitmapImageToVideoFrameCopier>(
        WebGraphicsContext3DVideoFramePool::
            IsGpuMemoryBufferReadbackFromTextureEnabled());
  }

  const bool context_color_is_opaque =
      context_ ? context_->CanvasRenderingContextSkColorInfo().isOpaque()
               : false;

  for (CanvasDrawListener* listener : listeners_) {
    if (!listener->NeedsNewFrame())
      continue;

    // Split the listener's callback so that it can be used with both the one
    // copy path and fallback two copy path below.
    auto split_callback =
        base::SplitOnceCallback(listener->GetNewFrameCallback());
    const bool can_discard_alpha = listener->CanDiscardAlpha();

    // First attempt to copy directly from the rendering context to a video
    // frame. Not all rendering contexts need to support this (for contexts
    // where GetSourceImageForCanvasInternal is zero-copy, this is superfluous).
    if (context_ && (context_color_is_opaque || can_discard_alpha) &&
        base::FeatureList::IsEnabled(kOneCopyCanvasCapture)) {
      if (context_->CopyRenderingResultsToVideoFrame(
              copier_->GetAcceleratedVideoFramePool(
                  SharedGpuContext::ContextProviderWrapper()),
              kBackBuffer, gfx::ColorSpace::CreateREC709(),
              std::move(split_callback.first))) {
        TRACE_EVENT1("blink", "HTMLCanvasElement::NotifyListenersCanvasChanged",
                     "one_copy_canvas_capture", true);
        continue;
      }
    }

    // If that fails, then create a StaticBitmapImage for the contents of
    // the RenderingContext.
    TRACE_EVENT1("blink", "HTMLCanvasElement::NotifyListenersCanvasChanged",
                 "one_copy_canvas_capture", false);

    if (!source_image) {
      SourceImageStatus status;
      source_image =
          GetSourceImageForCanvasInternal(FlushReason::kDrawListener, &status);
      if (status != kNormalSourceImageStatus)
        continue;
    }

    // Here we need to use the SharedGpuContext as some of the images may
    // have been originated with other contextProvider, but we internally
    // need a context_provider that has a RasterInterface available.
    copier_->Convert(source_image, can_discard_alpha,
                     SharedGpuContext::ContextProviderWrapper(),
                     std::move(split_callback.second));
  }
}

// Returns an image and the image's resolution scale factor.
std::pair<blink::Image*, float> HTMLCanvasElement::BrokenCanvas(
    float device_scale_factor) {
  if (device_scale_factor >= 2) {
    DEFINE_STATIC_REF(blink::Image, broken_canvas_hi_res,
                      (blink::Image::LoadPlatformResource(IDR_BROKENCANVAS,
                                                          ui::k200Percent)));
    return std::make_pair(broken_canvas_hi_res, 2);
  }

  DEFINE_STATIC_REF(blink::Image, broken_canvas_lo_res,
                    (blink::Image::LoadPlatformResource(IDR_BROKENCANVAS)));
  return std::make_pair(broken_canvas_lo_res, 1);
}

bool HTMLCanvasElement::LowLatencyEnabled() const {
  return !!frame_dispatcher_;
}

void HTMLCanvasElement::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  CanvasResourceHost::SetFilterQuality(filter_quality);
  if (IsOffscreenCanvasRegistered())
    UpdateOffscreenCanvasFilterQuality(filter_quality);

  if (context_ &&
      (IsWebGL() || IsWebGPU() || IsImageBitmapRenderingContext())) {
    context_->SetFilterQuality(filter_quality);
  }
}

// In some instances we don't actually want to paint to the parent layer
// We still might want to set filter quality and MarkFirstContentfulPaint though
void HTMLCanvasElement::Paint(GraphicsContext& context,
                              const PhysicalRect& r,
                              bool flatten_composited_layers) {
  if (context_creation_was_blocked_ ||
      (context_ && context_->isContextLost())) {
    float dpr = GetDocument().DevicePixelRatio();
    std::pair<Image*, float> broken_canvas_and_image_scale_factor =
        BrokenCanvas(dpr);
    Image* broken_canvas = broken_canvas_and_image_scale_factor.first;
    context.Save();
    context.FillRect(
        gfx::RectF(r), Color::kWhite,
        PaintAutoDarkMode(ComputedStyleRef(),
                          DarkModeFilter::ElementRole::kBackground),
        SkBlendMode::kSrc);
    // Place the icon near the upper left, like the missing image icon
    // for image elements. Offset it a bit from the upper corner.
    gfx::SizeF icon_size(broken_canvas->Size());
    icon_size.Scale(0.5f);
    gfx::PointF upper_left =
        gfx::PointF(r.PixelSnappedOffset()) +
        gfx::Vector2dF(icon_size.width(), icon_size.height());
    // Make the icon more visually prominent on high-DPI displays.
    icon_size.Scale(dpr);
    context.DrawImage(*broken_canvas, Image::kSyncDecode,
                      ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                      gfx::RectF(upper_left, icon_size));
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
    if (!image_for_printing)
      return;
    context.DrawImage(*image_for_printing, Image::kSyncDecode,
                      ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                      gfx::RectF(ToPixelSnappedRect(r)));
    return;
  }

  PaintInternal(context, r);
}

void HTMLCanvasElement::PaintInternal(GraphicsContext& context,
                                      const PhysicalRect& r) {
  context_->PaintRenderingResultsToCanvas(kFrontBuffer);
  CanvasResourceProvider* provider = ResourceProvider();
  if (provider != nullptr) {
    // For 2D Canvas, there are two ways of render Canvas for printing:
    // display list or image snapshot. Display list allows better PDF printing
    // and we prefer this method.
    // Here are the requirements for display list to be used:
    //    1. We must have had a full repaint of the Canvas after beforeprint
    //       event has been fired. Otherwise, we don't have a PaintRecord.
    //    2. CSS property 'image-rendering' must not be 'pixelated'.

    // display list rendering: we replay the last full PaintRecord, if Canvas
    // has been redraw since beforeprint happened.

    // Note: Test coverage for this is assured by manual (non-automated)
    // web test printing/manual/canvas2d-vector-text.html
    // That test should be run manually against CLs that touch this code.
    if (IsPrinting() && IsRenderingContext2D() && canvas2d_bridge_) {
      FlushRecording(FlushReason::kPrinting);
      // `FlushRecording` might be a no-op if a flush already happened before.
      // Fortunately, the last flush recording was kept by the provider.
      const std::optional<cc::PaintRecord>& last_recording =
          provider->LastRecording();
      if (last_recording.has_value() &&
          FilterQuality() != cc::PaintFlags::FilterQuality::kNone) {
        context.Canvas()->save();
        context.Canvas()->translate(r.X(), r.Y());
        context.Canvas()->scale(r.Width() / Size().width(),
                                r.Height() / Size().height());
        context.Canvas()->drawPicture(*last_recording);
        context.Canvas()->restore();
        UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", true);
        return;
      }
      UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.VectorPrintFallbackReason",
                                provider->printing_fallback_reason());
      UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", false);
    }
    // or image snapshot rendering: grab a snapshot and raster it.
    SkBlendMode composite_operator =
        !context_ || context_->CreationAttributes().alpha
            ? SkBlendMode::kSrcOver
            : SkBlendMode::kSrc;
    gfx::RectF src_rect((gfx::SizeF(Size())));
    scoped_refptr<StaticBitmapImage> snapshot =
        // TODO(crbug.com/40280152): Port this check away from checking
        // `canvas2d_bridge_` directly as part of eliminating
        // Canvas2DLayerBridge.
        canvas2d_bridge_
            ? context_->GetImage(FlushReason::kPaint)
            : (ResourceProvider()
                   ? ResourceProvider()->Snapshot(FlushReason::kPaint)
                   : nullptr);
    if (snapshot) {
      // GraphicsContext cannot handle gpu resource serialization.
      snapshot = snapshot->MakeUnaccelerated();
      DCHECK(!snapshot->IsTextureBacked());
      context.DrawImage(*snapshot, Image::kSyncDecode,
                        ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                        gfx::RectF(ToPixelSnappedRect(r)), &src_rect,
                        composite_operator);
    }
  } else {
    // When alpha is false, we should draw to opaque black.
    if (!context_->CreationAttributes().alpha) {
      context.FillRect(
          gfx::RectF(r), Color(0, 0, 0),
          PaintAutoDarkMode(ComputedStyleRef(),
                            DarkModeFilter::ElementRole::kBackground));
    }
  }

  if (IsWebGL() && PaintsIntoCanvasBuffer())
    context_->MarkLayerComposited();
}

bool HTMLCanvasElement::IsPrinting() const {
  return GetDocument().BeforePrintingOrPrinting();
}

UkmParameters HTMLCanvasElement::GetUkmParameters() {
  return {GetDocument().UkmRecorder(), GetDocument().UkmSourceID()};
}

void HTMLCanvasElement::SetSurfaceSize(gfx::Size size) {
  CanvasResourceHost::SetSize(size);
  did_fail_to_create_resource_provider_ = false;
  DiscardResourceProvider();
  if (IsRenderingContext2D() && context_->isContextLost())
    context_->DidSetSurfaceSize();
  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(Size());
}

const AtomicString HTMLCanvasElement::ImageSourceURL() const {
  return AtomicString(ToDataURLInternal(
      ImageEncoderUtils::kDefaultRequestedMimeType, 0, kFrontBuffer));
}

scoped_refptr<StaticBitmapImage> HTMLCanvasElement::Snapshot(
    FlushReason reason,
    SourceDrawingBuffer source_buffer) const {
  if (Size().IsEmpty()) {
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> image_bitmap;
  if (OffscreenCanvasFrame()) {  // Offscreen Canvas
    DCHECK(OffscreenCanvasFrame()->OriginClean());
    image_bitmap = OffscreenCanvasFrame()->Bitmap();
  } else if (IsWebGL()) {
    if (context_->CreationAttributes().premultiplied_alpha) {
      context_->PaintRenderingResultsToCanvas(source_buffer);
      if (ResourceProvider())
        image_bitmap = ResourceProvider()->Snapshot(reason);
    } else {
      sk_sp<SkData> pixel_data =
          context_->PaintRenderingResultsToDataArray(source_buffer);
      if (pixel_data) {
        // If the accelerated canvas is too big, there is a logic in WebGL code
        // path that scales down the drawing buffer to the maximum supported
        // size. Hence, we need to query the adjusted size of DrawingBuffer.
        gfx::Size adjusted_size = context_->DrawingBufferSize();
        if (!adjusted_size.IsEmpty()) {
          SkColorInfo color_info =
              GetRenderingContextSkColorInfo().makeAlphaType(
                  kUnpremul_SkAlphaType);
          if (color_info.colorType() == kN32_SkColorType)
            color_info = color_info.makeColorType(kRGBA_8888_SkColorType);
          else
            color_info = color_info.makeColorType(kRGBA_F16_SkColorType);
          image_bitmap = StaticBitmapImage::Create(
              std::move(pixel_data),
              SkImageInfo::Make(
                  SkISize::Make(adjusted_size.width(), adjusted_size.height()),
                  color_info));
        }
      }
    }
  } else if (context_) {
    DCHECK(IsRenderingContext2D() || IsImageBitmapRenderingContext() ||
           IsWebGPU());
    image_bitmap = context_->GetImage(reason);
  }

  if (image_bitmap)
    DCHECK(image_bitmap->SupportsDisplayCompositing());
  else
    image_bitmap = CreateTransparentImage(Size());

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

  scoped_refptr<StaticBitmapImage> image_bitmap =
      Snapshot(FlushReason::kToDataURL, source_buffer);
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
      NOTREACHED_IN_MIGRATION();
    }
    IdentifiabilityReportWithDigest(IdentifiabilityBenignStringToken(data_url));
    return data_url;
  }

  return String("data:,");
}

String HTMLCanvasElement::toDataURL(const String& mime_type,
                                    const ScriptValue& quality_argument,
                                    ExceptionState& exception_state) const {
  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`toDataURL()` cannot be called with open layers.");
    return String();
  }

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
  String data = ToDataURLInternal(mime_type, quality, kBackBuffer);
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"),
      "CanvasReadback", "data_url", data.Utf8());
  return data;
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

  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`toBlob()` cannot be called with open layers.");
    return;
  }

  if (!IsPaintable()) {
    // If the canvas element's bitmap has no pixels
    GetDocument()
        .GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&V8BlobCallback::InvokeAndReportException,
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
  scoped_refptr<StaticBitmapImage> image_bitmap =
      Snapshot(FlushReason::kToBlob, kBackBuffer);
  if (image_bitmap) {
    auto* options = ImageEncodeOptions::Create();
    options->setType(ImageEncodingMimeTypeName(encoding_mime_type));
    async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options,
        CanvasAsyncBlobCreator::kHTMLCanvasToBlobCallback, callback, start_time,
        GetExecutionContext(),
        IdentifiabilityStudySettings::Get()->ShouldSampleType(
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
                   WTF::BindOnce(&V8BlobCallback::InvokeAndReportException,
                                 WrapPersistent(callback), nullptr, nullptr));
  }
}

bool HTMLCanvasElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLCanvasElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    const AtomicString& height = FastGetAttribute(html_names::kHeightAttr);
    if (!height.IsNull())
      ApplyIntegerAspectRatioToStyle(value, height, style);
  } else if (name == html_names::kHeightAttr) {
    const AtomicString& width = FastGetAttribute(html_names::kWidthAttr);
    if (!width.IsNull())
      ApplyIntegerAspectRatioToStyle(width, value, style);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLCanvasElement::AddListener(CanvasDrawListener* listener) {
  // The presence of a listener forces OffscrenCanvas animations to be active
  listeners_.insert(listener);
  UpdateSuspendOffscreenCanvasAnimation();
}

void HTMLCanvasElement::RemoveListener(CanvasDrawListener* listener) {
  listeners_.erase(listener);
  UpdateSuspendOffscreenCanvasAnimation();
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

bool HTMLCanvasElement::PushFrame(scoped_refptr<CanvasResource>&& image,
                                  const SkIRect& damage_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool HTMLCanvasElement::ShouldAccelerate() const {
  if (context_ && !IsRenderingContext2D())
    return false;

  // The command line flag --disable-accelerated-2d-canvas toggles this option
  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled())
    return false;

  // Webview crashes with accelerated small canvases (crbug.com/1004304)
  // Experimenting to see if this still causes crashes (crbug.com/1136603)
  if (!RuntimeEnabledFeatures::AcceleratedSmallCanvasesEnabled() &&
      !base::FeatureList::IsEnabled(
          features::kWebviewAccelerateSmallCanvases)) {
    base::CheckedNumeric<int> checked_canvas_pixel_count =
        Size().GetCheckedArea();
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

  if (context_ &&
      context_->CreationAttributes().will_read_frequently ==
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined &&
      DisabledAccelerationCounterSupplement::From(GetDocument())
          .ShouldDisableAcceleration()) {
    return false;
  }

  return context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

bool HTMLCanvasElement::ShouldDisableAccelerationBecauseOfReadback() const {
  return DisabledAccelerationCounterSupplement::From(GetDocument())
      .ShouldDisableAcceleration();
}

std::unique_ptr<Canvas2DLayerBridge> HTMLCanvasElement::Create2DLayerBridge() {
  return std::make_unique<Canvas2DLayerBridge>(this);
}

void HTMLCanvasElement::SetCanvas2DLayerBridgeInternal(
    std::unique_ptr<Canvas2DLayerBridge> external_canvas2d_bridge) {
  DCHECK(IsRenderingContext2D() && !canvas2d_bridge_);
  did_fail_to_create_resource_provider_ = true;

  if (!IsValidImageSize(Size()))
    return;

  if (external_canvas2d_bridge) {
    canvas2d_bridge_ = std::move(external_canvas2d_bridge);
  } else {
    // If the canvas meets the criteria to use accelerated-GPU rendering, and
    // the user signals that the canvas will not be read frequently through
    // getImageData, which is a slow operation with GPU, the canvas will try to
    // use accelerated-GPU rendering.
    // If any of the two conditions fails, or if the creation of accelerated
    // resource provider fails, the canvas will fallback to CPU rendering.
    UMA_HISTOGRAM_BOOLEAN(
        "Blink.Canvas.2DLayerBridge.WillReadFrequently",
        context_ &&
            context_->CreationAttributes().will_read_frequently ==
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);

    bool will_read_frequently =
        context_->CreationAttributes().will_read_frequently ==
        CanvasContextCreationAttributesCore::WillReadFrequently::kTrue;
    RasterModeHint hint =
        ShouldAccelerate() && context_ && !will_read_frequently
            ? RasterModeHint::kPreferGPU
            : RasterModeHint::kPreferCPU;
    SetPreferred2DRasterMode(hint);
    canvas2d_bridge_ = Create2DLayerBridge();
  }

  if (!canvas2d_bridge_)
    return;

  did_fail_to_create_resource_provider_ = false;
  UpdateMemoryUsage();

  if (context_)
    SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::NotifyGpuContextLost() {
  if (IsRenderingContext2D())
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);

  // TODO(juonv): Do we need to do anything about frame_dispatcher_ here?
  // Desynchronized canvases seem to continue to work after recovering from a
  // GPU context loss, so maybe the status quo is fine.
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
    if (did_fail_to_create_resource_provider_ && !Size().IsEmpty() && context_)
      context_->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
  }
  return canvas2d_bridge_.get();
}

void HTMLCanvasElement::SetResourceProviderForTesting(
    std::unique_ptr<CanvasResourceProvider> provider,
    std::unique_ptr<Canvas2DLayerBridge> bridge,
    const gfx::Size& size) {
  DiscardResourceProvider();
  SetIntegralAttribute(html_names::kWidthAttr, size.width());
  SetIntegralAttribute(html_names::kHeightAttr, size.height());
  CanvasResourceHost::SetSize(size);
  SetCanvas2DLayerBridgeInternal(std::move(bridge));
  ReplaceResourceProvider(std::move(provider));
}

void HTMLCanvasElement::DiscardResourceProvider() {
  canvas2d_bridge_.reset();
  ResetLayer();
  CanvasResourceHost::DiscardResourceProvider();
  dirty_rect_ = gfx::Rect();
}

void HTMLCanvasElement::UpdateSuspendOffscreenCanvasAnimation() {
  if (GetPage()) {
    SetSuspendOffscreenCanvasAnimation(
        GetPage()->GetVisibilityState() ==
            mojom::blink::PageVisibilityState::kHidden &&
        !HasCanvasCapture());
  }
}

void HTMLCanvasElement::PageVisibilityChanged() {
  // If we are still painting, then continue to allow animations, even if the
  // page is otherwise hidden.
  CanvasRenderingContextHost::PageVisibilityChanged();
  UpdateSuspendOffscreenCanvasAnimation();
}

void HTMLCanvasElement::ContextDestroyed() {
  if (context_)
    context_->Stop();
}

bool HTMLCanvasElement::StyleChangeNeedsDidDraw(
    const ComputedStyle* old_style,
    const ComputedStyle& new_style) {
  // It will only need to redraw for a style change, if the new imageRendering
  // is different than the previous one, and only if one of the two are
  // pixelated.
  return old_style &&
         old_style->ImageRendering() != new_style.ImageRendering() &&
         (old_style->ImageRendering() == EImageRendering::kPixelated ||
          new_style.ImageRendering() == EImageRendering::kPixelated);
}

void HTMLCanvasElement::StyleDidChange(const ComputedStyle* old_style,
                                       const ComputedStyle& new_style) {
  cc::PaintFlags::FilterQuality filter_quality =
      cc::PaintFlags::FilterQuality::kLow;
  if (new_style.ImageRendering() == EImageRendering::kPixelated)
    filter_quality = cc::PaintFlags::FilterQuality::kNone;
  SetFilterQuality(filter_quality);
  style_is_visible_ = new_style.UsedVisibility() == EVisibility::kVisible;
  bool is_displayed = GetLayoutObject() && style_is_visible_;
  SetIsDisplayed(is_displayed);
  if (context_) {
    context_->StyleDidChange(old_style, new_style);
  }
  if (StyleChangeNeedsDidDraw(old_style, new_style))
    DidDraw();
}

void HTMLCanvasElement::LayoutObjectDestroyed() {
  // If the canvas has no layout object then it definitely isn't being
  // displayed any more.
  SetIsDisplayed(false);
}

void HTMLCanvasElement::DidMoveToNewDocument(Document& old_document) {
  SetExecutionContext(GetExecutionContext());
  SetPage(GetDocument().GetPage());
  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLCanvasElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLElement::DidRecalcStyle(change);
  ColorSchemeMayHaveChanged();
}

void HTMLCanvasElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  ColorSchemeMayHaveChanged();
}

void HTMLCanvasElement::WillDrawImageTo2DContext(CanvasImageSource* source) {
  // If the source is GPU-accelerated, and the canvas is not, but could be...
  if (source->IsAccelerated() && ShouldAccelerate() &&
      GetRasterMode() == RasterMode::kCPU) {
    // Recreate the canvas in GPU raster mode, and update its contents.
    if (RecreateCanvasInGPURasterMode()) {
      SetNeedsCompositingUpdate();
    }
  }
}

bool HTMLCanvasElement::EnableAcceleration() {
  return GetRasterMode() == RasterMode::kCPU ? RecreateCanvasInGPURasterMode()
                                             : true;
}

bool HTMLCanvasElement::RecreateCanvasInGPURasterMode() {
  if (!SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade()) {
    return false;
  }
  SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  std::unique_ptr<Canvas2DLayerBridge> surface = Create2DLayerBridge();
  if (!surface) {
    return false;
  }
  ReplaceExisting2dLayerBridge(std::move(surface));
  return true;
}

scoped_refptr<Image> HTMLCanvasElement::GetSourceImageForCanvas(
    FlushReason reason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition) {
  return GetSourceImageForCanvasInternal(reason, status, alpha_disposition);
}

scoped_refptr<StaticBitmapImage>
HTMLCanvasElement::GetSourceImageForCanvasInternal(
    FlushReason reason,
    SourceImageStatus* status,
    const AlphaDisposition alpha_disposition) {
  if (ContextHasOpenLayers(context_)) {
    *status = kLayersOpenInCanvasSource;
    return nullptr;
  }

  if (!width() || !height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }

  if (!IsPaintable()) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> image;

  if (OffscreenCanvasFrame()) {
    // This may be false to set status to normal if a valid image can be got
    // even if this HTMLCanvasElement has been transferred
    // control to an offscreenCanvas. As offscreencanvas with the
    // TransferControlToOffscreen is asynchronous, this will need to finish the
    // first Frame in order to have a first OffscreenCanvasFrame.
    image = OffscreenCanvasFrame()->Bitmap();
  } else {
    if (IsWebGL() || IsWebGPU()) {
      // TODO(https://crbug.com/672299): Canvas should produce sRGB images.
      // Because WebGL/WebGPU sources always require copying the back buffer,
      // we use PaintRenderingResultsToCanvas instead of GetImage in order to
      // keep a cached copy of the backing in the canvas's resource provider.
      RenderingContext()->PaintRenderingResultsToCanvas(kBackBuffer);
      // TODO(sunnyps): Check what PaintRenderingResultsToCanvas returns. It
      // seems the above returns false unexpectedly in some tests.
      if (ResourceProvider()) {
        image = ResourceProvider()->Snapshot(reason);
      }
    } else if (RenderingContext()) {
      // This is either CanvasRenderingContext2D or ImageBitmapRenderingContext.
      image = RenderingContext()->GetImage(reason);
    }
    if (!image) {
      image = GetTransparentImage();
    }
  }

  if (!image) {
    // All other possible error statuses were checked earlier.
    *status = kInvalidSourceImageStatus;
    return image;
  }

  *status = kNormalSourceImageStatus;

  // If the alpha_disposition is already correct, or the image is opaque, this
  // is a no-op.
  return StaticBitmapImageTransform::GetWithAlphaDisposition(
      reason, std::move(image), alpha_disposition);
}

bool HTMLCanvasElement::WouldTaintOrigin() const {
  return !OriginClean();
}

gfx::SizeF HTMLCanvasElement::ElementSize(
    const gfx::SizeF&,
    const RespectImageOrientationEnum) const {
  if (IsImageBitmapRenderingContext()) {
    scoped_refptr<Image> image =
        RenderingContext()->GetImage(FlushReason::kNone);
    if (image) {
      return gfx::SizeF(image->width(), image->height());
    }
    return gfx::SizeF(0, 0);
  }
  if (OffscreenCanvasFrame()) {
    return gfx::SizeF(OffscreenCanvasFrame()->Size());
  }
  return gfx::SizeF(width(), height());
}

gfx::Size HTMLCanvasElement::BitmapSourceSize() const {
  return Size();
}

ScriptPromise<ImageBitmap> HTMLCanvasElement::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`createImageBitmap()` cannot be called with open layers.");
    return EmptyPromise();
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      options, exception_state);
}

void HTMLCanvasElement::SetOffscreenCanvasResource(
    scoped_refptr<CanvasResource>&& image,
    viz::ResourceId resource_id) {
  OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(std::move(image),
                                                         resource_id);
  SetSize(OffscreenCanvasFrame()->Size());
  NotifyListenersCanvasChanged();
}

bool HTMLCanvasElement::IsOpaque() const {
  return context_ && !context_->CreationAttributes().alpha;
}

bool HTMLCanvasElement::CreateLayer() {
  DCHECK(!surface_layer_bridge_);
  LocalFrame* frame = GetDocument().GetFrame();
  // We do not design transferControlToOffscreen() for frame-less HTML canvas.
  if (!frame || !frame->GetPage()) {
    return false;
  }

  surface_layer_bridge_ = std::make_unique<::blink::SurfaceLayerBridge>(
      frame->GetPage()->GetChromeClient().GetFrameSinkId(frame),
      ::blink::SurfaceLayerBridge::ContainsVideo::kNo, this,
      base::NullCallback());
  // Creates a placeholder layer first before Surface is created.
  surface_layer_bridge_->CreateSolidColorLayer();
  // This may cause the canvas to be composited.
  SetNeedsCompositingUpdate();

  return true;
}

void HTMLCanvasElement::OnWebLayerUpdated() {
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::RegisterContentsLayer(cc::Layer* layer) {
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::UnregisterContentsLayer(cc::Layer* layer) {
  SetNeedsCompositingUpdate();
}

FontSelector* HTMLCanvasElement::GetFontSelector() {
  return GetDocument().GetStyleEngine().GetFontSelector();
}

void HTMLCanvasElement::UpdateMemoryUsage() {
  int non_gpu_buffer_count = 0;
  int gpu_buffer_count = 0;

  if (!IsRenderingContext2D() && !IsWebGL())
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

  if (IsWebGL())
    non_gpu_buffer_count += context_->ExternallyAllocatedBufferCountPerPixel();

  const int bytes_per_pixel = GetRenderingContextSkColorInfo().bytesPerPixel();

  intptr_t gpu_memory_usage = 0;
  uint32_t canvas_width = std::min(kMaximumCanvasSize, width());
  uint32_t canvas_height = std::min(kMaximumCanvasSize, height());

  if (gpu_buffer_count) {
    // Switch from cpu mode to gpu mode
    base::CheckedNumeric<intptr_t> checked_usage =
        gpu_buffer_count * bytes_per_pixel;
    checked_usage *= canvas_width;
    checked_usage *= canvas_height;
    gpu_memory_usage =
        checked_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());
  }

  // Recomputation of externally memory usage computation is carried out
  // in all cases.
  base::CheckedNumeric<intptr_t> checked_usage =
      non_gpu_buffer_count * bytes_per_pixel;
  checked_usage *= canvas_width;
  checked_usage *= canvas_height;
  checked_usage += gpu_memory_usage;
  intptr_t externally_allocated_memory =
      checked_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());
  // Subtracting two intptr_t that are known to be positive will never
  // underflow.
  intptr_t delta_bytes =
      externally_allocated_memory - externally_allocated_memory_;

  // TODO(junov): We assume that it is impossible to be inside a FastAPICall
  // from a host interface other than the rendering context.  This assumption
  // may need to be revisited in the future depending on how the usage of
  // [NoAllocDirectCall] evolves.
  if (delta_bytes) {
    // Here we check "IsAllocationAllowed", but it is actually garbage
    // collection that is not allowed, and allocations can trigger GC.
    // AdjustAmountOfExternalAllocatedMemory is not an allocation but it
    // can trigger GC, So we use "IsAllocationAllowed" as a proxy for
    // "is GC allowed". When garbage collection is already in progress,
    // allocations are not allowed, but calling
    // AdjustAmountOfExternalAllocatedMemory is safe, hence the
    // 'diposing_' condition in the DCHECK below.
    DCHECK(ThreadState::Current()->IsAllocationAllowed() || disposing_);
    external_memory_accounter_.Update(v8::Isolate::GetCurrent(), delta_bytes);
    externally_allocated_memory_ = externally_allocated_memory;
  }
}

size_t HTMLCanvasElement::GetMemoryUsage() const {
  return base::saturated_cast<size_t>(externally_allocated_memory_);
}

void HTMLCanvasElement::ReplaceExisting2dLayerBridge(
    std::unique_ptr<Canvas2DLayerBridge> new_layer_bridge,
    std::unique_ptr<CanvasResourceProvider> new_provider_for_testing) {
  CanvasResourceProvider* old_provider = ResourceProvider();
  if (old_provider == nullptr) {
    return;
  }

  scoped_refptr<StaticBitmapImage> image;
  std::unique_ptr<Canvas2DLayerBridge> old_layer_bridge;
  // TODO(crbug.com/40280152): Port this check away from checking
  // `canvas2d_bridge_` directly as part of eliminating
  // Canvas2DLayerBridge.
  if (canvas2d_bridge_) {
    image = context_->GetImage(FlushReason::kReplaceLayerBridge);
    // image can be null if allocation failed in which case we should just
    // abort the surface switch to retain the old surface which is still
    // functional.
    if (!image)
      return;
    old_layer_bridge = std::move(canvas2d_bridge_);
  }
  std::unique_ptr<MemoryManagedPaintRecorder> recorder =
      old_provider->ReleaseRecorder();
  ResetLayer();
  ReplaceResourceProvider(nullptr);
  canvas2d_bridge_ = std::move(new_layer_bridge);

  if (new_provider_for_testing) {
    ReplaceResourceProvider(std::move(new_provider_for_testing));
  }

  // If PaintCanvas cannot be get from the new layer bridge, revert the
  // replacement.
  CanvasResourceProvider* new_provider =
      canvas2d_bridge_->GetOrCreateResourceProvider();
  if (!new_provider) {
    if (old_layer_bridge) {
      canvas2d_bridge_ = std::move(old_layer_bridge);
    }
    return;
  }

  if (image) {
    auto paint_image = image->PaintImageForCurrentFrame();
    if ((GetRasterMode() == RasterMode::kCPU) &&
        paint_image.IsTextureBacked()) {
      // If new bridge is unaccelerated we must read back |paint_image| here.
      // DrawFullImage will record the image and potentially raster on a worker
      // thread, but texture backed PaintImages can't be used on a different
      // thread.
      auto sk_image = paint_image.GetSwSkImage();
      auto content_id = paint_image.GetContentIdForFrame(0);
      auto builder =
          cc::PaintImageBuilder::WithProperties(std::move(paint_image))
              .set_image(sk_image, content_id);
      paint_image = builder.TakePaintImage();
    }
    new_provider->RestoreBackBuffer(paint_image);
  }

  new_provider->SetRecorder(std::move(recorder));

  UpdateMemoryUsage();
}

CanvasResourceProvider* HTMLCanvasElement::GetOrCreateCanvasResourceProvider(
    RasterModeHint hint) {
  if (IsRenderingContext2D()) {
    Canvas2DLayerBridge* bridge = GetOrCreateCanvas2DLayerBridge();
    if (bridge == nullptr) {
      return nullptr;
    }
    return bridge->GetOrCreateResourceProvider();
  }

  return CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider(hint);
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

RespectImageOrientationEnum HTMLCanvasElement::RespectImageOrientation() const {
  // TODO(junov): Computing style here will be problematic for applying the
  // NoAllocDirectCall IDL attribute to drawImage.
  if (!GetComputedStyle()) {
    GetDocument().UpdateStyleAndLayoutTreeForElement(
        this, DocumentUpdateReason::kCanvas);
    const_cast<HTMLCanvasElement*>(this)->EnsureComputedStyle();
  }
  return LayoutObject::GetImageOrientation(GetLayoutObject());
}

// Temporary plumbing
bool HTMLCanvasElement::IsHibernating() const {
  return canvas2d_bridge_ &&
         canvas2d_bridge_->GetHibernationHandler().IsHibernating();
}

bool HTMLCanvasElement::IsAccelerated() const {
  return GetRasterMode() == RasterMode::kGPU;
}

void HTMLCanvasElement::SetHasPlacedElements() {
  // If this is the first time placeElement() is called, its possible that the
  // canvas contains fallback content that has been ignored and needs to be
  // laid out.
  if (!has_placed_elements_) {
    has_placed_elements_ = true;
    SetForceReattachLayoutTree();
  }
}

}  // namespace blink
