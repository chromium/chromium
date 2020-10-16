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
#include "base/rand_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/hit_region.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static const base::TimeDelta kTryRestoreContextInterval =
    base::TimeDelta::FromMilliseconds(500);
static const unsigned kMaxTryRestoreContextAttempts = 4;

static bool ContextLostRestoredEventsEnabled() {
  return RuntimeEnabledFeatures::Canvas2dContextLostRestoredEnabled();
}

// Drawing methods need to use this instead of SkAutoCanvasRestore in case
// overdraw detection substitutes the recording canvas (to discard overdrawn
// draw calls).
class CanvasRenderingContext2DAutoRestoreSkCanvas {
  STACK_ALLOCATED();

 public:
  explicit CanvasRenderingContext2DAutoRestoreSkCanvas(
      CanvasRenderingContext2D* context)
      : context_(context), save_count_(0) {
    DCHECK(context_);
    cc::PaintCanvas* c = context_->GetOrCreatePaintCanvas();
    if (c) {
      save_count_ = c->getSaveCount();
    }
  }

  ~CanvasRenderingContext2DAutoRestoreSkCanvas() {
    cc::PaintCanvas* c = context_->GetOrCreatePaintCanvas();
    if (c)
      c->restoreToCount(save_count_);
    context_->ValidateStateStack();
  }

 private:
  CanvasRenderingContext2D* context_;
  int save_count_;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(canvas, attrs),
      context_lost_mode_(kNotLostContext),
      context_restorable_(true),
      try_restore_context_attempt_count_(0),
      dispatch_context_lost_event_timer_(
          canvas->GetDocument().GetTaskRunner(TaskType::kMiscPlatformAPI),
          this,
          &CanvasRenderingContext2D::DispatchContextLostEvent),
      dispatch_context_restored_event_timer_(
          canvas->GetDocument().GetTaskRunner(TaskType::kMiscPlatformAPI),
          this,
          &CanvasRenderingContext2D::DispatchContextRestoredEvent),
      try_restore_context_event_timer_(
          canvas->GetDocument().GetTaskRunner(TaskType::kMiscPlatformAPI),
          this,
          &CanvasRenderingContext2D::TryRestoreContextEvent),
      should_prune_local_font_cache_(false),
      random_generator_((uint32_t)base::RandUint64()),
      bernoulli_distribution_(kRasterMetricProbability),
      ukm_recorder_(canvas->GetDocument().UkmRecorder()),
      ukm_source_id_(canvas->GetDocument().UkmSourceID()) {
  if (canvas->GetDocument().GetSettings() &&
      canvas->GetDocument().GetSettings()->GetAntialiasedClips2dCanvasEnabled())
    clip_antialiasing_ = kAntiAliased;
  SetShouldAntialias(true);
  ValidateStateStack();
}

void CanvasRenderingContext2D::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetCanvasRenderingContext2D(this);
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() = default;

void CanvasRenderingContext2D::ValidateStateStackWithCanvas(
    const cc::PaintCanvas* canvas) const {
#if DCHECK_IS_ON()
  if (canvas) {
    // The canvas should always have an initial save frame, to support
    // resetting the top level matrix and clip.
    DCHECK_GT(canvas->getSaveCount(), 1);

    if (context_lost_mode_ == kNotLostContext) {
      DCHECK_EQ(static_cast<size_t>(canvas->getSaveCount()),
                state_stack_.size() + 1);
    }
  }
#endif
  CHECK(state_stack_.front()
            .Get());  // Temporary for investigating crbug.com/648510
}

bool CanvasRenderingContext2D::IsAccelerated() const {
  Canvas2DLayerBridge* layer_bridge = canvas()->GetCanvas2DLayerBridge();
  if (!layer_bridge)
    return false;
  return layer_bridge->IsAccelerated();
}

bool CanvasRenderingContext2D::IsOriginTopLeft() const {
  // Accelerated 2D contexts have the origin of coordinates on the bottom left,
  // except if they are used for low latency mode (front buffer rendering).
  return !IsAccelerated() || canvas()->LowLatencyEnabled();
}

bool CanvasRenderingContext2D::IsComposited() const {
  return IsAccelerated();
}

void CanvasRenderingContext2D::Stop() {
  if (!isContextLost()) {
    // Never attempt to restore the context because the page is being torn down.
    LoseContext(kSyntheticLostContext);
  }
}

bool CanvasRenderingContext2D::isContextLost() const {
  return context_lost_mode_ != kNotLostContext;
}

void CanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  if (context_lost_mode_ == kSyntheticLostContext && Host()) {
    Host()->DiscardResourceProvider();
  }
  dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void CanvasRenderingContext2D::DidSetSurfaceSize() {
  if (!context_restorable_)
    return;
  // This code path is for restoring from an eviction
  // Restoring from surface failure is handled internally
  DCHECK(context_lost_mode_ != kNotLostContext && !IsPaintable());

  if (CanCreateCanvas2dResourceProvider()) {
    if (ContextLostRestoredEventsEnabled()) {
      dispatch_context_restored_event_timer_.StartOneShot(base::TimeDelta(),
                                                          FROM_HERE);
    } else {
      // legacy synchronous context restoration.
      Reset();
      context_lost_mode_ = kNotLostContext;
    }
  }
}

void CanvasRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(hit_region_manager_);
  visitor->Trace(filter_operations_);
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
  SVGResourceClient::Trace(visitor);
}

