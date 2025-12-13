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
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_draw_listener.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
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
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/transform_utils.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_to_video_frame_copier.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr unsigned kMaxCanvasAnimationBacklog = 2;

// These two constants determine if a newly created canvas starts with
// acceleration disabled. Specifically:
// 1. More than `kDisableAccelerationThreshold` canvases have been created.
// 2. The percent of canvases with acceleration disabled is >=
//    `kDisableAccelerationPercent`.
constexpr unsigned kDisableAccelerationThreshold = 100;
constexpr unsigned kDisableAccelerationPercent = 95;

BASE_FEATURE(kOneCopyCanvasCapture,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for not requesting continuous begin frame for low latency canvas.
BASE_FEATURE(kLowLatencyCanvasNoBeginFrameKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// These values come from the WhatWG spec.
constexpr int kDefaultCanvasWidth = 300;
constexpr int kDefaultCanvasHeight = 150;

// A default value of quality argument for toDataURL and toBlob
// It is in an invalid range (outside 0.0 - 1.0) so that it will not be
// misinterpreted as a user-input value
constexpr int kUndefinedQualityValue = -1.0;
constexpr int kMinimumAccelerated2dCanvasSize = 128 * 129;

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

// Tracks whether `transferToGPUTexture()` has been invoked on any canvas
// element created within the associated Document.
class TransferToGPUTextureInvokedSupplement final
    : public GarbageCollected<TransferToGPUTextureInvokedSupplement>,
      public Supplement<Document> {
 public:
  static constexpr char kSupplementName[] =
      "TransferToGPUTextureInvokedSupplement";

  static TransferToGPUTextureInvokedSupplement& From(Document& d) {
    TransferToGPUTextureInvokedSupplement* supplement =
        Supplement<Document>::From<TransferToGPUTextureInvokedSupplement>(d);
    if (!supplement) {
      supplement =
          MakeGarbageCollected<TransferToGPUTextureInvokedSupplement>(d);
      ProvideTo(d, supplement);
    }
    return *supplement;
  }

  explicit TransferToGPUTextureInvokedSupplement(Document& d)
      : Supplement<Document>(d) {}

  void SetTransferToGPUTextureWasInvoked() {
    transfer_to_gpu_texture_was_invoked_ = true;
  }

  bool TransferToGPUTextureWasInvoked() {
    return transfer_to_gpu_texture_was_invoked_;
  }

 private:
  bool transfer_to_gpu_texture_was_invoked_ = false;
};

// Adapter for wrapping a CanvasResourceReleaseCallback into a
// viz::ReleaseCallback
void ReleaseCanvasResource(CanvasResource::ReleaseCallback callback,
                           scoped_refptr<CanvasResource> canvas_resource,
                           const gpu::SyncToken& sync_token,
                           bool is_lost) {
  std::move(callback).Run(std::move(canvas_resource), sync_token, is_lost);
}

void UmaHistogramCompressionRatio(
    std::string_view histogram_name,
    const String& data_url,
    const CanvasContextCreationAttributesCore& canvas_attrs,
    const gfx::Size& image_size) {
  constexpr int32_t kPrefixSize =
      std::string_view("data:image/png;base64,").size();
  base::ClampedNumeric<int32_t> size_of_data_uri = data_url.length();
  if (size_of_data_uri <= kPrefixSize) {
    // Don't log UMA after an encoding failure.
    return;
  }

  // 4 base64 characters per 3 bytes.
  base::ClampedNumeric<int32_t> encoded_bytes_count =
      (size_of_data_uri - kPrefixSize) * 3 / 4;

  // Assuming that canvas always uses RGBA (i.e. 4 channels).
  constexpr int32_t kChannelsPerPixel = 4u;
  int32_t bytes_per_pixel = 0u;
  switch (canvas_attrs.pixel_format) {
    case CanvasPixelFormat::kF16:
      bytes_per_pixel = 2u * kChannelsPerPixel;
      break;
    case CanvasPixelFormat::kUint8:
      bytes_per_pixel = 1u * kChannelsPerPixel;
      break;
  }

  base::ClampedNumeric<int32_t> image_pixels_count = image_size.Area64();
  base::ClampedNumeric<int32_t> image_bytes_count =
      image_pixels_count * bytes_per_pixel;
  base::ClampedNumeric<int32_t> encoded_bytes_per_100_image_bytes =
      encoded_bytes_count * 100 / image_bytes_count;

  // We are not just using `base::UmaHistogramPercentage` because the overhead
  // of PNG metadata may result in `encoded_bytes_count` being more than 100% of
  // `image_bytes_count`.  For example, a PNG encoding of an image with a single
  // pixel will take at least 67 bytes, although below we ignore this extreme
  // and only support buckets up to 120%.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, 121, 60,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(encoded_bytes_per_100_image_bytes);
}

}  // namespace

HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(html_names::kCanvasTag, document),
      ExecutionContextLifecycleObserver(GetExecutionContext()),
      PageVisibilityObserver(document.GetPage()),
      CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kCanvasHost,
          gfx::Size(kDefaultCanvasWidth, kDefaultCanvasHeight)),
      context_creation_was_blocked_(false),
      origin_clean_(true),
      surface_layer_bridge_(nullptr) {
  UseCounter::Count(document, WebFeature::kHTMLCanvasElement);
  // Create supplements now, as they may be needed at a
  // time when garbage collected objects can not be created.
  DisabledAccelerationCounterSupplement::From(GetDocument());
  TransferToGPUTextureInvokedSupplement::From(GetDocument());
  GetDocument().IncrementNumberOfCanvases();
  auto* execution_context = GetExecutionContext();
  if (execution_context) {
    CanvasResourceTracker::For(execution_context->GetIsolate())
        ->Add(this, execution_context);
  }
  SetHasCustomStyleCallbacks();
}

