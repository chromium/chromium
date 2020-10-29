/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/traced_value.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

GraphicsLayer::GraphicsLayer(GraphicsLayerClient& client)
    : client_(client),
      prevent_contents_opaque_changes_(false),
      draws_content_(false),
      paints_hit_test_(false),
      contents_visible_(true),
      hit_testable_(false),
      needs_check_raster_invalidation_(false),
      raster_invalidated_(false),
      should_create_layers_after_paint_(false),
      repainted_(false),
      painting_phase_(kGraphicsLayerPaintAllWithOverflowClip),
      parent_(nullptr),
      raster_invalidation_function_(
          base::BindRepeating(&GraphicsLayer::InvalidateRaster,
                              base::Unretained(this))) {
  // TODO(crbug.com/1033240): Debugging information for the referenced bug.
  // Remove when it is fixed.
  CHECK(&client_);

#if DCHECK_IS_ON()
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  client.VerifyNotPainting();
#endif
  layer_ = cc::PictureLayer::Create(this);
  layer_->SetIsDrawable(draws_content_ && contents_visible_);
  layer_->SetHitTestable(hit_testable_);

  UpdateTrackingRasterInvalidations();
}

GraphicsLayer::~GraphicsLayer() {
  CcLayer().ClearClient();
  contents_layer_ = nullptr;

#if DCHECK_IS_ON()
  client_.VerifyNotPainting();
#endif

  RemoveAllChildren();
  RemoveFromParent();
  DCHECK(!parent_);

  // This ensures we clean-up the ElementId to cc::Layer mapping in
  // LayerTreeHost before a new layer with the same ElementId is added. See
  // https://crbug.com/979002 for more information.
  SetElementId(CompositorElementId());
}

void GraphicsLayer::AppendAdditionalInfoAsJSON(LayerTreeFlags flags,
                                               const cc::Layer& layer,
                                               JSONObject& json) const {
  // Only the primary layer associated with GraphicsLayer adds additional
  // information.  Other layer state, such as raster invalidations, don't
  // disambiguate between specific layers.
  if (&layer != layer_.get())
    return;

  if ((flags & (kLayerTreeIncludesInvalidations |
                kLayerTreeIncludesDetailedInvalidations)) &&
      Client().IsTrackingRasterInvalidations() &&
      GetRasterInvalidationTracking()) {
    GetRasterInvalidationTracking()->AsJSON(
        &json, flags & kLayerTreeIncludesDetailedInvalidations);
  }

  GraphicsLayerPaintingPhase painting_phase = PaintingPhase();
  if ((flags & kLayerTreeIncludesPaintingPhases) && painting_phase) {
    auto painting_phases_json = std::make_unique<JSONArray>();
    if (painting_phase & kGraphicsLayerPaintBackground)
      painting_phases_json->PushString("GraphicsLayerPaintBackground");
    if (painting_phase & kGraphicsLayerPaintForeground)
      painting_phases_json->PushString("GraphicsLayerPaintForeground");
    if (painting_phase & kGraphicsLayerPaintMask)
      painting_phases_json->PushString("GraphicsLayerPaintMask");
    if (painting_phase & kGraphicsLayerPaintOverflowContents)
      painting_phases_json->PushString("GraphicsLayerPaintOverflowContents");
    if (painting_phase & kGraphicsLayerPaintCompositedScroll)
      painting_phases_json->PushString("GraphicsLayerPaintCompositedScroll");
    if (painting_phase & kGraphicsLayerPaintDecoration)
      painting_phases_json->PushString("GraphicsLayerPaintDecoration");
    json.SetArray("paintingPhases", std::move(painting_phases_json));
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    bool debug = flags & kLayerTreeIncludesDebugInfo;
    {
      auto squashing_disallowed_reasons_json = std::make_unique<JSONArray>();
      SquashingDisallowedReasons squashing_disallowed_reasons =
          GetSquashingDisallowedReasons();
      auto names = debug ? SquashingDisallowedReason::Descriptions(
                               squashing_disallowed_reasons)
                         : SquashingDisallowedReason::ShortNames(
                               squashing_disallowed_reasons);
      for (const char* name : names)
        squashing_disallowed_reasons_json->PushString(name);
      json.SetArray("squashingDisallowedReasons",
                    std::move(squashing_disallowed_reasons_json));
    }
  }

  if (ShouldCreateLayersAfterPaint()) {
    json.SetBoolean("shouldCreateLayersAfterPaint", true);
  } else {
#if DCHECK_IS_ON()
    if (HasLayerState() && cc_display_item_list_ &&
        (flags & kLayerTreeIncludesPaintRecords)) {
      LoggingCanvas canvas;
      cc_display_item_list_->Raster(&canvas);
      json.SetValue("paintRecord", canvas.Log());
    }
#endif
  }
}

