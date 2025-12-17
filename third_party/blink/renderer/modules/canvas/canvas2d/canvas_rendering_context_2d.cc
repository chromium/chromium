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

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/values_equivalent.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/texture_layer.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "cc/layers/texture_layer_impl.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_rendering_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
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
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/platform_focus_ring.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/key_value_pair.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

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
class MemoryManagedPaintCanvas;
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

namespace {

}  // namespace

CanvasRenderingContext* CanvasRenderingContext2D::Factory::Create(
    ExecutionContext* execution_context,
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  DCHECK(!host->IsOffscreenCanvas());
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<CanvasRenderingContext2D>(
          static_cast<HTMLCanvasElement*>(host), attrs);
  DCHECK(rendering_context);
  UseCounter::CountWebDXFeature(execution_context, WebDXFeature::kCanvas2D);
  if (attrs.alpha) {
    UseCounter::CountWebDXFeature(execution_context,
                                  WebDXFeature::kCanvas2DAlpha);
  }
  if (attrs.desynchronized) {
    UseCounter::Count(execution_context,
                      WebFeature::kHTMLCanvasElementLowLatency_2D);
    UseCounter::CountWebDXFeature(execution_context,
                                  WebDXFeature::kCanvas2DDesynchronized);
  }
  if (attrs.will_read_frequently ==
      CanvasContextCreationAttributesCore::WillReadFrequently::kTrue) {
    UseCounter::CountWebDXFeature(execution_context,
                                  WebDXFeature::kCanvas2DWillreadfrequently);
  }
  if (attrs.color_space != PredefinedColorSpace::kSRGB) {
    UseCounter::Count(execution_context, WebFeature::kCanvasUseColorSpace);
  }
  return rendering_context;
}

CanvasRenderingContext2D::CanvasRenderingContext2D(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : BaseRenderingContext2D(
          canvas,
          attrs,
          canvas->GetDocument().GetTaskRunner(TaskType::kInternalDefault)),
      should_prune_local_font_cache_(false) {
  if (canvas->GetDocument().GetSettings() &&
      canvas->GetDocument().GetSettings()->GetAntialiasedClips2dCanvasEnabled())
    clip_antialiasing_ = kAntiAliased;
  SetShouldAntialias(true);
}

V8RenderingContext* CanvasRenderingContext2D::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

void CanvasRenderingContext2D::ResetInternal() {
  if (IsHibernating()) {
    CanvasHibernationHandler::ReportHibernationEvent(
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedOnReset);
    GetHibernationHandler()->Clear();
  }
  BaseRenderingContext2D::ResetInternal();
}

bool CanvasRenderingContext2D::IsComposited() const {
  // The following case is necessary for handling the special case of canvases
  // in the dev tools overlay.
  const HTMLCanvasElement* const element = canvas();
  auto* settings = element->GetDocument().GetSettings();
  if (settings && !settings->GetAcceleratedCompositingEnabled()) {
    return false;
  }
  if (IsHibernating()) {
    return false;
  }

  if (!resource_provider_) [[unlikely]] {
    return false;
  }

  return resource_provider_->SupportsDirectCompositing() &&
         !element->LowLatencyEnabled();
}

void CanvasRenderingContext2D::Stop() {
  // Never attempt to restore the context because the page is being torn down.
  context_restorable_ = false;
  if (isContextLost()) [[unlikely]] {
    // Stop any pending restoration.
    try_restore_context_event_timer_.Stop();
  } else {
    if (IsHibernating()) {
      CanvasHibernationHandler::ReportHibernationEvent(
          CanvasHibernationHandler::HibernationEvent::
              kHibernationEndedWithTeardown);
      GetHibernationHandler()->Clear();
    }
    LoseContext(kCanvasDisposed);
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
    resource_provider_ = nullptr;
    element->DiscardResources();
    element->DiscardResourceDispatcher();

    if (element->IsPageVisible()) {
      dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(),
                                                      FROM_HERE);
      return;
    }
  }
  needs_context_lost_event_ = true;
}

void CanvasRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(filter_operations_);
  ScriptWrappable::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
  SVGResourceClient::Trace(visitor);
}

void CanvasRenderingContext2D::WillDrawImage(CanvasImageSource* source,
                                             bool image_is_texture_backed) {
  // For images coming from canvases, use the image itself as the source of
  // truth for whether the canvas is accelerated, as
  // CanvasRenderingContextHost::IsAccelerated() is canvas2d-specific.
  bool source_is_accelerated =
      (source->IsCanvasElement() || source->IsOffscreenCanvas())
          ? image_is_texture_backed
          : source->IsAccelerated();
  // If the source is GPU-accelerated, and the canvas is not, but could be...
  if (source_is_accelerated && canvas()->ShouldAccelerate2dContext() &&
      canvas()->GetRasterModeForCanvas2D() == RasterMode::kCPU &&
      SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade()) {
    // Recreate the CRP in GPU raster mode and signal that it needs a
    // compositing update.
    canvas()->SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
    DropAndRecreateExistingResourceProvider();
    canvas()->SetNeedsCompositingUpdate();
  }
}