HTMLCanvasElement::~HTMLCanvasElement() = default;

bool HTMLCanvasElement::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  // The only context type that sets up HTMLCanvasElement as a
  // TextureLayerClient is CanvasRenderingContext2D.
  CHECK(IsRenderingContext2D());

  CHECK(cc_layer_);  // This explodes if FinalizeFrame() was not called.

  frames_since_last_commit_ = 0;
  if (rate_limiter_) {
    rate_limiter_->Reset();
  }

  // The beforeprint event listener is sometimes scheduled in the same task
  // as BeginFrame, which means that this code may sometimes be called between
  // the event listener and its associated FinalizeFrame call. So in order to
  // preserve the display list for printing, FlushRecording needs to know
  // whether any printing occurred in the current task.
  FlushReason reason = FlushReason::kCanvasPushFrame;
  if (RenderingContext()->did_print_in_current_task() || IsPrinting()) {
    reason = FlushReason::kCanvasPushFrameWhilePrinting;
  }

  scoped_refptr<CanvasResource> frame =
      RenderingContext()->PaintRenderingResultsToResource(kBackBuffer, reason);
  if (!frame || !frame->IsValid()) {
    return false;
  }

  CanvasResource::ReleaseCallback release_callback;
  if (!frame->PrepareTransferableResource(out_resource, &release_callback,
                                          /*needs_verified_synctoken=*/false) ||
      *out_resource == cc_layer_->current_transferable_resource()) {
    // If the resource did not change, the release will be handled correctly
    // when the callback from the previous frame is dispatched. But run the
    // |release_callback| to release the ref acquired above.
    std::move(release_callback)
        .Run(std::move(frame), gpu::SyncToken(), false /* is_lost */);
    return false;
  }
  // TODO(https://crbug.com/1475955): HDR metadata should be propagated to
  // `frame`, and should be populated by the above call to
  // CanvasResource::PrepareTransferableResource, rather than be inserted
  // here.
  out_resource->hdr_metadata = hdr_metadata_;
  // Note: frame is kept alive via a reference kept in out_release_callback.
  *out_release_callback = blink::BindOnce(
      ReleaseCanvasResource, std::move(release_callback), std::move(frame));

  return true;
}