void CanvasRenderingContext2D::DispatchContextLostEvent(TimerBase*) {
  if (canvas() && ContextLostRestoredEventsEnabled()) {
    Event* event = Event::CreateCancelable(event_type_names::kContextlost);
    canvas()->DispatchEvent(*event);
    if (event->defaultPrevented()) {
      context_restorable_ = false;
    }
  }

  // If RealLostContext, it means the context was not lost due to surface
  // failure but rather due to a an eviction, which means image buffer exists.
  if (context_restorable_ && context_lost_mode_ == kRealLostContext) {
    try_restore_context_attempt_count_ = 0;
    try_restore_context_event_timer_.StartRepeating(kTryRestoreContextInterval,
                                                    FROM_HERE);
  }
}

void CanvasRenderingContext2D::TryRestoreContextEvent(TimerBase* timer) {
  if (context_lost_mode_ == kNotLostContext) {
    // Canvas was already restored (possibly thanks to a resize), so stop
    // trying.
    try_restore_context_event_timer_.Stop();
    return;
  }

  DCHECK(context_lost_mode_ == kRealLostContext);
  if (IsPaintable() && canvas()->GetCanvas2DLayerBridge()->Restore()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
  }

  if (++try_restore_context_attempt_count_ > kMaxTryRestoreContextAttempts) {
    // final attempt: allocate a brand new image buffer instead of restoring
    Host()->DiscardResourceProvider();
    try_restore_context_event_timer_.Stop();
    if (CanCreateCanvas2dResourceProvider())
      DispatchContextRestoredEvent(nullptr);
  }
}

void CanvasRenderingContext2D::DispatchContextRestoredEvent(TimerBase*) {
  if (context_lost_mode_ == kNotLostContext)
    return;
  Reset();
  context_lost_mode_ = kNotLostContext;
  if (ContextLostRestoredEventsEnabled()) {
    Event* event(Event::Create(event_type_names::kContextrestored));
    canvas()->DispatchEvent(*event);
  }
}

void CanvasRenderingContext2D::WillDrawImage(CanvasImageSource* source) const {
  canvas()->WillDrawImageTo2DContext(source);
}

String CanvasRenderingContext2D::ColorSpaceAsString() const {
  return CanvasRenderingContext::ColorSpaceAsString();
}

CanvasColorParams CanvasRenderingContext2D::ColorParams() const {
  return CanvasRenderingContext::ColorParams();
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

CanvasPixelFormat CanvasRenderingContext2D::PixelFormat() const {
  return ColorParams().PixelFormat();
}

void CanvasRenderingContext2D::Reset() {
  // This is a multiple inheritance bootstrap
  BaseRenderingContext2D::reset();
}

void CanvasRenderingContext2D::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* c) const {
  RestoreMatrixClipStack(c);
}

bool CanvasRenderingContext2D::ShouldAntialias() const {
  return GetState().ShouldAntialias();
}

void CanvasRenderingContext2D::SetShouldAntialias(bool do_aa) {
  ModifiableState().SetShouldAntialias(do_aa);
}

void CanvasRenderingContext2D::scrollPathIntoView() {
  ScrollPathIntoViewInternal(path_);
}

