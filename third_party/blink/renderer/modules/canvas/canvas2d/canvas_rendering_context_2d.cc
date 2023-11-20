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

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/formatted_text/formatted_text.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

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

NoAllocDirectCallHost* CanvasRenderingContext2D::AsNoAllocDirectCallHost() {
  return this;
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

bool CanvasRenderingContext2D::IsOriginTopLeft() const {
  // Use top-left origin since Skia Graphite won't support bottom-left origin.
  return true;
}

bool CanvasRenderingContext2D::IsComposited() const {
  // The following case is necessary for handling the special case of canvases
  // in the dev tools overlay.
  auto* settings = canvas()->GetDocument().GetSettings();
  if (settings && !settings->GetAcceleratedCompositingEnabled()) {
    return false;
  }
  return canvas()->IsComposited();
}

void CanvasRenderingContext2D::Stop() {
  if (LIKELY(!isContextLost())) {
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
  PostDeferrableAction(WTF::BindOnce(
      [](BaseRenderingContext2D* context) { context->ResetInternal(); },
      WrapPersistent(this)));
  if (context_lost_mode_ == kSyntheticLostContext && Host()) {
    Host()->DiscardResourceProvider();
  }

  if (canvas() && canvas()->IsPageVisible()) {
    dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(),
                                                    FROM_HERE);
  } else {
    needs_context_lost_event_ = true;
  }
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
  if (context_lost_mode_ == kSyntheticLostContext &&
      canvas()->GetOrCreateCanvas2DLayerBridge() &&
      canvas()->GetCanvas2DLayerBridge()->GetPaintCanvas()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
    return;
  }

  // If RealLostContext, it means the context was not lost due to surface
  // failure but rather due to a an eviction, which means image buffer exists.
  if (context_lost_mode_ == kRealLostContext && IsPaintable() &&
      canvas()->GetCanvas2DLayerBridge()->Restore()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
    return;
  }

  // If it fails to restore the context, TryRestoreContextEvent again.
  if (++try_restore_context_attempt_count_ > kMaxTryRestoreContextAttempts) {
    // After 4 tries, we start the final attempt, allocate a brand new image
    // buffer instead of restoring
    try_restore_context_event_timer_.Stop();
    if (Host()) {
      Host()->DiscardResourceProvider();
    }
    if (CanCreateCanvas2dResourceProvider())
      DispatchContextRestoredEvent(nullptr);
  }
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
  return canvas()->GetCanvas2DLayerBridge()->WritePixels(orig_info, pixels,
                                                         row_bytes, x, y);
}

void CanvasRenderingContext2D::WillOverwriteCanvas() {
  if (IsPaintable())
    canvas()->GetCanvas2DLayerBridge()->WillOverwriteCanvas();
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

void CanvasRenderingContext2D::scrollPathIntoView() {
  ScrollPathIntoViewInternal(GetPath());
}

void CanvasRenderingContext2D::scrollPathIntoView(Path2D* path2d) {
  ScrollPathIntoViewInternal(path2d->GetPath());
}

void CanvasRenderingContext2D::ScrollPathIntoViewInternal(const Path& path) {
  if (UNLIKELY(!IsTransformInvertible() || path.IsEmpty())) {
    return;
  }

  canvas()->GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kJavaScript);

  LayoutObject* renderer = canvas()->GetLayoutObject();
  LayoutBox* layout_box = canvas()->GetLayoutBox();
  if (!renderer || !layout_box)
    return;

  if (Width() == 0 || Height() == 0)
    return;

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
      (canvas_rect.X() + path_rect.X() * canvas_rect.Width() / Width()));
  path_rect.SetY(
      (canvas_rect.Y() + path_rect.Y() * canvas_rect.Height() / Height()));
  path_rect.SetWidth((path_rect.Width() * canvas_rect.Width() / Width()));
  path_rect.SetHeight((path_rect.Height() * canvas_rect.Height() / Height()));

  // Then we clip the bounding box to the canvas visible range.
  path_rect.Intersect(canvas_rect);

  // Horizontal text is aligned at the top of the screen
  mojom::blink::ScrollAlignment horizontal_scroll_mode =
      ScrollAlignment::ToEdgeIfNeeded();
  mojom::blink::ScrollAlignment vertical_scroll_mode =
      ScrollAlignment::TopAlways();

  // Vertical text needs be aligned horizontally on the screen
  bool is_horizontal_writing_mode =
      canvas()->EnsureComputedStyle()->IsHorizontalWritingMode();
  if (!is_horizontal_writing_mode) {
    bool is_right_to_left =
        canvas()->EnsureComputedStyle()->IsFlippedBlocksWritingMode();
    horizontal_scroll_mode = (is_right_to_left ? ScrollAlignment::RightAlways()
                                               : ScrollAlignment::LeftAlways());
    vertical_scroll_mode = ScrollAlignment::ToEdgeIfNeeded();
  }
  scroll_into_view_util::ScrollRectToVisible(
      *renderer, path_rect,
      ScrollAlignment::CreateScrollIntoViewParams(
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
  return GetState().GetFilter(canvas(), canvas()->Size(), this);
}

cc::PaintCanvas* CanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (UNLIKELY(isContextLost()))
    return nullptr;
  if (LIKELY(canvas()->GetOrCreateCanvas2DLayerBridge())) {
    if (LIKELY(!layer_count_) && LIKELY(Host()->ResourceProvider())) {
      // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
      Host()->ResourceProvider()->FlushIfRecordingLimitExceeded();
    }
    return canvas()->GetCanvas2DLayerBridge()->GetPaintCanvas();
  }
  return nullptr;
}

cc::PaintCanvas* CanvasRenderingContext2D::GetPaintCanvas() {
  if (UNLIKELY(isContextLost() || !Host() || !Host()->ResourceProvider())) {
    return nullptr;
  }
  return canvas()->GetCanvas2DLayerBridge()->GetPaintCanvas();
}

void CanvasRenderingContext2D::WillDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  CanvasRenderingContext::DidDraw(dirty_rect, draw_type);
  // Always draw everything during printing.
  if (!layer_count_) {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    Host()->ResourceProvider()->FlushIfRecordingLimitExceeded();
  }
}

