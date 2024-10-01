/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "cc/layers/texture_layer.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/key_value_pair.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

// UMA Histogram macros trigger a bug in IWYU.
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <atomic>
// IWYU pragma: no_include "base/metrics/histogram_base.h"

namespace base {
struct PendingTask;
}  // namespace base
namespace cc {
class PaintFlags;
}  // namespace cc

namespace blink {
class ExecutionContext;
class FontSelector;
class ImageData;
class ImageDataSettings;
class LayoutObject;
class SVGResource;

static mojom::blink::ColorScheme GetColorSchemeFromCanvas(
    HTMLCanvasElement* canvas) {
  if (canvas && canvas->isConnected()) {
    if (auto* style = canvas->GetComputedStyle()) {
      return style->UsedColorScheme();
    }
  }
  return mojom::blink::ColorScheme::kLight;
}

CanvasRenderingContext* CanvasRenderingContext2D::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  DCHECK(!host->IsOffscreenCanvas());
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<CanvasRenderingContext2D>(
          static_cast<HTMLCanvasElement*>(host), attrs);
  DCHECK(rendering_context);
  return rendering_context;
}

CanvasRenderingContext2D::CanvasRenderingContext2D(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(canvas, attrs, CanvasRenderingAPI::k2D),
      BaseRenderingContext2D(
          canvas->GetDocument().GetTaskRunner(TaskType::kInternalDefault)),
      should_prune_local_font_cache_(false),
      color_params_(attrs.color_space, attrs.pixel_format, attrs.alpha) {
  identifiability_study_helper_.SetExecutionContext(
      canvas->GetTopExecutionContext());
  if (canvas->GetDocument().GetSettings() &&
      canvas->GetDocument().GetSettings()->GetAntialiasedClips2dCanvasEnabled())
    clip_antialiasing_ = kAntiAliased;
  SetShouldAntialias(true);
  ValidateStateStack();
}

V8RenderingContext* CanvasRenderingContext2D::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

bool CanvasRenderingContext2D::IsOriginTopLeft() const {
  // Use top-left origin since Skia Graphite won't support bottom-left origin.
  return true;
}

bool CanvasRenderingContext2D::IsComposited() const {
  // The following case is necessary for handling the special case of canvases
  // in the dev tools overlay.
  const HTMLCanvasElement* const element = canvas();
  auto* settings = element->GetDocument().GetSettings();
  if (settings && !settings->GetAcceleratedCompositingEnabled()) {
    return false;
  }
  return element->IsComposited();
}

void CanvasRenderingContext2D::Stop() {
  if (!isContextLost()) [[likely]] {
    // Never attempt to restore the context because the page is being torn down.
    context_restorable_ = false;
    LoseContext(kSyntheticLostContext);
  }
}

void CanvasRenderingContext2D::SendContextLostEventIfNeeded() {
  if (!needs_context_lost_event_)
    return;

  needs_context_lost_event_ = false;
  dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void CanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  ResetInternal();
  HTMLCanvasElement* const element = canvas();
  if (element != nullptr) [[likely]] {
    if (context_lost_mode_ == kSyntheticLostContext) {
      element->DiscardResourceProvider();
    }

    if (element->IsPageVisible()) {
      dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(),
                                                      FROM_HERE);
      return;
    }
  }
  needs_context_lost_event_ = true;
}

void CanvasRenderingContext2D::DidSetSurfaceSize() {
  if (!context_restorable_)
    return;
  // This code path is for restoring from an eviction
  // Restoring from surface failure is handled internally
  DCHECK(context_lost_mode_ != kNotLostContext && !IsPaintable());

  if (CanCreateCanvas2dResourceProvider()) {
    dispatch_context_restored_event_timer_.StartOneShot(base::TimeDelta(),
                                                        FROM_HERE);
  }
}

void CanvasRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(filter_operations_);
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
  SVGResourceClient::Trace(visitor);
}