void HTMLCanvasElement::Dispose() {
  disposing_ = true;
  // It's possible that the placeholder frame has been disposed but its ID still
  // exists. Make sure that it gets unregistered here
  UnregisterPlaceholderCanvas();

  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  frame_dispatcher_ = nullptr;
  DiscardResources();

  if (context_) {
    if (context_->Host())
      context_->DetachHost();
    context_ = nullptr;
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
  // Detect assignments to width/height and kick off any needed processing
  // *unless* the assignment is happening from within SetSize(), in which case
  // the OnWidthOrHeightAssigned() call will be made from SetSize() after both
  // attributes are assigned.
  if ((params.name == html_names::kWidthAttr ||
       params.name == html_names::kHeightAttr) &&
      !within_set_size_) {
    OnWidthOrHeightAssigned();
  }
  HTMLElement::ParseAttribute(params);
}

void HTMLCanvasElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (RuntimeEnabledFeatures::CanvasDrawElementEnabled() &&
      params.name == html_names::kLayoutsubtreeAttr) {
    setLayoutSubtree(!params.new_value.IsNull());
  }
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
  SetIsCanvasOrInCanvasSubtree(true);
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

void HTMLCanvasElement::setLayoutSubtree(bool value) {
  if (layoutSubtree() == value) {
    return;
  }

  SetBooleanAttribute(html_names::kLayoutsubtreeAttr, value);
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kAttribute));
  SetForceReattachLayoutTree();
  if (auto* object = GetLayoutObject()) {
    object->SetNeedsLayout(layout_invalidation_reason::kAttributeChanged);
  }
}

bool HTMLCanvasElement::layoutSubtree() const {
  return FastHasAttribute(html_names::kLayoutsubtreeAttr);
}

void HTMLCanvasElement::SetSize(gfx::Size new_size) {
  if (new_size == Size())
    return;
  within_set_size_ = true;
  SetIntegralAttribute(html_names::kWidthAttr, new_size.width());
  SetIntegralAttribute(html_names::kHeightAttr, new_size.height());
  OnWidthOrHeightAssigned();
  within_set_size_ = false;
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

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    const String& type,
    const CanvasContextCreationAttributesCore& attributes) {
  auto* old_contents_cc_layer = ContentsCcLayer();
  auto* result =
      GetCanvasRenderingContextInternal(execution_context, type, attributes);

  if (ContentsCcLayer() != old_contents_cc_layer)
    SetNeedsCompositingUpdate();

  return result;
}

bool HTMLCanvasElement::IsPageVisible() const {
  return GetPage() && GetPage()->IsPageVisible();
}

CanvasRenderingContext* HTMLCanvasElement::GetCanvasRenderingContextInternal(
    ExecutionContext* execution_context,
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

  context_ = factory->Create(execution_context, this, recomputed_attributes);
  if (!context_)
    return nullptr;

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
    GetOrCreateResourceDispatcher();
    UseCounter::Count(GetDocument(), WebFeature::kHTMLCanvasElementLowLatency);
  }

  // A 2D context does not know before lazy creation whether or not it is
  // direct composited.
  if (!IsRenderingContext2D())
    SetNeedsCompositingUpdate();

  is_opaque_ = SkAlphaTypeIsOpaque(GetRenderingContextAlphaType());
  if (cc_layer_) {
    cc_layer_->SetContentsOpaque(is_opaque_);
    cc_layer_->SetBlendBackgroundColor(!is_opaque_);
  }

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

  hdr_metadata_ = hdr_metadata;
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
  if (rect.isEmpty()) {
    return;
  }

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

void HTMLCanvasElement::InitializeLayerWithCSSProperties(cc::Layer* layer) {
  layer->SetFilterQuality(filter_quality_);
  layer->SetDynamicRangeLimit(dynamic_range_limit_);
}