void CanvasRenderingContext2D::scrollPathIntoView(Path2D* path2d) {
  ScrollPathIntoViewInternal(path2d->GetPath());
}

void CanvasRenderingContext2D::ScrollPathIntoViewInternal(const Path& path) {
  if (!GetState().IsTransformInvertible() || path.IsEmpty())
    return;

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
  transformed_path.Transform(GetState().Transform());
  FloatRect bounding_rect = transformed_path.BoundingRect();

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
  renderer->ScrollRectToVisible(
      path_rect, ScrollAlignment::CreateScrollIntoViewParams(
                     horizontal_scroll_mode, vertical_scroll_mode,
                     mojom::blink::ScrollType::kProgrammatic, false,
                     mojom::blink::ScrollBehavior::kAuto));
}

void CanvasRenderingContext2D::clearRect(double x,
                                         double y,
                                         double width,
                                         double height) {
  BaseRenderingContext2D::clearRect(x, y, width, height);

  if (hit_region_manager_ && std::isfinite(x) && std::isfinite(y) &&
      std::isfinite(width) && std::isfinite(height)) {
    FloatRect rect(clampTo<float>(x), clampTo<float>(y), clampTo<float>(width),
                   clampTo<float>(height));
    hit_region_manager_->RemoveHitRegionsInRect(rect, GetState().Transform());
  }
}

void CanvasRenderingContext2D::DidDraw(const SkIRect& dirty_rect) {
  if (dirty_rect.isEmpty())
    return;

  CanvasRenderingContext::DidDraw(dirty_rect);
}

bool CanvasRenderingContext2D::StateHasFilter() {
  return GetState().HasFilter(canvas(), canvas()->Size(), this);
}

sk_sp<PaintFilter> CanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilter(canvas(), canvas()->Size(), this);
}

void CanvasRenderingContext2D::SnapshotStateForFilter() {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (!canvas()->GetDocument().GetFrame())
    return;

  ModifiableState().SetFontForFilter(AccessFont());
}

cc::PaintCanvas* CanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (isContextLost())
    return nullptr;
  if (canvas()->GetOrCreateCanvas2DLayerBridge())
    return canvas()->GetCanvas2DLayerBridge()->GetPaintCanvas();
  return nullptr;
}

cc::PaintCanvas* CanvasRenderingContext2D::GetPaintCanvas() const {
  if (isContextLost() || !canvas()->GetCanvas2DLayerBridge())
    return nullptr;
  if (canvas() && canvas()->GetCanvas2DLayerBridge()->ResourceProvider())
    return canvas()->GetCanvas2DLayerBridge()->GetPaintCanvas();
  return nullptr;
}

String CanvasRenderingContext2D::font() const {
  if (!GetState().HasRealizedFont())
    return kDefaultFont;

  canvas()->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  StringBuilder serialized_font;
  const FontDescription& font_description = GetState().GetFontDescription();

  if (font_description.Style() == ItalicSlopeValue())
    serialized_font.Append("italic ");
  if (font_description.Weight() == BoldWeightValue())
    serialized_font.Append("bold ");
  if (font_description.VariantCaps() == FontDescription::kSmallCaps)
    serialized_font.Append("small-caps ");

  serialized_font.AppendNumber(font_description.ComputedSize());
  serialized_font.Append("px");

  const FontFamily& first_font_family = font_description.Family();
  for (const FontFamily* font_family = &first_font_family; font_family;
       font_family = font_family->Next()) {
    if (font_family != &first_font_family)
      serialized_font.Append(',');

    // FIXME: We should append family directly to serializedFont rather than
    // building a temporary string.
    String family = font_family->Family();
    if (family.StartsWith("-webkit-"))
      family = family.Substring(8);
    if (family.Contains(' '))
      family = "\"" + family + "\"";

    serialized_font.Append(' ');
    serialized_font.Append(family);
  }

  return serialized_font.ToString();
}