void CanvasRenderingContext2D::TryRestoreContextEvent(TimerBase* timer) {
  if (context_lost_mode_ == kNotLostContext) {
    // Canvas was already restored (possibly thanks to a resize), so stop
    // trying.
    try_restore_context_event_timer_.Stop();
    return;
  }

  DCHECK(context_lost_mode_ != kWebGLLoseContextLostContext);

  // If lost mode is |kSyntheticLostContext| and |context_restorable_| is set to
  // true, it means context is forced to be lost for testing purpose. Restore
  // the context.
  if (context_lost_mode_ == kSyntheticLostContext) {
    Canvas2DLayerBridge* bridge = canvas()->GetOrCreateCanvas2DLayerBridge();
    if (bridge && bridge->GetOrCreateResourceProvider()) {
      try_restore_context_event_timer_.Stop();
      DispatchContextRestoredEvent(nullptr);
      return;
    }
  }

  // If RealLostContext, it means the context was not lost due to surface
  // failure but rather due to a an eviction, which means image buffer exists.
  if (context_lost_mode_ == kRealLostContext && IsPaintable() && Restore()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
    return;
  }

  // If it fails to restore the context, TryRestoreContextEvent again.
  if (++try_restore_context_attempt_count_ > kMaxTryRestoreContextAttempts) {
    // After 4 tries, we start the final attempt, allocate a brand new image
    // buffer instead of restoring
    try_restore_context_event_timer_.Stop();
    if (CanvasRenderingContextHost* host = Host()) [[likely]] {
      host->DiscardResourceProvider();
    }
    if (CanCreateCanvas2dResourceProvider())
      DispatchContextRestoredEvent(nullptr);
  }
}

bool CanvasRenderingContext2D::Restore() {
  CanvasRenderingContextHost* host = Host();
  CHECK(host);
  CHECK(host->context_lost());
  if (host->GetRasterMode() == RasterMode::kCPU) {
    return false;
  }
  DCHECK(!host->ResourceProvider());

  host->ClearLayerTexture();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();

  if (!context_provider_wrapper->ContextProvider()->IsContextLost()) {
    CanvasResourceProvider* resource_provider =
        host->GetOrCreateCanvasResourceProviderImpl(RasterModeHint::kPreferGPU);

    // The current paradigm does not support switching from accelerated to
    // non-accelerated, which would be tricky due to changes to the layer tree,
    // which can only happen at specific times during the document lifecycle.
    // Therefore, we can only accept the restored surface if it is accelerated.
    if (resource_provider && host->GetRasterMode() == RasterMode::kCPU) {
      host->ReplaceResourceProvider(nullptr);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    } else {
      host->set_context_lost(false);
    }
  }

  host->UpdateMemoryUsage();

  return host->ResourceProvider();
}

void CanvasRenderingContext2D::WillDrawImage(CanvasImageSource* source) const {
  canvas()->WillDrawImageTo2DContext(source);
}

bool CanvasRenderingContext2D::WritePixels(const SkImageInfo& orig_info,
                                           const void* pixels,
                                           size_t row_bytes,
                                           int x,
                                           int y) {
  DCHECK(IsPaintable());
  CanvasRenderingContextHost* host = Host();
  CHECK(host);

  CanvasResourceProvider* provider =
      canvas()->GetCanvas2DLayerBridge()->GetOrCreateResourceProvider();
  if (provider == nullptr) {
    return false;
  }

  if (x <= 0 && y <= 0 && x + orig_info.width() >= host->Size().width() &&
      y + orig_info.height() >= host->Size().height()) {
    MemoryManagedPaintRecorder& recorder = provider->Recorder();
    if (recorder.HasSideRecording()) {
      // Even with opened layers, WritePixels would write to the main canvas
      // surface under the layers. We can therefore clear the paint ops recorded
      // before the first `beginLayer`, but the layers themselves must be kept
      // untouched. Note that this operation makes little sense and is actually
      // disabled in `putImageData` by raising an exception if layers are
      // opened. Still, it's preferable to handle this scenario here because the
      // alternative would be to crash or leave the canvas in an invalid state.
      recorder.ReleaseMainRecording();
    } else {
      recorder.RestartRecording();
    }
  } else {
    host->FlushRecording(FlushReason::kWritePixels);

    // Short-circuit out if an error occurred while flushing the recording.
    if (!host->ResourceProvider()->IsValid()) {
      return false;
    }
  }

  return host->ResourceProvider()->WritePixels(orig_info, pixels, row_bytes, x,
                                               y);
}

void CanvasRenderingContext2D::Reset() {
  // This is a multiple inheritance bootstrap
  BaseRenderingContext2D::ResetInternal();
}

void CanvasRenderingContext2D::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* c) const {
  RestoreMatrixClipStack(c);
}

