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

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/traced_value.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/platform.h"
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

GraphicsLayer::GraphicsLayer()
    : draws_content_(false),
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
  layer_ = cc::PictureLayer::Create(this);
  layer_->SetIsDrawable(draws_content_ && contents_visible_);
  layer_->SetHitTestable(hit_testable_);

  UpdateTrackingRasterInvalidations();
}

GraphicsLayer::~GraphicsLayer() {
#if DCHECK_IS_ON()
  DCHECK(is_destroyed_);
#endif
}

void GraphicsLayer::Destroy() {
  CcLayer().ClearClient();
  contents_layer_ = nullptr;

  RemoveAllChildren();
  RemoveFromParent();
  DCHECK(!parent_);

  // This ensures we clean-up the ElementId to cc::Layer mapping in
  // LayerTreeHost before a new layer with the same ElementId is added. See
  // https://crbug.com/979002 for more information.
  SetElementId(CompositorElementId());

#if DCHECK_IS_ON()
  is_destroyed_ = true;
#endif
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
      IsTrackingRasterInvalidations() && GetRasterInvalidationTracking()) {
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

  for (GraphicsLayer* new_child : new_children)
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
}

void GraphicsLayer::SetOffsetFromLayoutObject(const gfx::Vector2d& offset) {
  if (offset == offset_from_layout_object_)
    return;

  offset_from_layout_object_ = offset;
  Invalidate(PaintInvalidationReason::kFullLayer);  // As DisplayItemClient.
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

void GraphicsLayer::SetShouldCreateLayersAfterPaint(
    bool should_create_layers_after_paint) {
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

void GraphicsLayer::NotifyChildListChange() {}

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

  contents_layer_->SetBounds(contents_rect_.size());
}

void GraphicsLayer::SetContentsToCcLayer(
    scoped_refptr<cc::Layer> contents_layer) {
  DCHECK_NE(contents_layer, layer_);
  SetContentsTo(std::move(contents_layer));
}

void GraphicsLayer::SetContentsTo(scoped_refptr<cc::Layer> layer) {
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
  } else if (contents_layer_) {
    contents_layer_ = nullptr;
    NotifyChildListChange();
  }
}

RasterInvalidator& GraphicsLayer::EnsureRasterInvalidator() {
  if (!raster_invalidator_) {
    raster_invalidator_ = std::make_unique<RasterInvalidator>();
    raster_invalidator_->SetTracksRasterInvalidations(
        IsTrackingRasterInvalidations());
  }
  return *raster_invalidator_;
}

bool GraphicsLayer::IsTrackingRasterInvalidations() const {
#if DCHECK_IS_ON()
  if (VLOG_IS_ON(3))
    return true;
#endif
  return false;
}

void GraphicsLayer::UpdateTrackingRasterInvalidations() {
  if (IsTrackingRasterInvalidations())
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
                                            const gfx::Rect& rect,
                                            PaintInvalidationReason reason) {
  if (RasterInvalidationTracking::ShouldAlwaysTrack())
    EnsureRasterInvalidator().EnsureTracking();

  // This only tracks invalidations that the cc::Layer is fully invalidated
  // directly, e.g. from SetContentsNeedsDisplay(), etc. Other raster
  // invalidations are tracked in RasterInvalidator.
  if (auto* tracking = GetRasterInvalidationTracking()) {
    tracking->AddInvalidation(client.Id(), client.DebugName(), rect, reason);
  }
}

String GraphicsLayer::DebugName(const cc::Layer* layer) const {
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

void GraphicsLayer::SetPaintsHitTest(bool paints_hit_test) {
  if (paints_hit_test_ == paints_hit_test)
    return;
  // This flag will be updated when the layer is repainted.
  should_create_layers_after_paint_ = false;
  paints_hit_test_ = paints_hit_test;
}

void GraphicsLayer::SetHitTestable(bool should_hit_test) {
  if (hit_testable_ == should_hit_test)
    return;
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

void GraphicsLayer::InvalidateRaster(const gfx::Rect& rect) {
  DCHECK(PaintsContentOrHitTest());
  raster_invalidated_ = true;
  CcLayer().SetNeedsDisplayRect(rect);
}

void GraphicsLayer::SetContentsRect(const gfx::Rect& rect) {
  if (rect == contents_rect_)
    return;

  contents_rect_ = rect;
  UpdateContentsLayerBounds();
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
                                  const gfx::Vector2d& layer_offset) {
  if (layer_state_) {
    if (layer_state_->state == layer_state &&
        layer_state_->offset == layer_offset)
      return;
    layer_state_->state = layer_state;
    layer_state_->offset = layer_offset;
  } else {
    layer_state_ = std::make_unique<LayerState>(
        LayerState{RefCountedPropertyTreeState(layer_state), layer_offset});
  }

  CcLayer().SetSubtreePropertyChanged();
}

void GraphicsLayer::SetContentsLayerState(
    const PropertyTreeStateOrAlias& layer_state,
    const gfx::Vector2d& layer_offset) {
  DCHECK(ContentsLayer());

  if (contents_layer_state_) {
    if (contents_layer_state_->state == layer_state &&
        contents_layer_state_->offset == layer_offset)
      return;
    contents_layer_state_->state = layer_state;
    contents_layer_state_->offset = layer_offset;
  } else {
    contents_layer_state_ = std::make_unique<LayerState>(
        LayerState{RefCountedPropertyTreeState(layer_state), layer_offset});
  }

  ContentsLayer()->SetSubtreePropertyChanged();
}

gfx::Rect GraphicsLayer::PaintableRegion() const {
  return previous_interest_rect_;
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
  for (GraphicsLayer* child : Children())
    result += child->ApproximateUnsharedMemoryUsageRecursive();
  return result;
}

void GraphicsLayer::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  DisplayItemClient::Trace(visitor);
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