void CanvasRenderingContext2D::setFont(const String& new_font) {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (!canvas()->GetDocument().GetFrame())
    return;
  identifiability_study_helper_.MaybeUpdateBuilder(
      CanvasOps::kSetFont, IdentifiabilityBenignStringToken(new_font));

  base::TimeTicks start_time = base::TimeTicks::Now();
  canvas()->GetDocument().UpdateStyleAndLayoutTreeForNode(canvas());

  // The following early exit is dependent on the cache not being empty
  // because an empty cache may indicate that a style change has occured
  // which would require that the font be re-resolved. This check has to
  // come after the layout tree update to flush pending style changes.
  if (new_font == GetState().UnparsedFont() && GetState().HasRealizedFont() &&
      fonts_resolved_using_current_style_.size() > 0)
    return;

  CanvasFontCache* canvas_font_cache =
      canvas()->GetDocument().GetCanvasFontCache();

  // Map the <canvas> font into the text style. If the font uses keywords like
  // larger/smaller, these will work relative to the canvas.
  scoped_refptr<ComputedStyle> font_style;
  const ComputedStyle* computed_style = canvas()->EnsureComputedStyle();
  if (computed_style) {
    auto i = fonts_resolved_using_current_style_.find(new_font);
    if (i != fonts_resolved_using_current_style_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(!add_result.is_new_entry);
      ModifiableState().SetFont(i->value, Host()->GetFontSelector());
    } else {
      MutableCSSPropertyValueSet* parsed_style =
          canvas_font_cache->ParseFont(new_font);
      if (!parsed_style)
        return;
      font_style = ComputedStyle::Create();
      FontDescription element_font_description(
          computed_style->GetFontDescription());
      // Reset the computed size to avoid inheriting the zoom factor from the
      // <canvas> element.
      element_font_description.SetComputedSize(
          element_font_description.SpecifiedSize());
      element_font_description.SetAdjustedSize(
          element_font_description.SpecifiedSize());

      font_style->SetFontDescription(element_font_description);
      canvas()->GetDocument().GetStyleEngine().ComputeFont(
          *canvas(), font_style.get(), *parsed_style);

      // We need to reset Computed and Adjusted size so we skip zoom and
      // minimum font size.
      FontDescription final_description(
          font_style->GetFont().GetFontDescription());
      final_description.SetComputedSize(final_description.SpecifiedSize());
      final_description.SetAdjustedSize(final_description.SpecifiedSize());

      fonts_resolved_using_current_style_.insert(new_font, final_description);
      auto add_result = font_lru_list_.PrependOrMoveToFirst(new_font);
      DCHECK(add_result.is_new_entry);
      PruneLocalFontCache(canvas_font_cache->HardMaxFonts());  // hard limit
      should_prune_local_font_cache_ = true;  // apply soft limit
      ModifiableState().SetFont(final_description, Host()->GetFontSelector());
    }
  } else {
    Font resolved_font;
    if (!canvas_font_cache->GetFontUsingDefaultStyle(*canvas(), new_font,
                                                     resolved_font))
      return;

    // We need to reset Computed and Adjusted size so we skip zoom and
    // minimum font size for detached canvas.
    FontDescription final_description(resolved_font.GetFontDescription());
    final_description.SetComputedSize(final_description.SpecifiedSize());
    final_description.SetAdjustedSize(final_description.SpecifiedSize());
    ModifiableState().SetFont(final_description, Host()->GetFontSelector());
  }

  // The parse succeeded.
  String new_font_safe_copy(new_font);  // Create a string copy since newFont
                                        // can be deleted inside realizeSaves.
  ModifiableState().SetUnparsedFont(new_font_safe_copy);
  if (bernoulli_distribution_(random_generator_)) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    base::UmaHistogramMicrosecondsTimesUnderTenMilliseconds(
        "Canvas.TextMetrics.SetFont", elapsed);
  }
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
  // Only the visibility flag is considered here. display:none is
  // handled via creation and destruction of the LayoutObject.
  SetIsBeingDisplayed(new_style.Visibility() == EVisibility::kVisible);
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

void CanvasRenderingContext2D::ResourceContentChanged(InvalidationModeMask) {
  ResourceElementChanged();
}

void CanvasRenderingContext2D::ResourceElementChanged() {
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
  return Host()->Size().Width();
}

int CanvasRenderingContext2D::Height() const {
  return Host()->Size().Height();
}

bool CanvasRenderingContext2D::CanCreateCanvas2dResourceProvider() const {
  return canvas()->GetOrCreateCanvas2DLayerBridge();
}

scoped_refptr<StaticBitmapImage> blink::CanvasRenderingContext2D::GetImage() {
  if (!IsPaintable())
    return nullptr;
  return canvas()->GetCanvas2DLayerBridge()->NewImageSnapshot();
}