void GraphicsLayer::SetParent(GraphicsLayer* layer) {
#if DCHECK_IS_ON()
  DCHECK(!layer || !layer->HasAncestor(this));
#endif
  parent_ = layer;
}

#if DCHECK_IS_ON()

bool GraphicsLayer::HasAncestor(GraphicsLayer* ancestor) const {
  for (GraphicsLayer* curr = Parent(); curr; curr = curr->Parent()) {
    if (curr == ancestor)
      return true;
  }

  return false;
}

#endif

bool GraphicsLayer::SetChildren(const GraphicsLayerVector& new_children) {
  // If the contents of the arrays are the same, nothing to do.
  if (new_children == children_)
    return false;

  RemoveAllChildren();

  for (auto* new_child : new_children)
    AddChildInternal(new_child);

  NotifyChildListChange();

  return true;
}

void GraphicsLayer::AddChildInternal(GraphicsLayer* child_layer) {
  // TODO(szager): Remove CHECK after diagnosing crbug.com/1092673
  CHECK(child_layer);
  DCHECK_NE(child_layer, this);

  if (child_layer->Parent())
    child_layer->RemoveFromParent();

  child_layer->SetParent(this);
  children_.push_back(child_layer);

  // Don't call NotifyChildListChange here, this function is used in cases where
  // it should not be called until all children are processed.
}

void GraphicsLayer::AddChild(GraphicsLayer* child_layer) {
  AddChildInternal(child_layer);
  NotifyChildListChange();
}

void GraphicsLayer::RemoveAllChildren() {
  while (!children_.IsEmpty()) {
    GraphicsLayer* cur_layer = children_.back();
    DCHECK(cur_layer->Parent());
    cur_layer->RemoveFromParent();
  }
}