void HTMLCanvasElement::PostFinalizeFrame(FlushReason reason) {
  RecordCanvasSizeToUMA();

  if (IsWebGL()) {
    context_->ClearMarkedCanvasDirty();
  }

  // Note: LowLatencyEnabled() could be true for any context type as it just
  // checks whether the `desynchronized` attribute is set on the context, but
  // only WebGL and Canvas2D have specific flows for low latency (for other
  // context types, setting the attribute is a no-op).
  if (LowLatencyEnabled() && (IsWebGL() || IsRenderingContext2D()) &&
      frame_dispatcher_ && !dirty_rect_.IsEmpty()) {
    if (scoped_refptr<CanvasResource> canvas_resource =
            context_->PaintRenderingResultsToResource(kBackBuffer, reason)) {
      const gfx::Rect src_rect(Size());
      dirty_rect_.Intersect(src_rect);
      const gfx::Rect int_dirty = dirty_rect_;
      const SkIRect damage_rect = SkIRect::MakeXYWH(
          int_dirty.x(), int_dirty.y(), int_dirty.width(), int_dirty.height());
      frame_dispatcher_->DispatchFrame(std::move(canvas_resource), damage_rect,
                                       IsOpaque());
      dirty_rect_ = gfx::Rect();
    }
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

  NotifyCachesOfSwitchingFrame();
}

void HTMLCanvasElement::OnAccelerationDisabled() {
  CHECK(IsRenderingContext2D());
  DisabledAccelerationCounterSupplement::From(GetDocument())
      .IncrementDisabledCount();
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

    if (cc_layer_ && context_->IsComposited()) {
      cc_layer_->SetNeedsDisplayRect(gfx::ToEnclosingRect(invalidation_rect));
    }
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

void HTMLCanvasElement::OnWidthOrHeightAssigned() {
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

  // For Canvas2D, reuse any existing backing buffer rather than dropping and
  // recreating it. Doing this optimization for WebGL and WebGPU would require
  // investigation to make sure that it's spec-compliant (including adding any
  // operations as necessary to get it compliant).
  if (IsRenderingContext2D() && old_size == new_size) {
    return;
  }

  size_ = new_size;
  if (RenderingContext()) {
    RenderingContext()->SizeChanged();
  }

  DiscardResources();
  if (IsRenderingContext2D() && context_->isContextLost()) {
    context_->RestoreFromInvalidSizeIfNeeded();
  }
  if (frame_dispatcher_) {
    frame_dispatcher_->Reshape(Size());
  }

  if ((IsWebGL() && old_size != Size()) || IsWebGPU()) {
    context_->Reshape(width(), height());
    UpdateMemoryUsage();
  }

  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (layout_object->IsCanvas()) {
      if (old_size != Size())
        To<LayoutHTMLCanvas>(layout_object)->CanvasSizeChanged();
      layout_object->SetShouldDoFullPaintInvalidation();
    }
  }
}

void HTMLCanvasElement::ResetLayer() {
  if (cc_layer_) {
    // Orphaning the layer is required to trigger the recreation of a new
    // layer in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    cc_layer_->RemoveFromParent();
    cc_layer_->ClearClient();
    cc_layer_ = nullptr;
  }
}

gfx::Vector2dF HTMLCanvasElement::PhysicalPixelToCanvasGridScaleFactor() {
  if (!GetDocument().View()) {
    return {1., 1.};
  }

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kCanvasDrawElementImage);
  if (!GetLayoutBox()) {
    return {1., 1.};
  }

  // As a special case, if the canvas is sized to its devicePixelContentBox,
  // make sure the element's physical pixels are mapped 1:1 to the canvas
  // grid to avoid any inadverent fuzziness due to rounding.
  gfx::Size canvas_size = Size();
  gfx::Size device_pixel_content_box =
      ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
          LogicalSize(GetLayoutBox()->ContentLogicalWidth(),
                      GetLayoutBox()->ContentLogicalHeight()),
          *GetLayoutBox(), GetLayoutBox()->StyleRef());
  if (canvas_size == device_pixel_content_box) {
    return gfx::Vector2dF(1., 1.);
  }

  PhysicalRect content_rect;
  if (auto* replaced = DynamicTo<LayoutReplaced>(GetLayoutBox())) {
    content_rect = replaced->ReplacedContentRect();
  } else {
    content_rect = GetLayoutBox()->PhysicalContentBoxRect();
  }
  return gfx::Vector2dF(canvas_size.width() / content_rect.Width().ToFloat(),
                        canvas_size.height() / content_rect.Height().ToFloat());
}

namespace {

// Given a transform at the origin, return an adjusted transform that is
// equivalent, but can be applied to `element` given the current
// `transform-origin`.
DOMMatrix* AdjustTransformByTransformOrigin(const Element* element,
                                            DOMMatrix* transform) {
  gfx::Point3F origin_css;
  if (LayoutBox* box = element ? element->GetLayoutBox() : nullptr) {
    const PhysicalRect reference_box = ComputeReferenceBox(*box);
    const ComputedStyle& style = box->StyleRef();

    gfx::Point3F origin_phys;
    origin_phys.set_x(FloatValueForLength(style.GetTransformOrigin().X(),
                                          reference_box.Width()));
    origin_phys.set_y(FloatValueForLength(style.GetTransformOrigin().Y(),
                                          reference_box.Height()));
    origin_phys.set_z(style.GetTransformOrigin().Z());
    origin_css = ScalePoint(origin_phys, 1.0f / style.EffectiveZoom());
  }

  DOMMatrix* result = DOMMatrix::Create();
  result->translateSelf(-origin_css.x(), -origin_css.y(), -origin_css.z());
  result->multiplySelf(*transform);
  result->translateSelf(origin_css.x(), origin_css.y(), origin_css.z());
  return result;
}

}  // namespace