ImageData* CanvasRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ExceptionState& exception_state) {
  const IdentifiableSurface surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kCanvasReadback, GetContextType());
  if (IdentifiabilityStudySettings::Get()->IsSurfaceAllowed(surface)) {
    blink::IdentifiabilityMetricBuilder(ukm_source_id_)
        .Set(surface, 0)
        .Record(ukm_recorder_);
  }
  return BaseRenderingContext2D::getImageData(sx, sy, sw, sh, exception_state);
}

void CanvasRenderingContext2D::FinalizeFrame() {
  TRACE_EVENT0("blink", "CanvasRenderingContext2D::FinalizeFrame");
  if (IsPaintable())
    canvas()->GetCanvas2DLayerBridge()->FinalizeFrame();
}

bool CanvasRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  return ::blink::ParseColorOrCurrentColor(color, color_string, canvas());
}

HitTestCanvasResult* CanvasRenderingContext2D::GetControlAndIdIfHitRegionExists(
    const PhysicalOffset& location) {
  if (HitRegionsCount() <= 0)
    return MakeGarbageCollected<HitTestCanvasResult>(String(), nullptr);

  LayoutBox* box = canvas()->GetLayoutBox();
  FloatPoint local_pos(box->AbsoluteToLocalPoint(location));
  if (box->StyleRef().HasBorder() || box->StyleRef().MayHavePadding())
    local_pos.Move(FloatSize(-box->PhysicalContentBoxOffset()));
  float scaleWidth = box->ContentWidth().ToFloat() == 0.0f
                         ? 1.0f
                         : canvas()->width() / box->ContentWidth();
  float scaleHeight = box->ContentHeight().ToFloat() == 0.0f
                          ? 1.0f
                          : canvas()->height() / box->ContentHeight();
  local_pos.Scale(scaleWidth, scaleHeight);

  HitRegion* hit_region = HitRegionAtPoint(local_pos);
  if (hit_region) {
    Element* control = hit_region->Control();
    if (control && canvas()->IsSupportedInteractiveCanvasFallback(*control)) {
      return MakeGarbageCollected<HitTestCanvasResult>(hit_region->Id(),
                                                       hit_region->Control());
    }
    return MakeGarbageCollected<HitTestCanvasResult>(hit_region->Id(), nullptr);
  }
  return MakeGarbageCollected<HitTestCanvasResult>(String(), nullptr);
}

String CanvasRenderingContext2D::GetIdFromControl(const Element* element) {
  if (HitRegionsCount() <= 0)
    return String();

  if (HitRegion* hit_region =
          hit_region_manager_->GetHitRegionByControl(element))
    return hit_region->Id();
  return String();
}

static inline TextDirection ToTextDirection(
    CanvasRenderingContext2DState::Direction direction,
    HTMLCanvasElement* canvas,
    const ComputedStyle** computed_style = nullptr) {
  const ComputedStyle* style =
      (computed_style ||
       direction == CanvasRenderingContext2DState::kDirectionInherit)
          ? canvas->EnsureComputedStyle()
          : nullptr;
  if (computed_style)
    *computed_style = style;
  switch (direction) {
    case CanvasRenderingContext2DState::kDirectionInherit:
      return style ? style->Direction() : TextDirection::kLtr;
    case CanvasRenderingContext2DState::kDirectionRTL:
      return TextDirection::kRtl;
    case CanvasRenderingContext2DState::kDirectionLTR:
      return TextDirection::kLtr;
  }
  NOTREACHED();
  return TextDirection::kLtr;
}

String CanvasRenderingContext2D::direction() const {
  if (GetState().GetDirection() ==
      CanvasRenderingContext2DState::kDirectionInherit)
    canvas()->GetDocument().UpdateStyleAndLayoutTreeForNode(canvas());
  return ToTextDirection(GetState().GetDirection(), canvas()) ==
                 TextDirection::kRtl
             ? kRtlDirectionString
             : kLtrDirectionString;
}