bool CanvasRenderingContext2D::WritePixels(const SkImageInfo& orig_info,
                                           const void* pixels,
                                           size_t row_bytes,
                                           int x,
                                           int y) {
  if (!resource_provider_ || !canvas() || isContextLost() ||
      !resource_provider_->IsValid()) {
    return false;
  }

  CanvasRenderingContextHost* host = Host();
  CanvasResourceProvider* provider = resource_provider_.get();

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
    provider->FlushCanvas();

    // Short-circuit out if an error occurred while flushing the recording.
    if (!provider->IsValid()) {
      return false;
    }
  }

  return provider->WritePixels(orig_info, pixels, row_bytes, x, y);
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
  const AffineTransform& transform = GetState().GetTransform();
  const Path transformed_path =
      transform.IsIdentity()
          ? path
          : PathBuilder(path).Transform(transform).Finalize();
  const gfx::RectF bounding_rect = transformed_path.BoundingRect();

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

sk_sp<PaintFilter> CanvasRenderingContext2D::StateGetFilter() {
  HTMLCanvasElement* const element = canvas();
  return GetState().GetFilter(element, element->Size(), this);
}

MemoryManagedPaintCanvas* CanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (isContextLost()) [[unlikely]] {
    return nullptr;
  }

  CanvasResourceProvider* provider = GetResourceProvider();
  if (provider != nullptr) [[likely]] {
    // If we already had a provider, we can check whether it recorded ops passed
    // the autoflush limit.
    if (layer_count_ == 0) [[likely]] {
      // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
      provider->FlushIfRecordingLimitExceeded();
    }
  } else {
    // If we have no provider, try creating one.
    provider = GetOrCreateResourceProvider();
    if (provider == nullptr) [[unlikely]] {
      return nullptr;
    }
  }

  return &provider->Recorder().getRecordingCanvas();
}

const MemoryManagedPaintCanvas* CanvasRenderingContext2D::GetPaintCanvas()
    const {
  if (isContextLost()) [[unlikely]] {
    return nullptr;
  }
  const CanvasResourceProvider* provider = GetResourceProvider();
  if (!provider) [[unlikely]] {
    return nullptr;
  }
  return &provider->Recorder().getRecordingCanvas();
}

const MemoryManagedPaintRecorder* CanvasRenderingContext2D::Recorder() const {
  const CanvasResourceProvider* provider = GetResourceProvider();
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

  if (!canvas()) {
    return;
  }

  // Always draw everything during printing.
  if (CanvasResourceProvider* provider = GetResourceProvider();
      layer_count_ == 0 && provider != nullptr) [[likely]] {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    provider->FlushIfRecordingLimitExceeded();
  }
}