bool CanvasRenderingContext2D::ShouldAntialias() const {
  return GetState().ShouldAntialias();
}

void CanvasRenderingContext2D::SetShouldAntialias(bool do_aa) {
  GetState().SetShouldAntialias(do_aa);
}

void CanvasRenderingContext2D::ScrollPathIntoViewInternal(const Path& path) {
  if (!IsTransformInvertible() || path.IsEmpty()) [[unlikely]] {
    return;
  }

  HTMLCanvasElement* const element = canvas();
  element->GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kJavaScript);

  LayoutObject* renderer = element->GetLayoutObject();
  LayoutBox* layout_box = element->GetLayoutBox();
  if (!renderer || !layout_box)
    return;

  const int width = Width();
  const int height = Height();
  if (width == 0 || height == 0) {
    return;
  }

  // Apply transformation and get the bounding rect
  Path transformed_path = path;
  transformed_path.Transform(GetState().GetTransform());
  gfx::RectF bounding_rect = transformed_path.BoundingRect();

  // We first map canvas coordinates to layout coordinates.
  PhysicalRect path_rect = PhysicalRect::EnclosingRect(bounding_rect);
  PhysicalRect canvas_rect = layout_box->PhysicalContentBoxRect();
  // TODO(fserb): Is this kIgnoreTransforms correct?
  canvas_rect.Move(
      layout_box->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms));
  path_rect.SetX(
      (canvas_rect.X() + path_rect.X() * canvas_rect.Width() / width));
  path_rect.SetY(
      (canvas_rect.Y() + path_rect.Y() * canvas_rect.Height() / height));
  path_rect.SetWidth((path_rect.Width() * canvas_rect.Width() / width));
  path_rect.SetHeight((path_rect.Height() * canvas_rect.Height() / height));

  // Then we clip the bounding box to the canvas visible range.
  path_rect.Intersect(canvas_rect);

  // Horizontal text is aligned at the top of the screen
  mojom::blink::ScrollAlignment horizontal_scroll_mode =
      ScrollAlignment::ToEdgeIfNeeded();
  mojom::blink::ScrollAlignment vertical_scroll_mode =
      ScrollAlignment::TopAlways();

  // Vertical text needs be aligned horizontally on the screen
  bool is_horizontal_writing_mode =
      element->EnsureComputedStyle()->IsHorizontalWritingMode();
  if (!is_horizontal_writing_mode) {
    bool is_right_to_left =
        element->EnsureComputedStyle()->IsFlippedBlocksWritingMode();
    horizontal_scroll_mode = (is_right_to_left ? ScrollAlignment::RightAlways()
                                               : ScrollAlignment::LeftAlways());
    vertical_scroll_mode = ScrollAlignment::ToEdgeIfNeeded();
  }
  scroll_into_view_util::ScrollRectToVisible(
      *renderer, path_rect,
      scroll_into_view_util::CreateScrollIntoViewParams(
          horizontal_scroll_mode, vertical_scroll_mode,
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kAuto));
}

void CanvasRenderingContext2D::clearRect(double x,
                                         double y,
                                         double width,
                                         double height) {
  BaseRenderingContext2D::clearRect(x, y, width, height);
}

sk_sp<PaintFilter> CanvasRenderingContext2D::StateGetFilter() {
  HTMLCanvasElement* const element = canvas();
  return GetState().GetFilter(element, element->Size(), this);
}

cc::PaintCanvas* CanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (isContextLost()) [[unlikely]] {
    return nullptr;
  }

  Canvas2DLayerBridge* bridge = canvas()->GetOrCreateCanvas2DLayerBridge();
  if (bridge == nullptr) [[unlikely]] {
    return nullptr;
  }

  CanvasResourceProvider* provider = ResourceProvider();
  if (provider != nullptr) [[likely]] {
    // If we already had a provider, we can check whether it recorded ops passed
    // the autoflush limit.
    if (layer_count_ == 0) [[likely]] {
      // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
      provider->FlushIfRecordingLimitExceeded();
    }
  } else {
    // If we have no provider, try creating one.
    provider = bridge->GetOrCreateResourceProvider();
    if (provider == nullptr) [[unlikely]] {
      return nullptr;
    }
  }

  return &provider->Recorder().getRecordingCanvas();
}