void CanvasRenderingContext2D::setDirection(const String& direction_string) {
  CanvasRenderingContext2DState::Direction direction;
  if (direction_string == kInheritDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionInherit;
  else if (direction_string == kRtlDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionRTL;
  else if (direction_string == kLtrDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionLTR;
  else
    return;

  if (GetState().GetDirection() == direction)
    return;

  ModifiableState().SetDirection(direction);
}

void CanvasRenderingContext2D::setTextLetterSpacing(
    const double letter_spacing) {
  if (!GetState().HasRealizedFont())
    setFont(font());

  float letter_spacing_float = clampTo<float>(letter_spacing);
  ModifiableState().SetTextLetterSpacing(letter_spacing_float,
                                         Host()->GetFontSelector());
}

void CanvasRenderingContext2D::setTextWordSpacing(const double word_spacing) {
  if (!GetState().HasRealizedFont())
    setFont(font());

  float word_spacing_float = clampTo<float>(word_spacing);
  ModifiableState().SetTextWordSpacing(word_spacing_float,
                                       Host()->GetFontSelector());
}

void CanvasRenderingContext2D::fillText(const String& text,
                                        double x,
                                        double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType);
}

void CanvasRenderingContext2D::fillText(const String& text,
                                        double x,
                                        double y,
                                        double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType,
                   &max_width);
}

void CanvasRenderingContext2D::strokeText(const String& text,
                                          double x,
                                          double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType);
}

void CanvasRenderingContext2D::strokeText(const String& text,
                                          double x,
                                          double y,
                                          double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType,
                   &max_width);
}

TextMetrics* CanvasRenderingContext2D::measureText(const String& text) {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (!canvas()->GetDocument().GetFrame())
    return MakeGarbageCollected<TextMetrics>();

  canvas()->GetDocument().UpdateStyleAndLayoutTreeForNode(canvas());

  const Font& font = AccessFont();

  TextDirection direction;
  if (GetState().GetDirection() ==
      CanvasRenderingContext2DState::kDirectionInherit)
    direction = DetermineDirectionality(text);
  else
    direction = ToTextDirection(GetState().GetDirection(), canvas());

  return MakeGarbageCollected<TextMetrics>(font, direction,
                                           GetState().GetTextBaseline(),
                                           GetState().GetTextAlign(), text);
}

void CanvasRenderingContext2D::DrawTextInternal(
    const String& text,
    double x,
    double y,
    CanvasRenderingContext2DState::PaintType paint_type,
    double* max_width) {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  if (!canvas()->GetDocument().GetFrame())
    return;

  // accessFont needs the style to be up to date, but updating style can cause
  // script to run, (e.g. due to autofocus) which can free the canvas (set size
  // to 0, for example), so update style before grabbing the PaintCanvas.
  canvas()->GetDocument().UpdateStyleAndLayoutTreeForNode(canvas());

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(x) || !std::isfinite(y))
    return;
  if (max_width && (!std::isfinite(*max_width) || *max_width <= 0))
    return;

  identifiability_study_helper_.MaybeUpdateBuilder(
      paint_type == CanvasRenderingContext2DState::kFillPaintType
          ? CanvasOps::kFillText
          : CanvasOps::kStrokeText,
      IdentifiabilitySensitiveStringToken(text), x, y,
      max_width ? *max_width : -1);
  identifiability_study_helper_.set_encountered_sensitive_ops();

  const Font& font = AccessFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;
  const FontMetrics& font_metrics = font_data->GetFontMetrics();

  // FIXME: Need to turn off font smoothing.

  const ComputedStyle* computed_style = nullptr;
  TextDirection direction =
      ToTextDirection(GetState().GetDirection(), canvas(), &computed_style);
  bool is_rtl = direction == TextDirection::kRtl;
  bool override =
      computed_style ? IsOverride(computed_style->GetUnicodeBidi()) : false;

  TextRun text_run(text, 0, 0, TextRun::kAllowTrailingExpansion, direction,
                   override);
  text_run.SetNormalizeSpace(true);
  // Draw the item text at the correct point.
  FloatPoint location(clampTo<float>(x),
                      clampTo<float>(y + GetFontBaseline(*font_data)));
  double font_width = font.Width(text_run);

  bool use_max_width = (max_width && *max_width < font_width);
  double width = use_max_width ? *max_width : font_width;

  TextAlign align = GetState().GetTextAlign();
  if (align == kStartTextAlign)
    align = is_rtl ? kRightTextAlign : kLeftTextAlign;
  else if (align == kEndTextAlign)
    align = is_rtl ? kLeftTextAlign : kRightTextAlign;

  switch (align) {
    case kCenterTextAlign:
      location.SetX(location.X() - width / 2);
      break;
    case kRightTextAlign:
      location.SetX(location.X() - width);
      break;
    default:
      break;
  }

  TextRunPaintInfo text_run_paint_info(text_run);
  FloatRect bounds(
      location.X() - font_metrics.Height() / 2,
      location.Y() - font_metrics.Ascent() - font_metrics.LineGap(),
      clampTo<float>(width + font_metrics.Height()),
      font_metrics.LineSpacing());
  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  CanvasRenderingContext2DAutoRestoreSkCanvas state_restorer(this);
  if (use_max_width) {
    c->save();
    // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op)
    // still work. As the width of canvas is scaled, so text can be scaled to
    // match the given maxwidth, update text location so it appears on desired
    // place.
    c->scale(clampTo<float>(width / font_width), 1);
    location.SetX(location.X() / clampTo<float>(width / font_width));
  }

  Draw(
      [&font, &text_run_paint_info, &location](
          cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
      {
        font.DrawBidiText(c, text_run_paint_info, location,
                          Font::kUseFallbackIfFontNotReady, kCDeviceScaleFactor,
                          *flags);
      },
      [](const SkIRect& rect)  // overdraw test lambda
      { return false; },
      bounds, paint_type, CanvasRenderingContext2DState::kNoImage);
}