void GraphicsLayer::RemoveFromParent() {
  if (parent_) {
    // We use reverseFind so that removeAllChildren() isn't n^2.
    parent_->children_.EraseAt(parent_->children_.ReverseFind(this));
    SetParent(nullptr);
  }

  // cc::Layers are created and removed in PaintArtifactCompositor so ensure it
  // is notified that something has changed.
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::SetOffsetFromLayoutObject(const IntSize& offset) {
  if (offset == offset_from_layout_object_)
    return;

  offset_from_layout_object_ = offset;
  Invalidate(PaintInvalidationReason::kFullLayer);  // As DisplayItemClient.
}

IntRect GraphicsLayer::InterestRect() {
  return previous_interest_rect_;
}

void GraphicsLayer::ClearPaintStateRecursively() {
  ForAllGraphicsLayers(
      *this,
      [](GraphicsLayer& layer) -> bool {
        layer.paint_controller_ = nullptr;
        layer.raster_invalidator_ = nullptr;
        return true;
      },
      [](GraphicsLayer&, const cc::Layer&) {});
}

bool GraphicsLayer::PaintRecursively(
    GraphicsContext& context,
    Vector<PreCompositedLayerInfo>& pre_composited_layers,
    PaintBenchmarkMode benchmark_mode) {
  bool repainted = false;
  ForAllGraphicsLayers(
      *this,
      [&](GraphicsLayer& layer) -> bool {
        if (layer.Client().ShouldSkipPaintingSubtree()) {
          layer.ClearPaintStateRecursively();
          return false;
        }
        layer.Paint(pre_composited_layers, benchmark_mode);
        repainted |= layer.repainted_;
        return true;
      },
      [&](const GraphicsLayer& layer, cc::Layer& contents_layer) {
        PaintChunkSubsetRecorder subset_recorder(context.GetPaintController());
        RecordForeignLayer(
            context, layer, DisplayItem::kForeignLayerContentsWrapper,
            &contents_layer, layer.GetContentsOffsetFromTransformNode(),
            &layer.GetContentsPropertyTreeState());
        pre_composited_layers.push_back(
            PreCompositedLayerInfo{subset_recorder.Get()});
      });
#if DCHECK_IS_ON()
  if (repainted) {
    VLOG(2) << "GraphicsLayer tree:\n"
            << GraphicsLayerTreeAsTextForTesting(
                   this, VLOG_IS_ON(3) ? 0xffffffff : kOutputAsLayerTree)
                   .Utf8();
  }
#endif
  return repainted;
}

void GraphicsLayer::PaintForTesting(const IntRect& interest_rect) {
  Vector<PreCompositedLayerInfo> pre_composited_layers;
  Paint(pre_composited_layers, PaintBenchmarkMode::kNormal, &interest_rect);
}

void GraphicsLayer::Paint(Vector<PreCompositedLayerInfo>& pre_composited_layers,
                          PaintBenchmarkMode benchmark_mode,
                          const IntRect* interest_rect) {
  repainted_ = false;

  DCHECK(!client_.ShouldSkipPaintingSubtree());

  if (!PaintsContentOrHitTest()) {
    if (IsHitTestable()) {
      pre_composited_layers.push_back(
          PreCompositedLayerInfo{PaintChunkSubset(), this});
    }
    return;
  }

#if !DCHECK_IS_ON()
  // TODO(crbug.com/853096): Investigate why we can ever reach here without
  // a valid layer state. Seems to only happen on Android builds.
  // TODO(chrishtr): I think this might have been due to iframe throttling.
  // The comment above and the code here was written before the early
  // out if client_.ShouldThrottleRendering() was true was added. Throttled
  // layers can of course have a stale or missing layer_state_.
  if (!layer_state_)
    return;
#endif
  DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();

  IntRect new_interest_rect =
      interest_rect
          ? *interest_rect
          : client_.ComputeInterestRect(this, previous_interest_rect_);

  auto& paint_controller = GetPaintController();
  PaintController::ScopedBenchmarkMode scoped_benchmark_mode(paint_controller,
                                                             benchmark_mode);
  bool cached = !paint_controller.ShouldForcePaintForBenchmark() &&
                !client_.NeedsRepaint(*this) &&
                // TODO(wangxianzhu): This will be replaced by subsequence
                // caching when unifying PaintController.
                paint_controller.ClientCacheIsValid(*this) &&
                previous_interest_rect_ == new_interest_rect;
  if (!cached) {
    GraphicsContext context(paint_controller);
    DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();
    paint_controller.UpdateCurrentPaintChunkProperties(nullptr,
                                                       layer_state_->state);
    previous_interest_rect_ = new_interest_rect;
    client_.PaintContents(this, context, painting_phase_, new_interest_rect);
    paint_controller.CommitNewDisplayItems();
    // TODO(wangxianzhu): Remove this and friend class in DisplayItemClient
    // when unifying PaintController.
    Validate();
    DVLOG(2) << "Painted GraphicsLayer: " << DebugName()
             << " interest_rect=" << InterestRect().ToString();
  }

  PaintChunkSubset chunks(paint_controller.GetPaintArtifactShared());
  pre_composited_layers.push_back(PreCompositedLayerInfo{chunks, this});

  if (cached && !needs_check_raster_invalidation_ &&
      paint_controller.GetBenchmarkMode() !=
          PaintBenchmarkMode::kForceRasterInvalidationAndConvert) {
    return;
  }

  if (!ShouldCreateLayersAfterPaint()) {
    auto& raster_invalidator = EnsureRasterInvalidator();
    gfx::Size old_layer_size = raster_invalidator.LayerBounds().size();
    gfx::Rect layer_bounds(layer_state_->offset, Size());
    EnsureRasterInvalidator().Generate(raster_invalidation_function_, chunks,
                                       layer_bounds,
                                       layer_state_->state.Unalias(), this);

    base::Optional<RasterUnderInvalidationCheckingParams>
        raster_under_invalidation_params;
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
        PaintsContentOrHitTest()) {
      raster_under_invalidation_params.emplace(
          EnsureRasterInvalidator().EnsureTracking(), InterestRect(),
          DebugName());
    }

    // If nothing changed in the layer, keep the original display item list.
    // Here check layer_bounds because RasterInvalidator doesn't issue raster
    // invalidation when only layer_bounds change.
    if (raster_invalidated_ || !cc_display_item_list_ ||
        old_layer_size != Size() || raster_under_invalidation_params) {
      cc_display_item_list_ = PaintChunksToCcLayer::Convert(
          chunks, layer_state_->state.Unalias(),
          gfx::Vector2dF(layer_state_->offset.X(), layer_state_->offset.Y()),
          cc::DisplayItemList::kTopLevelDisplayItemList,
          base::OptionalOrNullptr(raster_under_invalidation_params));
      raster_invalidated_ = false;
    }
  }

  needs_check_raster_invalidation_ = false;
  repainted_ = true;
}