const cc::PaintCanvas* CanvasRenderingContext2D::GetPaintCanvas() const {
  if (isContextLost()) [[unlikely]] {
    return nullptr;
  }
  const CanvasResourceProvider* provider = ResourceProvider();
  if (!provider) [[unlikely]] {
    return nullptr;
  }
  return &provider->Recorder().getRecordingCanvas();
}

const MemoryManagedPaintRecorder* CanvasRenderingContext2D::Recorder() const {
  const CanvasResourceProvider* provider = ResourceProvider();
  if (provider == nullptr) [[unlikely]] {
    return nullptr;
  }
  return &provider->Recorder();
}

void CanvasRenderingContext2D::WillDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  if (ShouldAntialias()) {
    SkIRect inflated_dirty_rect = dirty_rect.makeOutset(1, 1);
    CanvasRenderingContext::DidDraw(inflated_dirty_rect, draw_type);
  } else {
    CanvasRenderingContext::DidDraw(dirty_rect, draw_type);
  }
  // Always draw everything during printing.
  if (CanvasResourceProvider* provider = ResourceProvider();
      layer_count_ == 0 && provider != nullptr) [[likely]] {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    provider->FlushIfRecordingLimitExceeded();
  }
}

std::optional<cc::PaintRecord> CanvasRenderingContext2D::FlushCanvas(
    FlushReason reason) {
  CanvasResourceProvider* provider = ResourceProvider();
  if (provider == nullptr) [[unlikely]] {
    return std::nullopt;
  }
  return provider->FlushCanvas(reason);
}

bool CanvasRenderingContext2D::WillSetFont() const {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  const HTMLCanvasElement* const element = canvas();
  Document& document = element->GetDocument();
  if (!document.GetFrame()) {
    return false;
  }

  document.UpdateStyleAndLayoutTreeForElement(element,
                                              DocumentUpdateReason::kCanvas);
  return true;
}

bool CanvasRenderingContext2D::CurrentFontResolvedAndUpToDate() const {
  // An empty cache may indicate that a style change has occurred
  // which would require that the font be re-resolved. This check has to
  // come after the layout tree update in WillSetFont() to flush pending
  // style changes.
  return GetState().HasRealizedFont() &&
         fonts_resolved_using_current_style_.size() > 0;
}

void CanvasRenderingContext2D::setFontForTesting(const String& new_font) {
  // Dependency inversion to allow BaseRenderingContext2D::setFont
  // to be invoked from core unit tests.
  setFont(new_font);
}

bool CanvasRenderingContext2D::ResolveFont(const String& new_font) {
  HTMLCanvasElement* const element = canvas();
  Document& document = element->GetDocument();
  CanvasFontCache* canvas_font_cache = document.GetCanvasFontCache();

  // Map the <canvas> font into the text style. If the font uses keywords like
  // larger/smaller, these will work relative to the canvas.
  const ComputedStyle* computed_style = element->EnsureComputedStyle();
  if (computed_style) {
    auto i = fonts_resolved_using_current_style_.find(new_font);
    if (i != fonts_resolved_using_current_style_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(!add_result.is_new_entry);
      GetState().SetFont(i->value, Host()->GetFontSelector());
    } else {
      MutableCSSPropertyValueSet* parsed_style =
          canvas_font_cache->ParseFont(new_font);
      if (!parsed_style)
        return false;
      ComputedStyleBuilder font_style_builder =
          document.GetStyleResolver().CreateComputedStyleBuilder();
      FontDescription element_font_description(
          computed_style->GetFontDescription());
      // Reset the computed size to avoid inheriting the zoom factor from the
      // <canvas> element.
      element_font_description.SetComputedSize(
          element_font_description.SpecifiedSize());
      element_font_description.SetAdjustedSize(
          element_font_description.SpecifiedSize());

      font_style_builder.SetFontDescription(element_font_description);
      const ComputedStyle* font_style = font_style_builder.TakeStyle();
      Font font = document.GetStyleEngine().ComputeFont(*element, *font_style,
                                                        *parsed_style);

      // We need to reset Computed and Adjusted size so we skip zoom and
      // minimum font size.
      FontDescription final_description(font.GetFontDescription());
      final_description.SetComputedSize(final_description.SpecifiedSize());
      final_description.SetAdjustedSize(final_description.SpecifiedSize());

      fonts_resolved_using_current_style_.insert(new_font, final_description);
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(add_result.is_new_entry);
      PruneLocalFontCache(canvas_font_cache->HardMaxFonts());  // hard limit
      should_prune_local_font_cache_ = true;  // apply soft limit
      GetState().SetFont(final_description, Host()->GetFontSelector());
    }
  } else {
    Font resolved_font;
    if (!canvas_font_cache->GetFontUsingDefaultStyle(*element, new_font,
                                                     resolved_font)) {
      return false;
    }

    // We need to reset Computed and Adjusted size so we skip zoom and
    // minimum font size for detached canvas.
    FontDescription final_description(resolved_font.GetFontDescription());
    final_description.SetComputedSize(final_description.SpecifiedSize());
    final_description.SetAdjustedSize(final_description.SpecifiedSize());
    GetState().SetFont(final_description, Host()->GetFontSelector());
  }
  return true;
}