DOMMatrix* HTMLCanvasElement::getElementTransform(
    Element* element,
    DOMMatrix* draw_transform,
    ExceptionState& exception_state) {
  DOMMatrix* result = DOMMatrix::Create();

  // This is a change of basis for a transform in canvas pixel grid coordinates
  // to a canvas in css coordinates. The general formula is:
  // T_css = S_canvas_to_css * T_canvas * S_canvas_to_css-1
  gfx::Vector2dF physical_to_canvas_grid =
      PhysicalPixelToCanvasGridScaleFactor();
  float physical_to_css = 1.0f / element->ComputedStyleRef().EffectiveZoom();
  float canvas_grid_to_css_x = physical_to_css / physical_to_canvas_grid.x();
  float canvas_grid_to_css_y = physical_to_css / physical_to_canvas_grid.y();
  result->scaleSelf(canvas_grid_to_css_x, canvas_grid_to_css_y);
  result->multiplySelf(*draw_transform);
  result->scaleSelf(1.0f / canvas_grid_to_css_x, 1.0f / canvas_grid_to_css_y);

  return AdjustTransformByTransformOrigin(element, result);
}

bool HTMLCanvasElement::PaintsIntoCanvasBuffer() const {
  if (HasOffscreenCanvasFrame()) {
    return false;
  }
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
      context_ && SkAlphaTypeIsOpaque(context_->GetAlphaType());

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
      source_image = GetSourceImageForCanvasInternal(&status);
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
  return context_ && context_->CreationAttributes().desynchronized;
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
  if (!context_ && !HasOffscreenCanvasFrame()) {
    return;
  }

  // If the canvas is gpu composited, it has another way of getting to screen
  if (!PaintsIntoCanvasBuffer()) {
    // For click-and-drag or printing we still want to draw
    if (!(flatten_composited_layers ||
          GetDocument().IsPrintingOrPaintingPreview())) {
      return;
    }
  }

  if (HasOffscreenCanvasFrame()) {
    DCHECK(GetDocument().IsPrintingOrPaintingPreview());
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
  if (IsPrinting() && IsRenderingContext2D()) {
    RenderingContext()->FlushCanvas(FlushReason::kPrinting);
    // `FlushRecording` might be a no-op if a flush already happened before.
    // Fortunately, the last flush recording was kept by the context.
    const std::optional<cc::PaintRecord>& last_recording =
        RenderingContext()->GetLastRecordingForCanvas2D();
    if (last_recording.has_value() &&
        filter_quality_ != cc::PaintFlags::FilterQuality::kNone) {
      context.Canvas()->save();
      context.Canvas()->translate(r.X(), r.Y());
      context.Canvas()->scale(r.Width() / Size().width(),
                              r.Height() / Size().height());
      context.Canvas()->drawPicture(*last_recording);
      context.Canvas()->restore();
      UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", true);
      return;
    }
    UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DPrintingAsVector", false);
  }

  // Grab a snapshot.
  scoped_refptr<StaticBitmapImage> snapshot =
      context_->PaintRenderingResultsToSnapshot(kFrontBuffer);

  if (snapshot) {
    SkBlendMode composite_operator =
        !context_ || context_->CreationAttributes().alpha
            ? SkBlendMode::kSrcOver
            : SkBlendMode::kSrc;
    gfx::RectF src_rect((gfx::SizeF(Size())));

    // GraphicsContext cannot handle gpu resource serialization.
    snapshot = snapshot->MakeUnaccelerated();
    DCHECK(!snapshot->IsTextureBacked());
    context.DrawImage(*snapshot, Image::kSyncDecode,
                      ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                      gfx::RectF(ToPixelSnappedRect(r)), &src_rect,
                      composite_operator);
  } else {
    // When alpha is false, we should draw to opaque black.
    if (!context_->CreationAttributes().alpha) {
      context.FillRect(
          gfx::RectF(r), Color(0, 0, 0),
          PaintAutoDarkMode(ComputedStyleRef(),
                            DarkModeFilter::ElementRole::kBackground));
    }
  }

  if (IsWebGL() && PaintsIntoCanvasBuffer()) {
    context_->MarkLayerComposited();
  }
}

bool HTMLCanvasElement::IsPrinting() const {
  return GetDocument().BeforePrintingOrPrinting();
}

UkmParameters HTMLCanvasElement::GetUkmParameters() {
  return {GetDocument().UkmRecorder(), GetDocument().UkmSourceID()};
}

const AtomicString HTMLCanvasElement::ImageSourceURL() const {
  return AtomicString(ToDataURLInternal(
      ImageEncoderUtils::kDefaultRequestedMimeType, 0, kFrontBuffer));
}

scoped_refptr<StaticBitmapImage> HTMLCanvasElement::Snapshot(
    SourceDrawingBuffer source_buffer) const {
  if (Size().IsEmpty()) {
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> image_bitmap;
  if (HasOffscreenCanvasFrame()) {  // Offscreen Canvas
    DCHECK(OffscreenCanvasFrame()->OriginClean());
    image_bitmap = OffscreenCanvasFrame()->Bitmap();
  } else if (IsWebGL()) {
    if (context_->CreationAttributes().premultiplied_alpha) {
      image_bitmap = context_->PaintRenderingResultsToSnapshot(source_buffer);
    } else {
      image_bitmap =
          context_->GetRGBAUnacceleratedStaticBitmapImage(source_buffer);
    }
  } else if (context_) {
    DCHECK(IsRenderingContext2D() || IsImageBitmapRenderingContext() ||
           IsWebGPU());
    image_bitmap = context_->GetImage();
  }

  if (!image_bitmap) {
    image_bitmap = CreateTransparentImage();
  }

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
      const CanvasRenderingContext* context = RenderingContext();
      if (context) {
        UmaHistogramCompressionRatio(
            "Blink.Canvas.ToDataURLCompressionRatio.PNG", data_url,
            context->CreationAttributes(), image_bitmap->Size());
      }
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
                   BindOnce(&V8BlobCallback::InvokeAndReportException,
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
    options->setType(ImageEncoderUtils::MimeTypeName(encoding_mime_type));
    async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options,
        CanvasAsyncBlobCreator::kHTMLCanvasToBlobCallback, callback, start_time,
        GetExecutionContext());
  }

  if (async_creator) {
    async_creator->ScheduleAsyncBlobCreation(quality);
  } else {
    GetDocument()
        .GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   BindOnce(&V8BlobCallback::InvokeAndReportException,
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
    HeapVector<CSSPropertyValue, 8>& style) {
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
  if (HasOffscreenCanvasFrame()) {
    return OffscreenCanvasFrame()->OriginClean();
  }
  return origin_clean_;
}

bool HTMLCanvasElement::ShouldAccelerate2dContext() const {
  return ShouldAccelerate();
}

CanvasResourceDispatcher* HTMLCanvasElement::GetOrCreateResourceDispatcher() {
  if (!frame_dispatcher_ && context_ &&
      context_->CreationAttributes().desynchronized) {
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
  }
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

  if (context_ &&
      context_->CreationAttributes().will_read_frequently ==
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined &&
      DisabledAccelerationCounterSupplement::From(GetDocument())
          .ShouldDisableAcceleration()) {
    return false;
  }

  // Avoid creating |contextProvider| until we're sure we want to try use it,
  // since it costs us GPU memory.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper) {
    return false;
  }

  return context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

bool HTMLCanvasElement::CanStartSelection() const {
  if (!layoutSubtree()) {
    return false;
  }
  return HTMLElement::CanStartSelection();
}

bool HTMLCanvasElement::ShouldDisableAccelerationBecauseOfReadback() const {
  return DisabledAccelerationCounterSupplement::From(GetDocument())
      .ShouldDisableAcceleration();
}

void HTMLCanvasElement::NotifyGpuContextLost() {
  if (IsRenderingContext2D()) {
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);
  }
}

void HTMLCanvasElement::Trace(Visitor* visitor) const {
  visitor->Trace(listeners_);
  visitor->Trace(context_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  CanvasRenderingContextHost::Trace(visitor);
  HTMLElement::Trace(visitor);
  CanvasRenderingContextHost::Trace(visitor);
}

void HTMLCanvasElement::UpdatePreferred2DRasterMode() {
  // If the canvas meets the criteria to use accelerated-GPU rendering, and
  // the user signals that the canvas will not be read frequently through
  // getImageData, which is a slow operation with GPU, the canvas will try to
  // use accelerated-GPU rendering.
  // If any of the two conditions fails, or if the creation of accelerated
  // resource provider fails, the canvas will fallback to CPU rendering.
  bool will_read_frequently =
      context_ &&
      context_->CreationAttributes().will_read_frequently ==
          CanvasContextCreationAttributesCore::WillReadFrequently::kTrue;
  UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DLayerBridge.WillReadFrequently",
                        will_read_frequently);

  RasterModeHint hint = ShouldAccelerate() && !will_read_frequently
                            ? RasterModeHint::kPreferGPU
                            : RasterModeHint::kPreferCPU;
  SetPreferred2DRasterMode(hint);
}

SharedContextRateLimiter* HTMLCanvasElement::RateLimiter() const {
  return rate_limiter_.get();
}

void HTMLCanvasElement::CreateRateLimiter() {
  rate_limiter_ =
      std::make_unique<SharedContextRateLimiter>(kMaxCanvasAnimationBacklog);
}

void HTMLCanvasElement::SetIsDisplayed(bool displayed) {
  is_displayed_ = displayed;
  // If the canvas is no longer being displayed, stop using the rate
  // limiter.
  if (!is_displayed_) {
    frames_since_last_commit_ = 0;
    if (rate_limiter_) {
      rate_limiter_->Reset();
      rate_limiter_.reset(nullptr);
    }
  }
}

cc::TextureLayer* HTMLCanvasElement::GetOrCreateCcLayerForCanvas2DIfNeeded() {
  CHECK(IsRenderingContext2D());
  CHECK(context_->IsComposited());

  if (!cc_layer_) [[unlikely]] {
    cc_layer_ = cc::TextureLayer::Create(this);
    InitializeLayerWithCSSProperties(cc_layer_.get());
    cc_layer_->SetIsDrawable(true);
    cc_layer_->SetHitTestable(true);
    cc_layer_->SetContentsOpaque(is_opaque_);
    cc_layer_->SetBlendBackgroundColor(!is_opaque_);
  }
  return cc_layer_.get();
}

void HTMLCanvasElement::ClearCanvas2DLayerTexture() {
  CHECK(IsRenderingContext2D());
  if (cc_layer_) {
    cc_layer_->ClearTexture();
  }
}

void HTMLCanvasElement::SetNeedsPushProperties() {
  if (cc_layer_) {
    cc_layer_->SetNeedsSetTransferableResource();
  }
}

void HTMLCanvasElement::DiscardResources() {
  ResetLayer();
  UpdateMemoryUsage();
  dirty_rect_ = gfx::Rect();
}

void HTMLCanvasElement::UpdateSuspendOffscreenCanvasAnimation() {
  if (!GetPage()) {
    return;
  }

  CanvasResourceDispatcher::AnimationState animation_state =
      CanvasResourceDispatcher::AnimationState::kActive;
  const bool is_hidden = GetPage()->GetVisibilityState() ==
                         mojom::blink::PageVisibilityState::kHidden;
  if (is_hidden) {
    if (HasCanvasCapture()) {
      const bool allow_synthetic_timing =
          RuntimeEnabledFeatures::AllowSyntheticTimingForCanvasCaptureEnabled();
      animation_state = allow_synthetic_timing
                            ? CanvasResourceDispatcher::AnimationState::
                                  kActiveWithSyntheticTiming
                            : CanvasResourceDispatcher::AnimationState::kActive;
    } else {
      animation_state = CanvasResourceDispatcher::AnimationState::kSuspended;
    }
  }

  SetSuspendOffscreenCanvasAnimation(animation_state);
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
  const auto new_filter_quality =
      (new_style.ImageRendering() == EImageRendering::kPixelated)
          ? cc::PaintFlags::FilterQuality::kNone
          : cc::PaintFlags::FilterQuality::kLow;
  const auto new_dynamic_range_limit = new_style.GetDynamicRangeLimit();
  if (filter_quality_ != new_filter_quality ||
      dynamic_range_limit_ != new_dynamic_range_limit) {
    filter_quality_ = new_filter_quality;
    dynamic_range_limit_ = new_dynamic_range_limit;

    if (cc::Layer* cc_layer = ContentsCcLayer()) {
      cc_layer->SetFilterQuality(filter_quality_);
      cc_layer->SetDynamicRangeLimit(dynamic_range_limit_);
    }
  }

  style_is_visible_ = new_style.Visibility() == EVisibility::kVisible;
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

void HTMLCanvasElement::LangAttributeChanged() {
  if (context_) {
    context_->LangAttributeChanged();
  }
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

void HTMLCanvasElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (hasChildren()) {
    UseCounter::Count(GetDocument(), WebFeature::kCanvasFallbackContent);
    if (firstElementChild()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kCanvasFallbackElementContent);
    }
  }
}

scoped_refptr<Image> HTMLCanvasElement::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const gfx::SizeF&) {
  return GetSourceImageForCanvasInternal(status);
}

scoped_refptr<StaticBitmapImage>
HTMLCanvasElement::GetSourceImageForCanvasInternal(SourceImageStatus* status) {
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

  if (HasOffscreenCanvasFrame()) {
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
      // we use PaintRenderingResultsToSnapshot instead of GetImage in order to
      // keep a cached copy of the backing in the canvas's resource provider.
      image = RenderingContext()->PaintRenderingResultsToSnapshot(kBackBuffer);
    } else if (RenderingContext()) {
      // This is either CanvasRenderingContext2D or ImageBitmapRenderingContext.
      image = RenderingContext()->GetImage();
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
  return image;
}

bool HTMLCanvasElement::WouldTaintOrigin() const {
  return !OriginClean();
}

gfx::SizeF HTMLCanvasElement::ElementSize(
    const gfx::SizeF&,
    const RespectImageOrientationEnum) const {
  if (IsImageBitmapRenderingContext()) {
    scoped_refptr<Image> image = RenderingContext()->GetImage();
    if (image) {
      return gfx::SizeF(image->width(), image->height());
    }
    return gfx::SizeF(0, 0);
  }
  if (HasOffscreenCanvasFrame()) {
    return gfx::SizeF(OffscreenCanvasFrame()->Size());
  }
  return gfx::SizeF(width(), height());
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
      frame->GetPage()->GetChromeClient().GetFrameSinkId(frame), this,
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
  // This is called when a new SurfaceLayer (or sometimes SolidColorLayer) is
  // attached to `surface_layer_bridge_`. Initialize the paint flags for this
  // layer from the CSS properties of this element.
  InitializeLayerWithCSSProperties(layer);
  SetNeedsCompositingUpdate();
}

void HTMLCanvasElement::UnregisterContentsLayer(cc::Layer* layer) {
  SetNeedsCompositingUpdate();
}

TextDirection HTMLCanvasElement::GetTextDirection(const ComputedStyle* style) {
  // In the absence of an explicit CSS direction property, the HTML dir
  // attribute has been pushed as the default style direction. So we fallback
  // to the HTML attribute if CSS has not provided a direction.
  if (!style) {
    GetDocument().UpdateStyleAndLayoutTreeForElement(
        this, DocumentUpdateReason::kCanvas);
    style = EnsureComputedStyle();
  }
  // Detached elements may still not have style.
  if (style) {
    if (CachedDirectionality() != style->Direction()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kCanvasTextDirectionConflict);
    }
    return style->Direction();
  }

  // In the absence of style, use the element's dir attribute to resolve the
  // direction. This value would have been pushed to the style had there been
  // a style.
  return CachedDirectionality();
}

const LayoutLocale* HTMLCanvasElement::GetLocale() const {
  const AtomicString language = ComputeInheritedLanguage();
  if (!language.IsNull()) {
    return LayoutLocale::Get(language);
  }

  // The spec says to use the language of the canvas element, so if it doesn't
  // have one we need to return the default (the "unknown language").
  return &LayoutLocale::GetDefault();
}

UniqueFontSelector* HTMLCanvasElement::GetFontSelector() {
  if (UniqueFontSelector* unique_font_selector = unique_font_selector_) {
    return unique_font_selector;
  }
  auto* unique_font_selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());
  unique_font_selector_ = unique_font_selector;
  return unique_font_selector;
}

scoped_refptr<StaticBitmapImage> HTMLCanvasElement::GetTransparentImage() {
  if (!transparent_image_ || transparent_image_.get()->Size() != Size()) {
    transparent_image_ = CreateTransparentImage();
  }
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

void HTMLCanvasElement::SetTransferToGPUTextureWasInvoked() {
  TransferToGPUTextureInvokedSupplement::From(GetDocument())
      .SetTransferToGPUTextureWasInvoked();
}

bool HTMLCanvasElement::TransferToGPUTextureWasInvoked() {
  return TransferToGPUTextureInvokedSupplement::From(GetDocument())
      .TransferToGPUTextureWasInvoked();
}

}  // namespace blink