std::optional<cc::PaintRecord> CanvasRenderingContext2D::FlushCanvas(
    FlushReason reason) {
  CanvasResourceProvider* provider = GetResourceProvider();
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
  return BaseRenderingContext2D::CurrentFontResolvedAndUpToDate() &&
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
  const LayoutLocale* locale = LocaleFromLang();

  // Map the <canvas> font into the text style. If the font uses keywords like
  // larger/smaller, these will work relative to the canvas.
  const ComputedStyle* computed_style = element->EnsureComputedStyle();
  if (computed_style) {
    auto i = fonts_resolved_using_current_style_.find(new_font);
    if (i != fonts_resolved_using_current_style_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(!add_result.is_new_entry);
      if (i->value.Locale() != locale) {
        i->value.SetLocale(locale);
      }
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
      element_font_description.SetLocale(locale);
      // Reset the computed size to avoid inheriting the zoom factor from the
      // <canvas> element.
      element_font_description.SetComputedSize(
          element_font_description.SpecifiedSize());
      element_font_description.SetAdjustedSize(
          element_font_description.SpecifiedSize());

      font_style_builder.SetFontDescription(element_font_description);
      const ComputedStyle* font_style = font_style_builder.TakeStyle();
      const Font* font = document.GetStyleEngine().ComputeFont(
          *element, *font_style, *parsed_style);

      // We need to reset Computed and Adjusted size so we skip zoom and
      // minimum font size.
      FontDescription final_description(font->GetFontDescription());
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
    const Font* resolved_font =
        canvas_font_cache->GetFontUsingDefaultStyle(*element, new_font);
    if (!resolved_font) {
      return false;
    }

    // We need to reset Computed and Adjusted size so we skip zoom and
    // minimum font size for detached canvas.
    FontDescription final_description(resolved_font->GetFontDescription());
    final_description.SetLocale(locale);
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
  if (old_style &&
      (base::FeatureList::IsEnabled(blink::features::kCSSFontComparisonFix)
           ? base::ValuesEquivalent(old_style->GetFont(), new_style.GetFont())
           : old_style->GetFont() == new_style.GetFont())) {
    return;
  }
  PruneLocalFontCache(0);
}

void CanvasRenderingContext2D::LangAttributeChanged() {
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLang() == kInheritString) {
    PruneLocalFontCache(0);
    if (state.HasRealizedFont()) {
      setFont(font());
    }
  }
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

scoped_refptr<CanvasResource>
CanvasRenderingContext2D::PaintRenderingResultsToResource(
    SourceDrawingBuffer source_buffer,
    FlushReason reason) {
  if (!IsResourceProviderValid()) {
    return nullptr;
  }

  // Only CRPSI can produce CanvasResources.
  auto* si_provider = resource_provider_->AsSharedImageProvider();
  if (!si_provider) {
    return nullptr;
  }

  return si_provider->ProduceCanvasResource(reason);
}

const std::optional<cc::PaintRecord>&
CanvasRenderingContext2D::GetLastRecordingForCanvas2D() {
  auto* provider = GetResourceProvider();
  if (!provider) {
    return empty_recording_;
  }
  return provider->LastRecording();
}

bool CanvasRenderingContext2D::CanCreateResourceProvider() {
  return GetOrCreateResourceProvider();
}

scoped_refptr<StaticBitmapImage> blink::CanvasRenderingContext2D::GetImage() {
  if (IsHibernating()) {
    return UnacceleratedStaticBitmapImage::Create(
        GetHibernationHandler()->GetImage());
  }

  if (!IsResourceProviderValid()) {
    return nullptr;
  }

  resource_provider_->FlushCanvas();
  return resource_provider_->Snapshot();
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
  TRACE_EVENT0("blink", "GetImageData");
  return BaseRenderingContext2D::getImageDataInternal(
      sx, sy, sw, sh, image_data_settings, exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElement(
    Element* element,
    double dx,
    double dy,
    ExceptionState& exception_state) {
  return DrawElementInternal(
      element,
      /*sx*/ std::nullopt, /*sy*/ std::nullopt,
      /*swidth*/ std::nullopt, /*sheight*/ std::nullopt, dx, dy,
      /*dwidth*/ std::nullopt, /*dheight*/ std::nullopt, exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElement(
    Element* element,
    double dx,
    double dy,
    double dwidth,
    double dheight,
    ExceptionState& exception_state) {
  return DrawElementInternal(element,
                             /*sx*/ std::nullopt, /*sy*/ std::nullopt,
                             /*swidth*/ std::nullopt, /*sheight*/ std::nullopt,
                             dx, dy, dwidth, dheight, exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElementImage(
    Element* element,
    double dx,
    double dy,
    ExceptionState& exception_state) {
  return DrawElementInternal(
      element,
      /*sx*/ std::nullopt, /*sy*/ std::nullopt,
      /*swidth*/ std::nullopt, /*sheight*/ std::nullopt, dx, dy,
      /*dwidth*/ std::nullopt, /*dheight*/ std::nullopt, exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElementImage(
    Element* element,
    double dx,
    double dy,
    double dwidth,
    double dheight,
    ExceptionState& exception_state) {
  return DrawElementInternal(element,
                             /*sx*/ std::nullopt, /*sy*/ std::nullopt,
                             /*swidth*/ std::nullopt, /*sheight*/ std::nullopt,
                             dx, dy, dwidth, dheight, exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElementImage(
    Element* element,
    double sx,
    double sy,
    double swidth,
    double sheight,
    double dx,
    double dy,
    ExceptionState& exception_state) {
  return DrawElementInternal(element, sx, sy, swidth, sheight, dx, dy,
                             /*dwidth*/ std::nullopt, /*dheight*/ std::nullopt,
                             exception_state);
}

DOMMatrix* CanvasRenderingContext2D::drawElementImage(
    Element* element,
    double sx,
    double sy,
    double swidth,
    double sheight,
    double dx,
    double dy,
    double dwidth,
    double dheight,
    ExceptionState& exception_state) {
  return DrawElementInternal(element, sx, sy, swidth, sheight, dx, dy, dwidth,
                             dheight, exception_state);
}

void CanvasRenderingContext2D::EnableAccelerationIfPossible() {
  if (canvas()->GetRasterModeForCanvas2D() == RasterMode::kCPU &&
      SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade()) {
    canvas()->SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
    DropAndRecreateExistingResourceProvider();
  }
}

DOMMatrix* CanvasRenderingContext2D::DrawElementInternal(
    Element* element,
    std::optional<double> sx,
    std::optional<double> sy,
    std::optional<double> swidth,
    std::optional<double> sheight,
    double x,
    double y,
    std::optional<double> dwidth,
    std::optional<double> dheight,
    ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::CanvasDrawElementEnabled());

  if (!GetOrCreatePaintCanvas()) {
    return nullptr;
  }

  TRACE_EVENT0("blink", "DrawElementImage");

  element->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kCanvasDrawElementImage);

  // Element size in physical coordinates.
  gfx::SizeF box_size;
  if (element->GetLayoutBox()) {
    box_size = gfx::SizeF(element->GetLayoutBox()->StitchedSize());
  }
  gfx::RectF src_rect(box_size);
  std::optional<CullRect> cull_rect;
  if (sx && sy && swidth && sheight) {
    float dpr = element->ComputedStyleRef().EffectiveZoom();
    src_rect = gfx::RectF(*sx * dpr, *sy * dpr, *swidth * dpr, *sheight * dpr);
    cull_rect.emplace(gfx::ToEnclosingRect(src_rect));
  }

  std::optional<cc::PaintRecord> paint_record = GetElementPaintRecord(
      element, cull_rect, "drawElementImage()", exception_state);
  if (!paint_record) {
    return nullptr;
  }

  // The filter needs to be resolved before calling Draw, because it
  // immediately checks IsFilterResolved() and uses a null canvas if not.
  StateGetFilter();

  // The ideal size is the source content size, represented in canvas grid
  // coordinates. This will cause the element to have the same proportions when
  // appearing inside the canvas as it would have were it painted outside the
  // canvas.
  gfx::SizeF ideal_dst_size(src_rect.size());
  gfx::Vector2dF scale_factor =
      canvas()->PhysicalPixelToCanvasGridScaleFactor();
  ideal_dst_size.Scale(scale_factor.x(), scale_factor.y());

  gfx::RectF dst_rect(x, y, 0, 0);
  if (dwidth && dheight) {
    dst_rect.set_size(gfx::SizeF(*dwidth, *dheight));
  } else {
    // If no explicit destination size is given, default to the ideal size.
    dst_rect.set_size(ideal_dst_size);
  }

  // TODO(crbug.com/421834883): This code is based on image drawing. Maybe we
  // need a distinct paint_type: kImagePaintType seems to do the right thing
  // but maybe its treatment of anti-aliasing is incorrect. The kNonOpaqueImage
  // type controls drop shadow painting under transforms. It's not clear if we
  // should behave like a non-opaque image here, but the element may not be
  // opaque so going with that for now.
  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [paint_record, dst_rect, src_rect](MemoryManagedPaintCanvas* c,
                                         const cc::PaintFlags* flags) {
        cc::RecordPaintCanvas::DisableFlushCheckScope disable_flush_check_scope(
            static_cast<cc::RecordPaintCanvas*>(c));
        int initial_save_count = c->getSaveCount();

        if (flags->getImageFilter() ||
            flags->getBlendMode() != SkBlendMode::kSrcOver ||
            SkColorGetA(flags->getColor()) < 255) {
          SkM44 ctm = c->getLocalToDevice();
          SkM44 inv_ctm;
          if (!ctm.invert(&inv_ctm)) {
            // There is an earlier check for invertibility, but the arithmetic
            // in AffineTransform is not exactly identical, so it is possible
            // for SkMatrix to find the transform to be non-invertible at this
            // stage. crbug.com/504687
            return;
          }
          SkRect bounds = gfx::RectFToSkRect(dst_rect);
          ctm.asM33().mapRect(&bounds);
          if (!bounds.isFinite()) {
            // There is an earlier check for the correctness of the bounds, but
            // it is possible that after applying the matrix transformation we
            // get a faulty set of bounds, so we want to catch this asap and
            // avoid sending a draw command. crbug.com/1039125 We want to do
            // this before the save command is sent.
            return;
          }
          c->save();
          c->concat(inv_ctm);

          cc::PaintFlags layer_flags;
          layer_flags.setBlendMode(flags->getBlendMode());
          layer_flags.setImageFilter(flags->getImageFilter());
          layer_flags.setColor(flags->getColor());

          c->saveLayer(bounds, layer_flags);
          c->concat(ctm);
        }

        c->save();
        c->translate(dst_rect.x(), dst_rect.y());
        c->scale(dst_rect.width() / src_rect.width(),
                 dst_rect.height() / src_rect.height());
        c->translate(-src_rect.x(), -src_rect.y());

        c->clipRect(SkRect::MakeXYWH(src_rect.x(), src_rect.y(),
                                     src_rect.width(), src_rect.height()));

        c->drawPicture(paint_record.value(),
                       // use a save at the beginning of the record to keep
                       // transforms local:
                       true);

        c->restoreToCount(initial_save_count);
      },
      NoOverdraw, /*bounds=*/gfx::RectF(src_rect.width(), src_rect.height()),
      CanvasRenderingContext2DState::kImagePaintType,
      CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kElement);

  // Compute the transform, in canvas grid coordinates, that we just drew with.
  // We start from the context's CTM, then offset by x,y, and finally apply any
  // dest scaling.
  gfx::Transform draw_transform = GetState().GetTransform().ToTransform();
  draw_transform.Translate(x, y);
  // The drawing commands above scale by `dst_rect.size() / src_rect.size()`,
  // which does two things: 1) scales the drawing commands of `paint_record` (in
  // physical pixels) to canvas grid coordinates, and 2) applies any additional
  // dest scaling. We are only returning #2 in the logic below.
  draw_transform.Scale(dst_rect.width() / ideal_dst_size.width(),
                       dst_rect.height() / ideal_dst_size.height());

  // This call will take our draw transform in canvas grid coordinates, and
  // convert it to a transform in CSS pixels suitable for positioning the
  // element.
  DOMMatrix* draw_matrix = MakeGarbageCollected<DOMMatrix>(draw_transform);
  return canvas()->getElementTransform(element, draw_matrix, exception_state);
}

void CanvasRenderingContext2D::PreFinalizeFrame() {
  // Low-latency 2d canvases produce their frames after the resource gets single
  // buffered.
  // TODO(crbug.com/40280152): Analyze whether this call is redundant (i.e.,
  // whether the CRP is guaranteed to always be present).
  if (canvas() && canvas()->LowLatencyEnabled() && canvas()->IsDirty()) {
    GetOrCreateResourceProvider();
  }
}

void CanvasRenderingContext2D::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "CanvasRenderingContext2D::FinalizeFrame");
  if (!IsPaintable()) {
    return;
  }

  HTMLCanvasElement* host = canvas();
  CHECK(host);

  GetResourceProvider()->FlushCanvas(reason);
  if (reason == FlushReason::kCanvasPushFrame) {
    if (host->IsDisplayed()) {
      // Make sure the GPU is never more than two animation frames behind.
      constexpr unsigned kMaxCanvasAnimationBacklog = 2;
      if (host->IncrementFramesSinceLastCommit() >=
          static_cast<int>(kMaxCanvasAnimationBacklog)) {
        if (IsComposited() && !host->RateLimiter()) {
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

bool CanvasRenderingContext2D::IsPaintable() const {
  return GetResourceProvider();
}

bool CanvasRenderingContext2D::IsHibernating() const {
  auto* hibernation_handler = GetHibernationHandler();
  return hibernation_handler && hibernation_handler->IsHibernating();
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

  bool page_is_visible = element->IsPageVisible();

  // If the canvas is backed by a SharedImage resource provider, toggle
  // whether resource recycling is enabled based on page visibility.
  auto* resource_provider = GetResourceProvider();
  auto* resource_provider_si =
      resource_provider ? resource_provider->AsSharedImageProvider() : nullptr;
  if (resource_provider_si) {
    resource_provider_si->SetResourceRecyclingEnabled(page_is_visible);
  }

  // Conserve memory.
  SetAggressivelyFreeSharedGpuContextResourcesIfPossible(!page_is_visible);

  if (features::IsCanvas2DHibernationEnabled() && !page_is_visible &&
      !IsHibernating() && resource_provider &&
      resource_provider->IsAccelerated()) {
    // Assuming 8-bit RGBA or similar, this means that we don't bother
    // hibernating canvas elements smaller than 64kiB. Hibernation has a cost,
    // and a lot of pages have very small canvas elements, according to metrics.
    if (!(base::FeatureList::IsEnabled(
              features::kCanvas2DHibernationNoSmallCanvas) &&
          Height() * Width() < 128 * 128)) {
      GetHibernationHandler()->InitiateHibernationIfNecessary();
    }
  }

  // The impl tree may have dropped the transferable resource for this canvas
  // while it wasn't visible. Make sure that it gets pushed there again, now
  // that we've visible.
  //
  // This is done all the time, but it is especially important when canvas
  // hibernation is disabled. In this case, when the impl-side active tree
  // releases the TextureLayer's transferable resource, it will not be freed
  // since the texture has not been cleared above (there is a remaining
  // reference held from the TextureLayer). Then the next time the page becomes
  // visible, the TextureLayer will note the resource hasn't changed (in
  // Update()), and will not add the layer to the list of those that need to
  // push properties. But since the impl-side tree no longer holds the resource,
  // we need TreeSynchronizer to always consider this layer.
  //
  // This makes sure that we do push properties. It is not needed when canvas
  // hibernation is enabled (since the resource will have changed, it will be
  // pushed), but we do it anyway, since these interactions are subtle.
  bool resource_may_have_been_dropped =
      cc::TextureLayerImpl::MayEvictResourceInBackground(
          viz::TransferableResource::ResourceSource::kCanvas);
  if (page_is_visible && resource_may_have_been_dropped) {
    element->SetNeedsPushProperties();
  }

  if (page_is_visible && IsHibernating()) {
    GetOrCreateResourceProvider();  // Rude awakening
  }

  if (!element->IsPageVisible()) {
    PruneLocalFontCache(0);
  }
}

cc::Layer* CanvasRenderingContext2D::CcLayer() const {
  return canvas() ? canvas()->GetOrCreateCcLayerForCanvas2DIfNeeded() : nullptr;
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element) {
  DrawFocusIfNeededInternal(GetPath(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D* path2d,
                                                 Element* element) {
  DrawFocusIfNeededInternal(path2d->GetPath(), element);
}

void CanvasRenderingContext2D::DrawFocusIfNeededInternal(const Path& path,
                                                         Element* element) {
  if (!FocusRingCallIsValid(path, element))
    return;

  // Note: we need to check document->focusedElement() rather than just calling
  // element->focused(), because element->focused() isn't updated until after
  // focus events fire.
  if (element->GetDocument().FocusedElement() == element) {
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
  const AffineTransform& transform = GetState().GetTransform();
  const Path transformed_path =
      transform.IsIdentity()
          ? path
          : PathBuilder(path).Transform(transform).Finalize();

  // Add border and padding to the bounding rect.
  PhysicalRect element_rect =
      PhysicalRect::EnclosingRect(transformed_path.BoundingRect());
  element_rect.Move({lbmo->BorderLeft() + lbmo->PaddingLeft(),
                     lbmo->BorderTop() + lbmo->PaddingTop()});

  // Update the accessible object.
  ax_object_cache->SetCanvasObjectBounds(canvas_element, element, element_rect);
}

void CanvasRenderingContext2D::DisableAcceleration() {
  canvas()->OnAccelerationDisabled();

  // Create and configure an unaccelerated CanvasResourceProvider.
  canvas()->SetPreferred2DRasterMode(RasterModeHint::kPreferCPU);

  DropAndRecreateExistingResourceProvider();

  // We must force a paint invalidation on the canvas even if its
  // content did not change, because its layer was destroyed.
  canvas()->DidDraw();
  canvas()->SetNeedsCompositingUpdate();
}

bool CanvasRenderingContext2D::ShouldDisableAccelerationBecauseOfReadback()
    const {
  return canvas()->ShouldDisableAccelerationBecauseOfReadback();
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

UniqueFontSelector* CanvasRenderingContext2D::GetFontSelector() const {
  return canvas()->GetFontSelector();
}

void CanvasRenderingContext2D::SizeChanged() {
  resource_provider_ = nullptr;
  did_fail_to_create_resource_provider_ = false;
}

CanvasHibernationHandler* CanvasRenderingContext2D::GetHibernationHandler()
    const {
  return hibernation_handler_.get();
}

void CanvasRenderingContext2D::Dispose() {
  hibernation_handler_ = nullptr;
  resource_provider_ = nullptr;
  CanvasRenderingContext::Dispose();
}

std::unique_ptr<CanvasResourceProvider>
CanvasRenderingContext2D::CreateCanvasResourceProvider() {
  CHECK(!GetResourceProvider());

  base::WeakPtr<CanvasResourceDispatcher> dispatcher =
      canvas()->GetOrCreateResourceDispatcher()
          ? canvas()->GetOrCreateResourceDispatcher()->GetWeakPtr()
          : nullptr;

  std::unique_ptr<CanvasResourceProvider> provider;
  const SkAlphaType alpha_type = GetAlphaType();
  const viz::SharedImageFormat format = GetSharedImageFormat();
  const gfx::ColorSpace color_space = GetColorSpace();
  const bool use_gpu = canvas()->ShouldTryToUseGpuRaster() &&
                       canvas()->ShouldAccelerate2dContext();
  constexpr auto kShouldInitialize =
      CanvasResourceProvider::ShouldInitialize::kCallClear;
  if (use_gpu && canvas()->LowLatencyEnabled()) {
    // Try a SharedImage provider with usage optimized for low-latency.
    gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    bool can_use_swapchain = SharedGpuContext::ContextProviderWrapper()
                                 ->ContextProvider()
                                 .SharedImageInterface()
                                 ->GetCapabilities()
                                 .shared_image_swap_chain;
    bool can_use_concurrent_read_write =
        can_use_swapchain ||
        (SharedGpuContext::MaySupportImageChromium() &&
         (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled() ||
          base::FeatureList::IsEnabled(
              features::kLowLatencyCanvas2dImageChromium)));
    if (can_use_concurrent_read_write) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
    }
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        canvas()->Size(), format, alpha_type, color_space, kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        shared_image_usage_flags, canvas());
  } else if (use_gpu) {
    // First try to be optimized for displaying on screen. In the case we are
    // hardware compositing, we also try to enable the usage of the image as
    // scanout buffer (overlay)
    gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (SharedGpuContext::MaySupportImageChromium() &&
        RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        canvas()->Size(), format, alpha_type, color_space, kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        shared_image_usage_flags, canvas());
  } else if (SharedGpuContext::MaySupportImageChromium() &&
             RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
    // In this case, we are using CPU raster and GPU compositing and native
    // mappable buffers are supported. Try to use a
    // CanvasResourceProviderSharedImage, which if successful will result in
    // using a SharedImage that can be mapped onto the CPU for software raster
    // writes and then read by the display compositor (and potentially used as
    // an overlay).
    const gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        canvas()->Size(), format, alpha_type, color_space, kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kCPU,
        shared_image_usage_flags, canvas());
  }

  // If either of the other modes failed and / or it was not possible to do, we
  // will backup with a software SharedImage, and if that was not possible with
  // a Bitmap provider.
  if (!provider && !SharedGpuContext::IsGpuCompositingEnabled()) {
    // In this case, we are using CPU raster and CPU compositing. Create a
    // CanvasResourceProvider that uses a SharedImage backed by a shared-memory
    // buffer that can be written by canvas raster and read by the compositor.
    provider =
        CanvasResourceProvider::CreateSharedImageProviderForSoftwareCompositor(
            canvas()->Size(), format, alpha_type, color_space,
            kShouldInitialize, SharedGpuContext::SharedImageInterfaceProvider(),
            canvas());
  }
  if (!provider) {
    // The final fallback is to raster into a bitmap that will then either be
    // uploaded into GPU memory (for GPU compositing) or copied into the Viz
    // process (for software compositing).
    provider = Canvas2DResourceProviderBitmap::Create(
        canvas()->Size(), format, alpha_type, color_space, kShouldInitialize,
        canvas());
  }

  return provider;
}

CanvasResourceProvider* CanvasRenderingContext2D::GetResourceProvider() const {
  if (!canvas()) {
    return nullptr;
  }
  return resource_provider_.get();
}

CanvasResourceProvider*
CanvasRenderingContext2D::GetOrCreateResourceProvider() {
  HTMLCanvasElement* const element = canvas();
  if (!element) [[unlikely]] {
    return nullptr;
  }

  CanvasResourceProvider* resource_provider = GetResourceProvider();
  if (isContextLost() && !IsContextBeingRestored()) {
    DCHECK(!resource_provider);
    return nullptr;
  }

  if (resource_provider) {
    if (!resource_provider->IsValid()) {
      // The canvas context is not lost but the provider is invalid. This
      // happens if the GPU process dies in the middle of a render task. The
      // canvas is notified of GPU context losses via the
      // `NotifyGpuContextLost` callback and restoration happens in
      // `TryRestoreContextEvent`. Both callbacks are executed in their own
      // separate task. If the GPU context goes invalid in the middle of a
      // render task, the canvas won't immediately know about it and canvas
      // APIs will continue using the provider that is now invalid. We can
      // early return here, trying to re-create the provider right away would
      // just fail. We need to let `TryRestoreContextEvent` wait for the GPU
      // process to up again.
      return nullptr;
    }
    return resource_provider;
  }

  if (did_fail_to_create_resource_provider_) {
    return nullptr;
  }

  if (!canvas()->IsValidImageSize()) {
    did_fail_to_create_resource_provider_ = true;
    if (!canvas()->Size().IsEmpty()) {
      LoseContext(CanvasRenderingContext::kInvalidCanvasSize);
    }
    return nullptr;
  }

  canvas()->UpdatePreferred2DRasterMode();

  if (!GetHibernationHandler()) {
    hibernation_handler_ = std::make_unique<CanvasHibernationHandler>(*this);
  }

  RecreateResourceProvider();

  canvas()->UpdateMemoryUsage();

  canvas()->SetNeedsCompositingUpdate();

  return resource_provider_.get();
}

std::unique_ptr<CanvasResourceProvider>
CanvasRenderingContext2D::ReplaceResourceProvider(
    std::unique_ptr<CanvasResourceProvider> provider) {
  std::unique_ptr<CanvasResourceProvider> old_resource_provider =
      std::move(resource_provider_);
  resource_provider_ = std::move(provider);
  canvas()->UpdateMemoryUsage();
  if (old_resource_provider) {
    old_resource_provider->SetDelegate(nullptr);
  }
  return old_resource_provider;
}

void CanvasRenderingContext2D::DropAndRecreateExistingResourceProvider() {
  CanvasResourceProvider* old_provider = GetResourceProvider();
  if (old_provider == nullptr) {
    return;
  }

  scoped_refptr<StaticBitmapImage> image = GetImage();
  // image can be null if allocation failed in which case we should just
  // abort the provider switch to retain the old provider, which is still
  // functional.
  if (!image) {
    return;
  }
  std::unique_ptr<MemoryManagedPaintRecorder> recorder =
      old_provider->ReleaseRecorder();
  canvas()->ResetLayer();
  ReplaceResourceProvider(nullptr);

  // Bail out if the context is lost.
  if (isContextLost() && !IsContextBeingRestored()) {
    return;
  }

  // Bail out if it's not possible to create a new provider.
  RecreateResourceProvider();
  if (!resource_provider_) {
    return;
  }

  resource_provider_->RestoreBackBuffer(image->PaintImageForCurrentFrame());
  resource_provider_->SetRecorder(std::move(recorder));

  canvas()->UpdateMemoryUsage();
}

void CanvasRenderingContext2D::RecreateResourceProvider() {
  CHECK(GetHibernationHandler());
  CHECK(!resource_provider_);

  if (did_fail_to_create_resource_provider_) {
    return;
  }

  if (canvas()->IsValidImageSize()) {
    resource_provider_ = CreateCanvasResourceProvider();
    canvas()->UpdateMemoryUsage();
  }
  if (!resource_provider_) {
    did_fail_to_create_resource_provider_ = true;
    return;
  }

  CHECK(resource_provider_->IsValid());
  base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                            resource_provider_->IsAccelerated());
  base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                resource_provider_->GetType());

  if (GetHibernationHandler()->IsHibernating()) {
    WakeUpFromHibernation();
  }
}

void CanvasRenderingContext2D::WakeUpFromHibernation() {
  TRACE_EVENT0("base", "Canvas2dWakeUpFromHibernation");

  if (!canvas()->IsPageVisible()) {
    CanvasHibernationHandler::ReportHibernationEvent(
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithSwitchToBackgroundRendering);
  } else {
    if (resource_provider_->IsAccelerated()) {
      CanvasHibernationHandler::ReportHibernationEvent(
          CanvasHibernationHandler::HibernationEvent::
              kHibernationEndedNormally);
    } else {
      CanvasHibernationHandler::ReportHibernationEvent(
          CanvasHibernationHandler::HibernationEvent::
              kHibernationEndedWithFallbackToSW);
    }
  }

  CanvasHibernationHandler* hibernation_handler = GetHibernationHandler();
  PaintImageBuilder builder = PaintImageBuilder::WithDefault();
  builder.set_image(hibernation_handler->GetImage(),
                    PaintImage::GetNextContentId());
  builder.set_id(PaintImage::GetNextId());
  resource_provider_->RestoreBackBuffer(builder.TakePaintImage());
  resource_provider_->SetRecorder(hibernation_handler->ReleaseRecorder());
  // The hibernation image is no longer valid, clear it.
  hibernation_handler->Clear();
  DCHECK(!hibernation_handler->IsHibernating());

  // shouldBeDirectComposited() may have changed.
  canvas()->SetNeedsCompositingUpdate();
}

void CanvasRenderingContext2D::SetCanvas2DResourceProviderForTesting(
    std::unique_ptr<CanvasResourceProvider> provider,
    const gfx::Size& size) {
  canvas()->DiscardResources();
  canvas()->SetSize(size);
  hibernation_handler_ = std::make_unique<CanvasHibernationHandler>(*this);
  ReplaceResourceProvider(std::move(provider));
}

}  // namespace blink