const Font& CanvasRenderingContext2D::AccessFont() {
  if (!GetState().HasRealizedFont())
    setFont(GetState().UnparsedFont());
  canvas()->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  return GetState().GetFont();
}

void CanvasRenderingContext2D::SetIsInHiddenPage(bool hidden) {
  if (IsPaintable())
    canvas()->GetCanvas2DLayerBridge()->SetIsInHiddenPage(hidden);
  if (hidden)
    PruneLocalFontCache(0);
}

void CanvasRenderingContext2D::SetIsBeingDisplayed(bool displayed) {
  if (IsPaintable())
    canvas()->GetCanvas2DLayerBridge()->SetIsBeingDisplayed(displayed);
}

bool CanvasRenderingContext2D::IsTransformInvertible() const {
  return GetState().IsTransformInvertible();
}

AffineTransform CanvasRenderingContext2D::Transform() const {
  return GetState().Transform();
}

cc::Layer* CanvasRenderingContext2D::CcLayer() const {
  return IsPaintable() ? canvas()->GetCanvas2DLayerBridge()->Layer() : nullptr;
}

CanvasRenderingContext2DSettings*
CanvasRenderingContext2D::getContextAttributes() const {
  CanvasRenderingContext2DSettings* settings =
      CanvasRenderingContext2DSettings::Create();
  settings->setAlpha(CreationAttributes().alpha);
  if (RuntimeEnabledFeatures::CanvasColorManagementEnabled()) {
    settings->setColorSpace(ColorSpaceAsString());
    settings->setPixelFormat(PixelFormatAsString());
  }
  settings->setDesynchronized(Host()->LowLatencyEnabled());
  if (RuntimeEnabledFeatures::NewCanvas2DAPIEnabled())
    settings->setWillReadFrequently(CreationAttributes().will_read_frequently);
  return settings;
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element) {
  DrawFocusIfNeededInternal(path_, element);
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
    DrawFocusRing(path);
  }

  // Update its accessible bounds whether it's focused or not.
  UpdateElementAccessibility(path, element);
}

bool CanvasRenderingContext2D::FocusRingCallIsValid(const Path& path,
                                                    Element* element) {
  DCHECK(element);
  if (!GetState().IsTransformInvertible())
    return false;
  if (path.IsEmpty())
    return false;
  if (!element->IsDescendantOf(canvas()))
    return false;

  return true;
}

void CanvasRenderingContext2D::DrawFocusRing(const Path& path) {
  if (!GetOrCreatePaintCanvas())
    return;

  SkColor color = LayoutTheme::GetTheme().FocusRingColor().Rgb();
  const int kFocusRingWidth = 5;
  DrawPlatformFocusRing(path.GetSkPath(), GetPaintCanvas(), color,
                        /*width=*/kFocusRingWidth, /*radius=*/kFocusRingWidth);

  // We need to add focusRingWidth to dirtyRect.
  StrokeData stroke_data;
  stroke_data.SetThickness(kFocusRingWidth);

  SkIRect dirty_rect;
  if (!ComputeDirtyRect(path.StrokeBoundingRect(stroke_data), &dirty_rect))
    return;

  DidDraw(dirty_rect);
}