void CanvasRenderingContext2D::DidProcessTask(
    const base::PendingTask& pending_task) {
  CanvasRenderingContext::DidProcessTask(pending_task);
  // This should be the only place where canvas() needs to be checked for
  // nullness because the circular refence with HTMLCanvasElement means the
  // canvas and the context keep each other alive. As long as the pair is
  // referenced, the task observer is the only persistent refernce to this
  // object
  // that is not traced, so didProcessTask() may be called at a time when the
  // canvas has been garbage collected but not the context.
  const HTMLCanvasElement* const element = canvas();
  if (should_prune_local_font_cache_) {
    if (element != nullptr) [[likely]] {
      should_prune_local_font_cache_ = false;
      PruneLocalFontCache(
          element->GetDocument().GetCanvasFontCache()->MaxFonts());
    }
  }
}

void CanvasRenderingContext2D::PruneLocalFontCache(size_t target_size) {
  if (target_size == 0) {
    // Short cut: LRU does not matter when evicting everything
    font_lru_list_.clear();
    fonts_resolved_using_current_style_.clear();
    return;
  }
  while (font_lru_list_.size() > target_size) {
    fonts_resolved_using_current_style_.erase(font_lru_list_.back());
    font_lru_list_.pop_back();
  }
}

void CanvasRenderingContext2D::StyleDidChange(const ComputedStyle* old_style,
                                              const ComputedStyle& new_style) {
  if (old_style && old_style->GetFont() == new_style.GetFont())
    return;
  PruneLocalFontCache(0);
}

void CanvasRenderingContext2D::ClearFilterReferences() {
  filter_operations_.RemoveClient(*this);
  filter_operations_.clear();
}

void CanvasRenderingContext2D::UpdateFilterReferences(
    const FilterOperations& filters) {
  filters.AddClient(*this);
  ClearFilterReferences();
  filter_operations_ = filters;
}

void CanvasRenderingContext2D::ResourceContentChanged(SVGResource*) {
  ClearFilterReferences();
  GetState().ClearResolvedFilter();
}

bool CanvasRenderingContext2D::OriginClean() const {
  return Host()->OriginClean();
}

void CanvasRenderingContext2D::SetOriginTainted() {
  Host()->SetOriginTainted();
}

int CanvasRenderingContext2D::Width() const {
  return Host()->Size().width();
}

int CanvasRenderingContext2D::Height() const {
  return Host()->Size().height();
}

bool CanvasRenderingContext2D::CanCreateCanvas2dResourceProvider() const {
  return canvas()->GetOrCreateCanvas2DLayerBridge();
}

scoped_refptr<StaticBitmapImage> blink::CanvasRenderingContext2D::GetImage(
    FlushReason reason) {
  if (!IsPaintable())
    return nullptr;

  Canvas2DLayerBridge* bridge = canvas()->GetCanvas2DLayerBridge();

  CanvasHibernationHandler& hibernation_handler =
      bridge->GetHibernationHandler();

  if (hibernation_handler.IsHibernating()) {
    return UnacceleratedStaticBitmapImage::Create(
        hibernation_handler.GetImage());
  }

  if (!Host()->IsResourceValid()) {
    return nullptr;
  }
  // GetOrCreateResourceProvider needs to be called before FlushRecording, to
  // make sure "hint" is properly taken into account.
  if (!bridge->GetOrCreateResourceProvider()) {
    return nullptr;
  }
  Host()->FlushRecording(reason);
  return Host()->ResourceProvider()->Snapshot(reason);
}

