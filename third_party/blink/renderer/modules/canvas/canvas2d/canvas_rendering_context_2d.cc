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
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/common/trace_event_common.h"
#include "cc/layers/texture_layer.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "cc/layers/texture_layer_impl.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_record.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
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
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
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
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/platform_focus_ring.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
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

namespace {

}  // namespace

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
    : BaseRenderingContext2D(
          canvas,
          attrs,
          canvas->GetDocument().GetTaskRunner(TaskType::kInternalDefault)),
      should_prune_local_font_cache_(false) {
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
  // Never attempt to restore the context because the page is being torn down.
  context_restorable_ = false;
  if (isContextLost()) [[unlikely]] {
    // Stop any pending restoration.
    try_restore_context_event_timer_.Stop();
  } else {
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
    element->DiscardResourceProvider();
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

void CanvasRenderingContext2D::WillDrawImage(CanvasImageSource* source) const {
  canvas()->WillDrawImageTo2DContext(source);
}

bool CanvasRenderingContext2D::WritePixels(const SkImageInfo& orig_info,
                                           const void* pixels,
                                           size_t row_bytes,
                                           int x,
                                           int y) {
  DCHECK(IsCanvas2DBufferValid());
  CanvasRenderingContextHost* host = Host();
  CHECK(host);

  CanvasResourceProvider* provider =
      canvas()->GetOrCreateCanvasResourceProvider();
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

cc::PaintCanvas* CanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (isContextLost()) [[unlikely]] {
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
    provider = canvas()->GetOrCreateCanvasResourceProvider();
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
  bool use_locale = RuntimeEnabledFeatures::CanvasTextLangEnabled();
  const LayoutLocale* locale = use_locale ? LocaleFromLang() : nullptr;

  // Map the <canvas> font into the text style. If the font uses keywords like
  // larger/smaller, these will work relative to the canvas.
  const ComputedStyle* computed_style = element->EnsureComputedStyle();
  if (computed_style) {
    auto i = fonts_resolved_using_current_style_.find(new_font);
    if (i != fonts_resolved_using_current_style_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(!add_result.is_new_entry);
      if (use_locale && i->value.Locale() != locale) {
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
      if (use_locale) {
        element_font_description.SetLocale(locale);
      }
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
    if (use_locale) {
      final_description.SetLocale(locale);
    }
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
  if (old_style && old_style->GetFont() == new_style.GetFont()) {
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

bool CanvasRenderingContext2D::CanCreateCanvas2dResourceProvider() const {
  return canvas()->GetOrCreateCanvasResourceProvider();
}

scoped_refptr<StaticBitmapImage> blink::CanvasRenderingContext2D::GetImage(
    FlushReason reason) {
  if (CheckProviderInCanvas2DRenderingContextIsPaintable()) {
    // We can get an image if either (a) there is a ResourceProvider or (b) the
    // canvas is hibernating (in which case there will be no resource provider
    // but we can get a snapshot from the hibernation handler).
    bool is_hibernating = canvas() && canvas()->IsHibernating();
    if (!IsPaintable() && !is_hibernating) {
      return nullptr;
    }
  } else {
    if (!IsPaintable()) {
      return nullptr;
    }
  }

  if (canvas()->IsHibernating()) {
    return UnacceleratedStaticBitmapImage::Create(
        canvas()->GetHibernationHandler()->GetImage());
  }

  if (!canvas()->IsResourceValid()) {
    return nullptr;
  }
  // GetOrCreateResourceProvider needs to be called before FlushRecording, to
  // make sure "hint" is properly taken into account.
  if (!Host()->GetOrCreateCanvasResourceProvider()) {
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

void CanvasRenderingContext2D::drawElement(Element* element,
                                           double x,
                                           double y,
                                           ExceptionState& exception_state) {
  DrawElementInternal(element, x, y, std::nullopt, std::nullopt,
                      exception_state);
}

void CanvasRenderingContext2D::drawElement(Element* element,
                                           double x,
                                           double y,
                                           double dwidth,
                                           double dheight,
                                           ExceptionState& exception_state) {
  DrawElementInternal(element, x, y, dwidth, dheight, exception_state);
}

void CanvasRenderingContext2D::DrawElementInternal(
    Element* element,
    double x,
    double y,
    std::optional<double> dwidth,
    std::optional<double> dheight,
    ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::CanvasDrawElementEnabled());

  HTMLCanvasElement* canvas_element = HostAsHTMLCanvasElement();
  DCHECK(canvas_element);
  canvas_element->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kCanvasDrawElement);

  if (!IsDrawElementEligible(element, exception_state)) {
    return;
  }

  // TODO(crbug.com/380277045): Taint for cross-origin and PII content.
  // SetOriginTaintedByContent();

  PaintRecordBuilder builder;
  LayoutBox* layout_box = element->GetLayoutBox();
  // All drawn elements should have their own stacking contexts.
  CHECK(layout_box->HasLayer());
  CHECK(layout_box->IsStacked());
  PaintLayer* layer = layout_box->EnclosingLayer();

  auto box_rect = gfx::Rect(ToCeiledSize(layer->GetLayoutBox()->Size()));
  // TODO(https://issues.chromium.org/379143301): Figure out the actual painted
  // rect of the element plus its descendants, and use that instead of the
  // box's size.
  OverriddenCullRectScope cull_rect_scope(*layer, CullRect(box_rect),
                                          /*disable_expansion*/ true);

  PaintLayerPainter paint_layer_painter = PaintLayerPainter(*layer);
  paint_layer_painter.Paint(builder.Context(), PaintFlag::kPlacedElement);

  PropertyTreeState property_tree_state = layer->GetLayoutObject()
                                              .FirstFragment()
                                              .LocalBorderBoxProperties()
                                              .Unalias();

  cc::PaintRecord paint_record = builder.EndRecording(property_tree_state);

  WillDraw(SkIRect::MakeXYWH(0, 0, Width(), Height()),
           CanvasPerformanceMonitor::DrawType::kOther);

  cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();
  canvas->save();
  canvas->translate(x, y);
  if (dwidth && dheight) {
    canvas->scale(*dwidth / box_rect.width(), *dheight / box_rect.height());
  }

  canvas->clipRect(SkRect::MakeWH(box_rect.width(), box_rect.height()));
  canvas->drawPicture(
      paint_record,
      // use a save at the beginning of the record to keep transforms local:
      true);

  canvas->restore();
}

void CanvasRenderingContext2D::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "CanvasRenderingContext2D::FinalizeFrame");
  if (!IsPaintable()) {
    return;
  }

  // NOTE: Historically IsPaintable() checked for the existence of the canvas'
  // bridge rather than its ResourceProvider. When IsPaintable() checks for the
  // existence of the ResourceProvider, the below code is unnecessary.
  if (!CheckProviderInCanvas2DRenderingContextIsPaintable()) {
    // Make sure surface is ready for painting: fix the rendering mode now
    // because it will be too late during the paint invalidation phase.
    if (!canvas()->GetOrCreateCanvasResourceProvider()) {
      return;
    }
  }

  HTMLCanvasElement* host = canvas();
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

bool CanvasRenderingContext2D::IsPaintable() const {
  if (CheckProviderInCanvas2DRenderingContextIsPaintable()) {
    return canvas() && canvas()->ResourceProvider();
  } else {
    return canvas() && canvas()->GetCanvas2DLayerBridge();
  }
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

  // NOTE: Historically this method executed the code in
  // OnPageVisibilityChangeWhenPaintable() only when the bridge existed because
  // that method used to be on the bridge itself.  It is not correct to guard
  // the execution of this call on the presence of the resource provider since
  // OnPageVisibilityChangeWhenPaintable() internally has logic to handle the
  // case where the resource provider isn't present. Code inspection shows that
  // there is no indication that the execution of this code needs to have any
  // restriction.
  // TODO(crbug.com/40280152): Merge OnPageVisibilityChangeWhenPaintable() into
  // this method post-safe rollout.
  if (IsPaintable() || CheckProviderInCanvas2DRenderingContextIsPaintable()) {
    OnPageVisibilityChangeWhenPaintable();
  }
  if (!element->IsPageVisible()) {
    PruneLocalFontCache(0);
  }
}

void CanvasRenderingContext2D::OnPageVisibilityChangeWhenPaintable() {
  // NOTE: See the comment at the callsite of this method.
  if (!CheckProviderInCanvas2DRenderingContextIsPaintable()) {
    CHECK(IsPaintable());
  }
  HTMLCanvasElement* const element = canvas();

  bool page_is_visible = element->IsPageVisible();
  CanvasResourceProvider* resource_provider = element->ResourceProvider();
  if (resource_provider) {
    resource_provider->SetResourceRecyclingEnabled(page_is_visible);
  }

  // Conserve memory.
  SetAggressivelyFreeSharedGpuContextResourcesIfPossible(!page_is_visible);

  if (features::IsCanvas2DHibernationEnabled() && !page_is_visible &&
      !element->IsHibernating() && resource_provider &&
      resource_provider->IsAccelerated()) {
    element->GetHibernationHandler()->InitiateHibernationIfNecessary();
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

  if (page_is_visible && element->IsHibernating()) {
    element->GetOrCreateCanvasResourceProvider();  // Rude awakening
  }
}

cc::Layer* CanvasRenderingContext2D::CcLayer() const {
  // This check of IsPaintable() originated when the CC layer was held and
  // obtained by the bridge. It is now held and obtained by the canvas, so it
  // makes sense to simply check whether the canvas is present before asking it
  // to get/create the CC layer.
  bool can_get_cc_layer = CheckProviderInCanvas2DRenderingContextIsPaintable()
                              ? canvas() != nullptr
                              : IsPaintable();

  if (!can_get_cc_layer) {
    return nullptr;
  }

  return canvas()->GetOrCreateCcLayerIfNeeded();
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

UniqueFontSelector* CanvasRenderingContext2D::GetFontSelector() const {
  return canvas()->GetFontSelector();
}

CanvasResourceProvider*
CanvasRenderingContext2D::GetOrCreateCanvas2DResourceProvider() {
  HTMLCanvasElement* const element = canvas();
  if (!element) [[unlikely]] {
    return nullptr;
  }
  return element->GetOrCreateCanvasResourceProvider();
}

}  // namespace blink