void CanvasRenderingContext2D::UpdateElementAccessibility(const Path& path,
                                                          Element* element) {
  element->GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kAccessibility);
  AXObjectCache* ax_object_cache =
      element->GetDocument().ExistingAXObjectCache();
  LayoutBoxModelObject* lbmo = canvas()->GetLayoutBoxModelObject();
  LayoutObject* renderer = canvas()->GetLayoutObject();
  if (!ax_object_cache || !lbmo || !renderer)
    return;

  // Get the transformed path.
  Path transformed_path = path;
  transformed_path.Transform(GetState().Transform());

  // Add border and padding to the bounding rect.
  LayoutRect element_rect =
      EnclosingLayoutRect(transformed_path.BoundingRect());
  element_rect.Move(lbmo->BorderLeft() + lbmo->PaddingLeft(),
                    lbmo->BorderTop() + lbmo->PaddingTop());

  // Update the accessible object.
  ax_object_cache->SetCanvasObjectBounds(canvas(), element, element_rect);
}

void CanvasRenderingContext2D::addHitRegion(const HitRegionOptions* options,
                                            ExceptionState& exception_state) {
  if (options->id().IsEmpty() && !options->control()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Both id and control are null.");
    return;
  }

  if (options->control() &&
      !canvas()->IsSupportedInteractiveCanvasFallback(*options->control())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The control is neither null nor a "
                                      "supported interactive canvas fallback "
                                      "element.");
    return;
  }

  Path hit_region_path = options->path() ? options->path()->GetPath() : path_;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();

  if (hit_region_path.IsEmpty() || !c || !GetState().IsTransformInvertible() ||
      c->isClipEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The specified path has no pixels.");
    return;
  }

  hit_region_path.Transform(GetState().Transform());

  if (GetState().HasClip()) {
    hit_region_path.IntersectPath(GetState().GetCurrentClipPath());
    if (hit_region_path.IsEmpty()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "The specified path has no pixels.");
    }
  }

  if (!hit_region_manager_)
    hit_region_manager_ = MakeGarbageCollected<HitRegionManager>();

  // Remove previous region (with id or control)
  hit_region_manager_->RemoveHitRegionById(options->id());
  hit_region_manager_->RemoveHitRegionByControl(options->control());

  auto* hit_region = MakeGarbageCollected<HitRegion>(hit_region_path, options);
  Element* element = hit_region->Control();
  if (element && element->IsDescendantOf(canvas()))
    UpdateElementAccessibility(hit_region->GetPath(), hit_region->Control());
  hit_region_manager_->AddHitRegion(hit_region);
}

void CanvasRenderingContext2D::removeHitRegion(const String& id) {
  if (hit_region_manager_)
    hit_region_manager_->RemoveHitRegionById(id);
}

void CanvasRenderingContext2D::clearHitRegions() {
  if (hit_region_manager_)
    hit_region_manager_->RemoveAllHitRegions();
}

HitRegion* CanvasRenderingContext2D::HitRegionAtPoint(const FloatPoint& point) {
  if (hit_region_manager_)
    return hit_region_manager_->GetHitRegionAtPoint(point);

  return nullptr;
}

unsigned CanvasRenderingContext2D::HitRegionsCount() const {
  if (hit_region_manager_)
    return hit_region_manager_->GetHitRegionsCount();

  return 0;
}

// TODO(aaronhk) This is only used for the size heuristic. Delete this function
// once always accelerate fully lands.
void CanvasRenderingContext2D::DisableAcceleration() {
  canvas()->DisableAcceleration();
}

bool CanvasRenderingContext2D::IsCanvas2DBufferValid() const {
  if (IsPaintable()) {
    return canvas()->GetCanvas2DLayerBridge()->IsValid();
  }
  return false;
}

RespectImageOrientationEnum CanvasRenderingContext2D::RespectImageOrientation()
    const {
  if (canvas()->RespectImageOrientation() != kRespectImageOrientation) {
    return kDoNotRespectImageOrientation;
  }
  return kRespectImageOrientation;
}

}  // namespace blink