ImageData* CanvasRenderingContext2D::getImageDataInternal(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  UMA_HISTOGRAM_BOOLEAN(
      "Blink.Canvas.GetImageData.WillReadFrequently",
      CreationAttributes().will_read_frequently ==
          CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  return BaseRenderingContext2D::getImageDataInternal(
      sx, sy, sw, sh, image_data_settings, exception_state);
}

void CanvasRenderingContext2D::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "CanvasRenderingContext2D::FinalizeFrame");
  if (!IsPaintable()) {
    return;
  }

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!canvas()->GetCanvas2DLayerBridge()->GetOrCreateResourceProvider()) {
    return;
  }

  CanvasRenderingContextHost* host = Host();
  CHECK(host);

  host->FlushRecording(reason);
  if (reason == FlushReason::kCanvasPushFrame) {
    if (host->IsDisplayed()) {
      // Make sure the GPU is never more than two animation frames behind.
      constexpr unsigned kMaxCanvasAnimationBacklog = 2;
      if (host->IncrementFramesSinceLastCommit() >=
          static_cast<int>(kMaxCanvasAnimationBacklog)) {
        if (host->IsComposited() && !host->RateLimiter()) {
          host->CreateRateLimiter();
        }
      }
    }

    if (host->RateLimiter()) {
      host->RateLimiter()->Tick();
    }
  }
}

CanvasRenderingContextHost*
CanvasRenderingContext2D::GetCanvasRenderingContextHost() const {
  return Host();
}

ExecutionContext* CanvasRenderingContext2D::GetTopExecutionContext() const {
  return Host()->GetTopExecutionContext();
}

Color CanvasRenderingContext2D::GetCurrentColor() const {
  const HTMLCanvasElement* const element = canvas();
  if (!element || !element->isConnected() || !element->InlineStyle()) {
    return Color::kBlack;
  }
  Color color = Color::kBlack;
  CSSParser::ParseColor(
      color, element->InlineStyle()->GetPropertyValue(CSSPropertyID::kColor));
  return color;
}

void CanvasRenderingContext2D::PageVisibilityChanged() {
  HTMLCanvasElement* const element = canvas();
  if (IsPaintable()) {
    element->GetCanvas2DLayerBridge()->PageVisibilityChanged();
  }
  if (!element->IsPageVisible()) {
    PruneLocalFontCache(0);
  }
}

cc::Layer* CanvasRenderingContext2D::CcLayer() const {
  if (!IsPaintable()) {
    return nullptr;
  }
  return canvas()->GetOrCreateCcLayerIfNeeded();
}