absl::optional<cc::PaintRecord> CanvasRenderingContext2D::FlushCanvas(
    FlushReason reason) {
  if (Host() && Host()->ResourceProvider()) {
    return Host()->ResourceProvider()->FlushCanvas(reason);
  }
  return absl::nullopt;
}

bool CanvasRenderingContext2D::WillSetFont() const {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (!canvas()->GetDocument().GetFrame()) {
    return false;
  }

  canvas()->GetDocument().UpdateStyleAndLayoutTreeForNode(
      canvas(), DocumentUpdateReason::kCanvas);
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
  CanvasFontCache* canvas_font_cache =
      canvas()->GetDocument().GetCanvasFontCache();

  // Map the <canvas> font into the text style. If the font uses keywords like
  // larger/smaller, these will work relative to the canvas.
  const ComputedStyle* computed_style = canvas()->EnsureComputedStyle();
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
          canvas()
              ->GetDocument()
              .GetStyleResolver()
              .CreateComputedStyleBuilder();
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
      Font font = canvas()->GetDocument().GetStyleEngine().ComputeFont(
          *canvas(), *font_style, *parsed_style);

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
    if (!canvas_font_cache->GetFontUsingDefaultStyle(*canvas(), new_font,
                                                     resolved_font))
      return false;

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
  if (should_prune_local_font_cache_ && canvas()) {
    should_prune_local_font_cache_ = false;
    PruneLocalFontCache(
        canvas()->GetDocument().GetCanvasFontCache()->MaxFonts());
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
  return canvas()->GetCanvas2DLayerBridge()->NewImageSnapshot(reason);
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
  if (IsPaintable())
    canvas()->GetCanvas2DLayerBridge()->FinalizeFrame(reason);
}

CanvasRenderingContextHost*
CanvasRenderingContext2D::GetCanvasRenderingContextHost() const {
  return Host();
}

ExecutionContext* CanvasRenderingContext2D::GetTopExecutionContext() const {
  return Host()->GetTopExecutionContext();
}

Color CanvasRenderingContext2D::GetCurrentColor() const {
  if (!canvas() || !canvas()->isConnected() || !canvas()->InlineStyle()) {
    return Color::kBlack;
  }
  Color color = Color::kBlack;
  CSSParser::ParseColor(
      color, canvas()->InlineStyle()->GetPropertyValue(CSSPropertyID::kColor));
  return color;
}

void CanvasRenderingContext2D::drawFormattedText(
    FormattedText* formatted_text,
    double x,
    double y,
    ExceptionState& exception_state) {
  if (!formatted_text)
    return;
  // TODO(crbug.com/1234113): Instrument new canvas APIs.
  identifiability_study_helper_.set_encountered_skipped_ops();

  if (!GetState().HasRealizedFont())
    setFont(font());

  gfx::RectF bounds;
  PaintRecord recording = formatted_text->PaintFormattedText(
      canvas()->GetDocument(), GetState().GetFontDescription(), x, y, bounds,
      exception_state);
  if (recording.size()) {
    Draw<OverdrawOp::kNone>(
        [&recording](cc::PaintCanvas* c,
                     const cc::PaintFlags* flags)  // draw lambda
        { c->drawPicture(std::move(recording)); },
        [](const SkIRect& rect) { return false; }, bounds,
        CanvasRenderingContext2DState::PaintType::kFillPaintType,
        CanvasRenderingContext2DState::kNoImage,
        CanvasPerformanceMonitor::DrawType::kText);
  }
}

void CanvasRenderingContext2D::PageVisibilityChanged() {
  if (IsPaintable()) {
    canvas()->GetCanvas2DLayerBridge()->PageVisibilityChanged();
  }
  if (!Host()->IsPageVisible()) {
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
    if (UNLIKELY(identifiability_study_helper_.ShouldUpdateBuilder())) {
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
  if (UNLIKELY(!IsTransformInvertible())) {
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

  SkColor color = LayoutTheme::GetTheme().FocusRingColor(color_scheme).Rgb();
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
  LayoutBoxModelObject* lbmo = canvas()->GetLayoutBoxModelObject();
  LayoutObject* renderer = canvas()->GetLayoutObject();
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
  ax_object_cache->SetCanvasObjectBounds(canvas(), element, element_rect);
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

}  // namespace blink