void GraphicsLayer::SetShouldCreateLayersAfterPaint(
    bool should_create_layers_after_paint) {
  DCHECK(RuntimeEnabledFeatures::CompositeSVGEnabled());
  if (should_create_layers_after_paint != should_create_layers_after_paint_) {
    should_create_layers_after_paint_ = should_create_layers_after_paint;
    // Depending on |should_create_layers_after_paint_|, raster invalidation
    // will happen in via two different code paths. When it changes we need to
    // fully invalidate because the incremental raster invalidations of these
    // code paths will not work.
    if (raster_invalidator_)
      raster_invalidator_->ClearOldStates();
  }
}

void GraphicsLayer::NotifyChildListChange() {
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::UpdateLayerIsDrawable() {
  // For the rest of the accelerated compositor code, there is no reason to make
  // a distinction between drawsContent and contentsVisible. So, for
  // m_layer->layer(), these two flags are combined here. |m_contentsLayer|
  // shouldn't receive the drawsContent flag, so it is only given
  // contentsVisible.

  CcLayer().SetIsDrawable(draws_content_ && contents_visible_);
  if (contents_layer_)
    contents_layer_->SetIsDrawable(contents_visible_);

  if (draws_content_)
    CcLayer().SetNeedsDisplay();
}

void GraphicsLayer::UpdateContentsLayerBounds() {
  if (!contents_layer_)
    return;

  IntSize contents_size = contents_rect_.Size();
  contents_layer_->SetBounds(gfx::Size(contents_size));
}

void GraphicsLayer::SetContentsToCcLayer(
    scoped_refptr<cc::Layer> contents_layer,
    bool prevent_contents_opaque_changes) {
  DCHECK_NE(contents_layer, layer_);
  SetContentsTo(std::move(contents_layer), prevent_contents_opaque_changes);
}

void GraphicsLayer::SetContentsTo(scoped_refptr<cc::Layer> layer,
                                  bool prevent_contents_opaque_changes) {
  if (layer) {
    if (contents_layer_ != layer) {
      contents_layer_ = std::move(layer);
      // It is necessary to call SetDrawsContent() as soon as we receive the new
      // contents_layer, for the correctness of early exit conditions in
      // SetDrawsContent() and SetContentsVisible().
      contents_layer_->SetIsDrawable(contents_visible_);
      contents_layer_->SetHitTestable(contents_visible_);
      NotifyChildListChange();
    }
    UpdateContentsLayerBounds();
    prevent_contents_opaque_changes_ = prevent_contents_opaque_changes;
  } else if (contents_layer_) {
    contents_layer_ = nullptr;
    NotifyChildListChange();
  }
}

RasterInvalidator& GraphicsLayer::EnsureRasterInvalidator() {
  if (!raster_invalidator_) {
    raster_invalidator_ = std::make_unique<RasterInvalidator>();
    raster_invalidator_->SetTracksRasterInvalidations(
        client_.IsTrackingRasterInvalidations());
  }
  return *raster_invalidator_;
}

void GraphicsLayer::UpdateTrackingRasterInvalidations() {
  bool should_track = client_.IsTrackingRasterInvalidations();
  if (should_track)
    EnsureRasterInvalidator().SetTracksRasterInvalidations(true);
  else if (raster_invalidator_)
    raster_invalidator_->SetTracksRasterInvalidations(false);
}

void GraphicsLayer::ResetTrackedRasterInvalidations() {
  if (auto* tracking = GetRasterInvalidationTracking())
    tracking->ClearInvalidations();
}

bool GraphicsLayer::HasTrackedRasterInvalidations() const {
  if (auto* tracking = GetRasterInvalidationTracking())
    return tracking->HasInvalidations();
  return false;
}

RasterInvalidationTracking* GraphicsLayer::GetRasterInvalidationTracking()
    const {
  return raster_invalidator_ ? raster_invalidator_->GetTracking() : nullptr;
}

void GraphicsLayer::TrackRasterInvalidation(const DisplayItemClient& client,
                                            const IntRect& rect,
                                            PaintInvalidationReason reason) {
  if (RasterInvalidationTracking::ShouldAlwaysTrack())
    EnsureRasterInvalidator().EnsureTracking();

  // This only tracks invalidations that the cc::Layer is fully invalidated
  // directly, e.g. from SetContentsNeedsDisplay(), etc. Other raster
  // invalidations are tracked in RasterInvalidator.
  if (auto* tracking = GetRasterInvalidationTracking())
    tracking->AddInvalidation(&client, client.DebugName(), rect, reason);
}

String GraphicsLayer::DebugName(const cc::Layer* layer) const {
  if (layer == contents_layer_.get())
    return "ContentsLayer for " + client_.DebugName(this);

  if (layer == layer_.get())
    return client_.DebugName(this);

  NOTREACHED();
  return "";
}

const gfx::Size& GraphicsLayer::Size() const {
  return CcLayer().bounds();
}

void GraphicsLayer::SetSize(const gfx::Size& size) {
  DCHECK(size.width() >= 0 && size.height() >= 0);

  if (size == CcLayer().bounds())
    return;

  Invalidate(PaintInvalidationReason::kIncremental);  // as DisplayItemClient.

  CcLayer().SetBounds(size);
  SetNeedsCheckRasterInvalidation();
  // Note that we don't resize contents_layer_. It's up the caller to do that.
}

void GraphicsLayer::SetDrawsContent(bool draws_content) {
  // NOTE: This early-exit is only correct because we also properly call
  // cc::Layer::SetIsDrawable() whenever |contents_layer_| is set to a new
  // layer in SetupContentsLayer().
  if (draws_content == draws_content_)
    return;

  // This may affect which layers the client collects.
  client_.GraphicsLayersDidChange();
  // This flag will be updated when the layer is repainted.
  should_create_layers_after_paint_ = false;

  draws_content_ = draws_content;
  UpdateLayerIsDrawable();

  if (!draws_content) {
    paint_controller_.reset();
    raster_invalidator_.reset();
  }
}

void GraphicsLayer::SetContentsVisible(bool contents_visible) {
  // NOTE: This early-exit is only correct because we also properly call
  // cc::Layer::SetIsDrawable() whenever |contents_layer_| is set to a new
  // layer in SetupContentsLayer().
  if (contents_visible == contents_visible_)
    return;

  contents_visible_ = contents_visible;
  UpdateLayerIsDrawable();
}

void GraphicsLayer::SetContentsLayerBackgroundColor(Color color) {
  if (contents_layer_)
    contents_layer_->SetBackgroundColor(color.Rgb());
}

bool GraphicsLayer::ContentsOpaque() const {
  return CcLayer().contents_opaque();
}

void GraphicsLayer::SetContentsOpaque(bool opaque) {
  CcLayer().SetContentsOpaque(opaque);
  if (contents_layer_ && !prevent_contents_opaque_changes_)
    contents_layer_->SetContentsOpaque(opaque);
}

void GraphicsLayer::SetContentsOpaqueForText(bool opaque) {
  CcLayer().SetContentsOpaqueForText(opaque);
}

void GraphicsLayer::SetPaintsHitTest(bool paints_hit_test) {
  if (paints_hit_test_ == paints_hit_test)
    return;
  // This may affect which layers the client collects.
  client_.GraphicsLayersDidChange();
  // This flag will be updated when the layer is repainted.
  should_create_layers_after_paint_ = false;
  paints_hit_test_ = paints_hit_test;
}

void GraphicsLayer::SetHitTestable(bool should_hit_test) {
  if (hit_testable_ == should_hit_test)
    return;
  // This may affect which layers the client collects.
  client_.GraphicsLayersDidChange();
  hit_testable_ = should_hit_test;
  CcLayer().SetHitTestable(should_hit_test);
}

void GraphicsLayer::InvalidateContents() {
  if (contents_layer_) {
    contents_layer_->SetNeedsDisplay();
    TrackRasterInvalidation(*this, contents_rect_,
                            PaintInvalidationReason::kFullLayer);
  }
}

void GraphicsLayer::SetNeedsDisplay() {
  if (!PaintsContentOrHitTest())
    return;

  raster_invalidated_ = true;
  CcLayer().SetNeedsDisplay();

  // Invalidate the paint controller if it exists, but don't bother creating one
  // if not.
  if (paint_controller_)
    paint_controller_->InvalidateAll();

  if (raster_invalidator_)
    raster_invalidator_->ClearOldStates();

  TrackRasterInvalidation(*this, IntRect(IntPoint(), IntSize(Size())),
                          PaintInvalidationReason::kFullLayer);
}

void GraphicsLayer::InvalidateRaster(const IntRect& rect) {
  DCHECK(PaintsContentOrHitTest());
  raster_invalidated_ = true;
  CcLayer().SetNeedsDisplayRect(rect);
}

void GraphicsLayer::SetContentsRect(const IntRect& rect) {
  if (rect == contents_rect_)
    return;

  contents_rect_ = rect;
  UpdateContentsLayerBounds();
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::SetPaintingPhase(GraphicsLayerPaintingPhase phase) {
  if (painting_phase_ == phase)
    return;
  painting_phase_ = phase;
  Invalidate(PaintInvalidationReason::kFullLayer);  // As DisplayItemClient.
}

PaintController& GraphicsLayer::GetPaintController() const {
  CHECK(PaintsContentOrHitTest());
  if (!paint_controller_)
    paint_controller_ = std::make_unique<PaintController>();
  return *paint_controller_;
}

void GraphicsLayer::SetElementId(const CompositorElementId& id) {
  CcLayer().SetElementId(id);
}

void GraphicsLayer::SetLayerState(const PropertyTreeStateOrAlias& layer_state,
                                  const IntPoint& layer_offset) {
  if (layer_state_) {
    if (layer_state_->state == layer_state &&
        layer_state_->offset == layer_offset)
      return;
    layer_state_->state = layer_state;
    layer_state_->offset = layer_offset;
  } else {
    layer_state_ =
        std::make_unique<LayerState>(LayerState{layer_state, layer_offset});
  }

  CcLayer().SetSubtreePropertyChanged();
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::SetContentsLayerState(
    const PropertyTreeStateOrAlias& layer_state,
    const IntPoint& layer_offset) {
  DCHECK(ContentsLayer());

  if (contents_layer_state_) {
    if (contents_layer_state_->state == layer_state &&
        contents_layer_state_->offset == layer_offset)
      return;
    contents_layer_state_->state = layer_state;
    contents_layer_state_->offset = layer_offset;
  } else {
    contents_layer_state_ =
        std::make_unique<LayerState>(LayerState{layer_state, layer_offset});
  }

  ContentsLayer()->SetSubtreePropertyChanged();
  client_.GraphicsLayersDidChange();
}

scoped_refptr<cc::DisplayItemList> GraphicsLayer::PaintContentsToDisplayList() {
  DCHECK(!ShouldCreateLayersAfterPaint());
  return cc_display_item_list_;
}

size_t GraphicsLayer::ApproximateUnsharedMemoryUsageRecursive() const {
  size_t result = sizeof(*this);
  if (paint_controller_)
    result += paint_controller_->ApproximateUnsharedMemoryUsage();
  if (raster_invalidator_)
    result += raster_invalidator_->ApproximateUnsharedMemoryUsage();
  for (auto* child : Children())
    result += child->ApproximateUnsharedMemoryUsageRecursive();
  return result;
}

}  // namespace blink

#if DCHECK_IS_ON()
void showGraphicsLayerTree(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showGraphicsLayerTree for (nil).";
    return;
  }

  String output = blink::GraphicsLayerTreeAsTextForTesting(layer, 0xffffffff);
  LOG(INFO) << output.Utf8();
}
#endif