CanvasRenderingContext2DSettings*
CanvasRenderingContext2D::getContextAttributes() const {
  CanvasRenderingContext2DSettings* settings =
      CanvasRenderingContext2DSettings::Create();
  settings->setAlpha(CreationAttributes().alpha);
  settings->setColorSpace(color_params_.GetColorSpaceAsString());
  if (RuntimeEnabledFeatures::CanvasFloatingPointEnabled())
    settings->setPixelFormat(color_params_.GetPixelFormatAsString());
  settings->setDesynchronized(Host()->LowLatencyEnabled());
  switch (CreationAttributes().will_read_frequently) {
    case CanvasContextCreationAttributesCore::WillReadFrequently::kTrue:
      settings->setWillReadFrequently(V8CanvasWillReadFrequently::Enum::kTrue);
      break;
    case CanvasContextCreationAttributesCore::WillReadFrequently::kFalse:
      settings->setWillReadFrequently(V8CanvasWillReadFrequently::Enum::kFalse);
      break;
    case CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined:
      settings->setWillReadFrequently(
          V8CanvasWillReadFrequently::Enum::kUndefined);
  }
  return settings;
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element) {
  DrawFocusIfNeededInternal(GetPath(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D* path2d,
                                                 Element* element) {
  DrawFocusIfNeededInternal(path2d->GetPath(), element,
                            path2d->GetIdentifiableToken());
}

void CanvasRenderingContext2D::DrawFocusIfNeededInternal(
    const Path& path,
    Element* element,
    IdentifiableToken path_token) {
  if (!FocusRingCallIsValid(path, element))
    return;

  // Note: we need to check document->focusedElement() rather than just calling
  // element->focused(), because element->focused() isn't updated until after
  // focus events fire.
  if (element->GetDocument().FocusedElement() == element) {
    if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
      identifiability_study_helper_.UpdateBuilder(CanvasOps::kDrawFocusIfNeeded,
                                                  path_token);
    }
    ScrollPathIntoViewInternal(path);
    DrawFocusRing(path, element);
  }

  // Update its accessible bounds whether it's focused or not.
  UpdateElementAccessibility(path, element);
}

bool CanvasRenderingContext2D::FocusRingCallIsValid(const Path& path,
                                                    Element* element) {
  DCHECK(element);
  if (!IsTransformInvertible()) [[unlikely]] {
    return false;
  }
  if (path.IsEmpty())
    return false;
  if (!element->IsDescendantOf(canvas()))
    return false;

  return true;
}

void CanvasRenderingContext2D::DrawFocusRing(const Path& path,
                                             Element* element) {
  if (!GetOrCreatePaintCanvas())
    return;

  mojom::blink::ColorScheme color_scheme = mojom::blink::ColorScheme::kLight;
  if (element) {
    if (const ComputedStyle* style = element->GetComputedStyle())
      color_scheme = style->UsedColorScheme();
  }

  const SkColor4f color =
      LayoutTheme::GetTheme().FocusRingColor(color_scheme).toSkColor4f();
  const int kFocusRingWidth = 5;
  DrawPlatformFocusRing(path.GetSkPath(), GetPaintCanvas(), color,
                        /*width=*/kFocusRingWidth,
                        /*corner_radius=*/kFocusRingWidth);

  // We need to add focusRingWidth to dirtyRect.
  StrokeData stroke_data;
  stroke_data.SetThickness(kFocusRingWidth);

  SkIRect dirty_rect;
  if (!ComputeDirtyRect(path.StrokeBoundingRect(stroke_data), &dirty_rect))
    return;

  DidDraw(dirty_rect, CanvasPerformanceMonitor::DrawType::kPath);
}

void CanvasRenderingContext2D::UpdateElementAccessibility(const Path& path,
                                                          Element* element) {
  HTMLCanvasElement* const canvas_element = canvas();
  LayoutBoxModelObject* lbmo = canvas_element->GetLayoutBoxModelObject();
  LayoutObject* renderer = canvas_element->GetLayoutObject();
  if (!lbmo || !renderer) {
    return;
  }

  AXObjectCache* ax_object_cache =
      element->GetDocument().ExistingAXObjectCache();
  if (!ax_object_cache) {
    return;
  }
  ax_object_cache->UpdateAXForAllDocuments();

  // Get the transformed path.
  Path transformed_path = path;
  transformed_path.Transform(GetState().GetTransform());

  // Add border and padding to the bounding rect.
  PhysicalRect element_rect =
      PhysicalRect::EnclosingRect(transformed_path.BoundingRect());
  element_rect.Move({lbmo->BorderLeft() + lbmo->PaddingLeft(),
                     lbmo->BorderTop() + lbmo->PaddingTop()});

  // Update the accessible object.
  ax_object_cache->SetCanvasObjectBounds(canvas_element, element, element_rect);
}

// TODO(aaronhk) This is only used for the size heuristic. Delete this function
// once always accelerate fully lands.
void CanvasRenderingContext2D::DisableAcceleration() {
  canvas()->DisableAcceleration();
}

bool CanvasRenderingContext2D::ShouldDisableAccelerationBecauseOfReadback()
    const {
  return canvas()->ShouldDisableAccelerationBecauseOfReadback();
}

bool CanvasRenderingContext2D::IsCanvas2DBufferValid() const {
  if (IsPaintable()) {
    return canvas()->IsResourceValid();
  }
  return false;
}

void CanvasRenderingContext2D::ColorSchemeMayHaveChanged() {
  SetColorScheme(GetColorSchemeFromCanvas(canvas()));
}

RespectImageOrientationEnum CanvasRenderingContext2D::RespectImageOrientation()
    const {
  if (canvas()->RespectImageOrientation() != kRespectImageOrientation) {
    return kDoNotRespectImageOrientation;
  }
  return kRespectImageOrientation;
}

HTMLCanvasElement* CanvasRenderingContext2D::HostAsHTMLCanvasElement() const {
  return canvas();
}

FontSelector* CanvasRenderingContext2D::GetFontSelector() const {
  return canvas()->GetFontSelector();
}

int CanvasRenderingContext2D::LayerCount() const {
  return BaseRenderingContext2D::LayerCount();
}

}  // namespace blink
