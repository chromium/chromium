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
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
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

#include "third_party/skia/include/core/SkMatrix44.h"

namespace blink {

GraphicsLayer::GraphicsLayer(GraphicsLayerClient& client)
    : client_(client),
      prevent_contents_opaque_changes_(false),
      draws_content_(false),
      paints_hit_test_(false),
      contents_visible_(true),
      hit_testable_(false),
      needs_check_raster_invalidation_(false),
      painted_(false),
      painting_phase_(kGraphicsLayerPaintAllWithOverflowClip),
      parent_(nullptr),
      mask_layer_(nullptr),
      contents_layer_(nullptr),
      contents_layer_id_(0) {
#if DCHECK_IS_ON()
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  client.VerifyNotPainting();
#endif
  layer_ = cc::PictureLayer::Create(this);
  CcLayer()->SetIsDrawable(draws_content_ && contents_visible_);
  CcLayer()->SetHitTestable(hit_testable_);

  UpdateTrackingRasterInvalidations();
}

GraphicsLayer::~GraphicsLayer() {
  CcLayer()->ClearClient();
  SetContentsLayer(nullptr);

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

IntRect GraphicsLayer::VisualRect() const {
  DCHECK(layer_state_);
  return IntRect(layer_state_->offset, IntSize(Size()));
}

void GraphicsLayer::AppendAdditionalInfoAsJSON(LayerTreeFlags flags,
                                               const cc::Layer& layer,
                                               JSONObject& json) const {
  // Only the primary layer associated with GraphicsLayer adds additional
  // information.  Other layer state, such as raster invalidations, don't
  // disambiguate between specific layers.
  if (&layer != layer_.get())
    return;

  if ((flags & kLayerTreeIncludesPaintInvalidations) &&
      Client().IsTrackingRasterInvalidations() &&
      GetRasterInvalidationTracking()) {
    GetRasterInvalidationTracking()->AsJSON(&json);
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

#if DCHECK_IS_ON()
  if (HasLayerState() && DrawsContent() &&
      (flags & kLayerTreeIncludesPaintRecords))
    json.SetValue("paintRecord", RecordAsJSON(*CapturePaintRecord()));
#endif
}

void GraphicsLayer::SetHasWillChangeTransformHint(
    bool has_will_change_transform) {
  CcLayer()->SetHasWillChangeTransformHint(has_will_change_transform);
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

  size_t list_size = new_children.size();
  for (size_t i = 0; i < list_size; ++i)
    AddChildInternal(new_children[i]);

  UpdateChildList();

  return true;
}

void GraphicsLayer::AddChildInternal(GraphicsLayer* child_layer) {
  DCHECK_NE(child_layer, this);

  if (child_layer->Parent())
    child_layer->RemoveFromParent();

  child_layer->SetParent(this);
  children_.push_back(child_layer);

  // Don't call updateChildList here, this function is used in cases where it
  // should not be called until all children are processed.
}

void GraphicsLayer::AddChild(GraphicsLayer* child_layer) {
  AddChildInternal(child_layer);
  UpdateChildList();
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
  CcLayer()->SetFiltersOrigin(FloatPoint() -
                              FloatSize(offset_from_layout_object_));

  // If the compositing layer offset changes, we need to repaint.
  SetNeedsDisplay();
}

IntRect GraphicsLayer::InterestRect() {
  return previous_interest_rect_;
}

bool GraphicsLayer::PaintRecursively() {
  Vector<GraphicsLayer*> repainted_layers;
  PaintRecursivelyInternal(repainted_layers);

  // Notify the controllers that the artifact has been pushed and some
  // lifecycle state can be freed (such as raster invalidations).
  for (auto* layer : repainted_layers) {
#if DCHECK_IS_ON()
    if (VLOG_IS_ON(2))
      LOG(ERROR) << "FinishCycle for GraphicsLayer: " << layer->DebugName();
#endif
    layer->GetPaintController().FinishCycle();
  }
  return !repainted_layers.IsEmpty();
}

void GraphicsLayer::PaintRecursivelyInternal(
    Vector<GraphicsLayer*>& repainted_layers) {
  if (client_.PaintBlockedByDisplayLockIncludingAncestors(
          DisplayLockContextLifecycleTarget::kSelf)) {
    return;
  }

  if (PaintsContentOrHitTest()) {
    if (Paint())
      repainted_layers.push_back(this);
  }

  if (MaskLayer())
    MaskLayer()->PaintRecursivelyInternal(repainted_layers);

  for (auto* child : Children())
    child->PaintRecursivelyInternal(repainted_layers);
}

bool GraphicsLayer::Paint(GraphicsContext::DisabledMode disabled_mode) {
#if !DCHECK_IS_ON()
  // TODO(crbug.com/853096): Investigate why we can ever reach here without
  // a valid layer state. Seems to only happen on Android builds.
  if (!layer_state_)
    return false;
#endif

  if (PaintWithoutCommit(disabled_mode)) {
    GetPaintController().CommitNewDisplayItems();
    UpdateSafeOpaqueBackgroundColor();
  } else if (!needs_check_raster_invalidation_) {
    return false;
  }

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(2)) {
    LOG(ERROR) << "Painted GraphicsLayer: " << DebugName()
               << " interest_rect=" << InterestRect().ToString();
  }
#endif

  DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();
  // Generate raster invalidations for SPv1.
  IntRect layer_bounds(layer_state_->offset, IntSize(Size()));
  EnsureRasterInvalidator().Generate(
      GetPaintController().GetPaintArtifactShared(), layer_bounds,
      layer_state_->state, VisualRectSubpixelOffset(), this);

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      PaintsContentOrHitTest()) {
    auto& tracking = EnsureRasterInvalidator().EnsureTracking();
    tracking.CheckUnderInvalidations(DebugName(), CapturePaintRecord(),
                                     InterestRect());
    if (auto record = tracking.UnderInvalidationRecord()) {
      // Add the under-invalidation overlay onto the painted result.
      GetPaintController().AppendDebugDrawingAfterCommit(std::move(record),
                                                         layer_state_->state);
      // Ensure the compositor will raster the under-invalidation overlay.
      CcLayer()->SetNeedsDisplay();
    }
  }

  needs_check_raster_invalidation_ = false;
  return true;
}

void GraphicsLayer::UpdateSafeOpaqueBackgroundColor() {
  if (!DrawsContent())
    return;
  // Copy the first chunk's safe opaque background color over to the cc::Layer.
  const auto& chunks = GetPaintController().GetPaintArtifact().PaintChunks();
  CcLayer()->SetSafeOpaqueBackgroundColor(
      chunks.size() ? chunks[0].safe_opaque_background_color : SK_ColorWHITE);
}

bool GraphicsLayer::PaintWithoutCommitForTesting(
    const base::Optional<IntRect>& interest_rect) {
  return PaintWithoutCommit(GraphicsContext::kNothingDisabled,
                            base::OptionalOrNullptr(interest_rect));
}

bool GraphicsLayer::PaintWithoutCommit(
    GraphicsContext::DisabledMode disabled_mode,
    const IntRect* interest_rect) {
  DCHECK(PaintsContentOrHitTest());

  if (client_.ShouldThrottleRendering() || client_.IsUnderSVGHiddenContainer())
    return false;

  IntRect new_interest_rect;
  if (!interest_rect) {
    new_interest_rect =
        client_.ComputeInterestRect(this, previous_interest_rect_);
    interest_rect = &new_interest_rect;
  }

  if (!GetPaintController().SubsequenceCachingIsDisabled() &&
      !client_.NeedsRepaint(*this) &&
      !GetPaintController().CacheIsAllInvalid() &&
      previous_interest_rect_ == *interest_rect) {
    GetPaintController().UpdateUMACountsOnFullyCached();
    return false;
  }

  GraphicsContext context(GetPaintController(), disabled_mode, nullptr);
  DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();
  GetPaintController().UpdateCurrentPaintChunkProperties(base::nullopt,
                                                         layer_state_->state);

  previous_interest_rect_ = *interest_rect;
  client_.PaintContents(this, context, painting_phase_, *interest_rect);
  return true;
}

void GraphicsLayer::UpdateChildList() {
  // cc::Layers are created in PaintArtifactCompositor.
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::UpdateLayerIsDrawable() {
  // For the rest of the accelerated compositor code, there is no reason to make
  // a distinction between drawsContent and contentsVisible. So, for
  // m_layer->layer(), these two flags are combined here. |m_contentsLayer|
  // shouldn't receive the drawsContent flag, so it is only given
  // contentsVisible.

  CcLayer()->SetIsDrawable(draws_content_ && contents_visible_);
  if (cc::Layer* contents_layer = ContentsLayerIfRegistered())
    contents_layer->SetIsDrawable(contents_visible_);

  if (draws_content_)
    CcLayer()->SetNeedsDisplay();
}

void GraphicsLayer::UpdateContentsRect() {
  cc::Layer* contents_layer = ContentsLayerIfRegistered();
  if (!contents_layer)
    return;

  contents_layer->SetPosition(
      FloatPoint(contents_rect_.X(), contents_rect_.Y()));
  if (!image_layer_) {
    contents_layer->SetBounds(static_cast<gfx::Size>(contents_rect_.Size()));
  } else {
    DCHECK_EQ(image_layer_.get(), contents_layer_);
    // The image_layer_ has fixed bounds, and we apply bounds changes via the
    // transform instead. Since we never change the transform on the
    // |image_layer_| otherwise, we can assume it is identity and just apply
    // the bounds to it directly. Same thing for transform origin.
    DCHECK(image_layer_->transform_origin() == gfx::Point3F());

    if (contents_rect_.Size().IsEmpty() || image_size_.IsEmpty()) {
      image_layer_->SetTransform(gfx::Transform());
      contents_layer->SetBounds(static_cast<gfx::Size>(contents_rect_.Size()));
    } else {
      gfx::Transform image_transform;
      image_transform.Scale(
          static_cast<float>(contents_rect_.Width()) / image_size_.Width(),
          static_cast<float>(contents_rect_.Height()) / image_size_.Height());
      image_layer_->SetTransform(image_transform);
      image_layer_->SetBounds(static_cast<gfx::Size>(image_size_));
    }
  }
}

static HashSet<int>* g_registered_layer_set;

void GraphicsLayer::RegisterContentsLayer(cc::Layer* layer) {
  if (!g_registered_layer_set)
    g_registered_layer_set = new HashSet<int>;
  CHECK(!g_registered_layer_set->Contains(layer->id()));
  g_registered_layer_set->insert(layer->id());
}

void GraphicsLayer::UnregisterContentsLayer(cc::Layer* layer) {
  DCHECK(g_registered_layer_set);
  CHECK(g_registered_layer_set->Contains(layer->id()));
  g_registered_layer_set->erase(layer->id());
}

void GraphicsLayer::SetContentsTo(cc::Layer* layer,
                                  bool prevent_contents_opaque_changes) {
  bool children_changed = false;
  if (layer) {
    DCHECK(g_registered_layer_set);
    CHECK(g_registered_layer_set->Contains(layer->id()));
    if (contents_layer_id_ != layer->id()) {
      SetupContentsLayer(layer);
      children_changed = true;
    }
    UpdateContentsRect();
    prevent_contents_opaque_changes_ = prevent_contents_opaque_changes;
  } else {
    if (contents_layer_) {
      children_changed = true;

      // The old contents layer will be removed via updateChildList.
      SetContentsLayer(nullptr);
    }
  }

  if (children_changed)
    UpdateChildList();
}

void GraphicsLayer::SetupContentsLayer(cc::Layer* contents_layer) {
  DCHECK(contents_layer);
  SetContentsLayer(contents_layer);

  contents_layer_->SetTransformOrigin(FloatPoint3D());
  contents_layer_->SetUseParentBackfaceVisibility(true);

  // It is necessary to call SetDrawsContent() as soon as we receive the new
  // contents_layer, for the correctness of early exit conditions in
  // SetDrawsContent() and SetContentsVisible().
  contents_layer_->SetIsDrawable(contents_visible_);
  contents_layer_->SetHitTestable(contents_visible_);
}

void GraphicsLayer::ClearContentsLayerIfUnregistered() {
  if (!contents_layer_id_ ||
      g_registered_layer_set->Contains(contents_layer_id_))
    return;

  SetContentsLayer(nullptr);
}

void GraphicsLayer::SetContentsLayer(cc::Layer* contents_layer) {
  contents_layer_ = contents_layer;
  if (!contents_layer_) {
    contents_layer_id_ = 0;
    return;
  }
  contents_layer_id_ = contents_layer_->id();
}

cc::Layer* GraphicsLayer::ContentsLayerIfRegistered() {
  ClearContentsLayerIfUnregistered();
  return contents_layer_;
}

RasterInvalidator& GraphicsLayer::EnsureRasterInvalidator() {
  if (!raster_invalidator_) {
    raster_invalidator_ =
        std::make_unique<RasterInvalidator>(base::BindRepeating(
            &GraphicsLayer::SetNeedsDisplayInRect, base::Unretained(this)));
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
  if (layer->id() == contents_layer_id_)
    return "ContentsLayer for " + client_.DebugName(this);

  if (layer == layer_.get())
    return client_.DebugName(this);

  NOTREACHED();
  return "";
}

void GraphicsLayer::SetPosition(const gfx::PointF& point) {
  CcLayer()->SetPosition(point);
}

const gfx::PointF& GraphicsLayer::GetPosition() const {
  return CcLayer()->position();
}

const gfx::Size& GraphicsLayer::Size() const {
  return CcLayer()->bounds();
}

void GraphicsLayer::SetSize(const gfx::Size& size) {
  DCHECK(size.width() >= 0 && size.height() >= 0);

  if (size == CcLayer()->bounds())
    return;

  Invalidate(PaintInvalidationReason::kIncremental);  // as DisplayItemClient.

  CcLayer()->SetBounds(size);
  // Note that we don't resize m_contentsLayer. It's up the caller to do that.
}


bool GraphicsLayer::MasksToBounds() const {
  return CcLayer()->masks_to_bounds();
}

void GraphicsLayer::SetMasksToBounds(bool masks_to_bounds) {
  CcLayer()->SetMasksToBounds(masks_to_bounds);
}

void GraphicsLayer::SetDrawsContent(bool draws_content) {
  // NOTE: This early-exit is only correct because we also properly call
  // cc::Layer::SetIsDrawable() whenever |contents_layer_| is set to a new
  // layer in SetupContentsLayer().
  if (draws_content == draws_content_)
    return;

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

RGBA32 GraphicsLayer::BackgroundColor() const {
  return CcLayer()->background_color();
}

void GraphicsLayer::SetBackgroundColor(RGBA32 color) {
  CcLayer()->SetBackgroundColor(color);
}

bool GraphicsLayer::ContentsOpaque() const {
  return CcLayer()->contents_opaque();
}

void GraphicsLayer::SetContentsOpaque(bool opaque) {
  CcLayer()->SetContentsOpaque(opaque);
  ClearContentsLayerIfUnregistered();
  if (contents_layer_ && !prevent_contents_opaque_changes_)
    contents_layer_->SetContentsOpaque(opaque);
}

void GraphicsLayer::SetMaskLayer(GraphicsLayer* mask_layer) {
  if (mask_layer == mask_layer_)
    return;

  mask_layer_ = mask_layer;
}

bool GraphicsLayer::BackfaceVisibility() const {
  return CcLayer()->double_sided();
}

void GraphicsLayer::SetHitTestable(bool should_hit_test) {
  if (hit_testable_ == should_hit_test)
    return;
  hit_testable_ = should_hit_test;
  CcLayer()->SetHitTestable(should_hit_test);
}

void GraphicsLayer::SetContentsNeedsDisplay() {
  if (cc::Layer* contents_layer = ContentsLayerIfRegistered()) {
    contents_layer->SetNeedsDisplay();
    TrackRasterInvalidation(*this, contents_rect_,
                            PaintInvalidationReason::kFullLayer);
  }
}

void GraphicsLayer::SetNeedsDisplay() {
  if (!PaintsContentOrHitTest())
    return;

  CcLayer()->SetNeedsDisplay();

  // Invalidate the paint controller if it exists, but don't bother creating one
  // if not.
  if (paint_controller_)
    paint_controller_->InvalidateAll();

  if (raster_invalidator_)
    raster_invalidator_->ClearOldStates();

  TrackRasterInvalidation(*this, IntRect(IntPoint(), IntSize(Size())),
                          PaintInvalidationReason::kFullLayer);
}

void GraphicsLayer::SetNeedsDisplayInRect(const IntRect& rect) {
  DCHECK(PaintsContentOrHitTest());
  CcLayer()->SetNeedsDisplayRect(rect);
}

void GraphicsLayer::SetContentsRect(const IntRect& rect) {
  if (rect == contents_rect_)
    return;

  contents_rect_ = rect;
  UpdateContentsRect();
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::SetContentsToImage(
    Image* image,
    Image::ImageDecodingMode decode_mode,
    RespectImageOrientationEnum respect_image_orientation) {
  PaintImage paint_image;
  if (image)
    paint_image = image->PaintImageForCurrentFrame();

  ImageOrientation image_orientation = kOriginTopLeft;
  SkMatrix matrix;
  if (paint_image && image->IsBitmapImage() &&
      respect_image_orientation == kRespectImageOrientation) {
    image_orientation = ToBitmapImage(image)->CurrentFrameOrientation();
    image_size_ = IntSize(paint_image.width(), paint_image.height());
    if (image_orientation.UsesWidthAsHeight())
      image_size_ = image_size_.TransposedSize();
    auto affine =
        image_orientation.TransformFromDefault(FloatSize(image_size_));
    auto transform = affine.ToTransformationMatrix();
    matrix = TransformationMatrix::ToSkMatrix44(transform);
  } else if (paint_image) {
    matrix = SkMatrix::I();
    image_size_ = IntSize(paint_image.width(), paint_image.height());
  } else {
    matrix = SkMatrix::I();
    image_size_ = IntSize();
  }

  if (paint_image) {
    paint_image =
        PaintImageBuilder::WithCopy(std::move(paint_image))
            .set_decoding_mode(Image::ToPaintImageDecodingMode(decode_mode))
            .TakePaintImage();
    if (!image_layer_) {
      image_layer_ = cc::PictureImageLayer::Create();
      RegisterContentsLayer(image_layer_.get());
    }
    image_layer_->SetImage(std::move(paint_image), matrix,
                           image_orientation.UsesWidthAsHeight());
    // Image layers can not be marked as opaque due to crbug.com/870857.
    image_layer_->SetContentsOpaque(false);
    UpdateContentsRect();
  } else if (image_layer_) {
    UnregisterContentsLayer(image_layer_.get());
    image_layer_ = nullptr;
  }

  SetContentsTo(image_layer_.get(),
                /*prevent_contents_opaque_changes=*/true);
}

cc::PictureLayer* GraphicsLayer::CcLayer() const {
  return layer_.get();
}

void GraphicsLayer::SetFilterQuality(SkFilterQuality filter_quality) {
  if (image_layer_)
    image_layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
}

void GraphicsLayer::SetPaintingPhase(GraphicsLayerPaintingPhase phase) {
  if (painting_phase_ == phase)
    return;
  painting_phase_ = phase;
  SetNeedsDisplay();
}

PaintController& GraphicsLayer::GetPaintController() const {
  CHECK(PaintsContentOrHitTest());
  if (!paint_controller_)
    paint_controller_ = std::make_unique<PaintController>();
  return *paint_controller_;
}

void GraphicsLayer::SetElementId(const CompositorElementId& id) {
  if (cc::Layer* layer = CcLayer())
    layer->SetElementId(id);
}

sk_sp<PaintRecord> GraphicsLayer::CapturePaintRecord() const {
  DCHECK(PaintsContentOrHitTest());

  if (client_.ShouldThrottleRendering())
    return sk_sp<PaintRecord>(new PaintRecord);

  if (client_.IsUnderSVGHiddenContainer())
    return sk_sp<PaintRecord>(new PaintRecord);

  if (client_.PaintBlockedByDisplayLockIncludingAncestors(
          DisplayLockContextLifecycleTarget::kSelf)) {
    return sk_sp<PaintRecord>(new PaintRecord);
  }

  FloatRect bounds((IntRect(IntPoint(), IntSize(Size()))));
  GraphicsContext graphics_context(GetPaintController());
  graphics_context.BeginRecording(bounds);
  DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();
  GetPaintController().GetPaintArtifact().Replay(
      graphics_context, layer_state_->state, layer_state_->offset);
  return graphics_context.EndRecording();
}

void GraphicsLayer::SetLayerState(const PropertyTreeState& layer_state,
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

  if (auto* layer = CcLayer())
    layer->SetSubtreePropertyChanged();
  client_.GraphicsLayersDidChange();
}

void GraphicsLayer::SetContentsLayerState(const PropertyTreeState& layer_state,
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

scoped_refptr<cc::DisplayItemList> GraphicsLayer::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  TRACE_EVENT0("blink,benchmark", "GraphicsLayer::PaintContents");

  PaintController& paint_controller = GetPaintController();
  paint_controller.SetDisplayItemConstructionIsDisabled(
      painting_control == DISPLAY_LIST_CONSTRUCTION_DISABLED);
  paint_controller.SetSubsequenceCachingIsDisabled(
      painting_control == SUBSEQUENCE_CACHING_DISABLED);

  if (painting_control == PARTIAL_INVALIDATION)
    client_.InvalidateTargetElementForTesting();

  // We also disable caching when Painting or Construction are disabled. In both
  // cases we would like to compare assuming the full cost of recording, not the
  // cost of re-using cached content.
  if (painting_control == DISPLAY_LIST_CACHING_DISABLED ||
      painting_control == DISPLAY_LIST_PAINTING_DISABLED ||
      painting_control == DISPLAY_LIST_CONSTRUCTION_DISABLED)
    paint_controller.InvalidateAll();

  GraphicsContext::DisabledMode disabled_mode =
      GraphicsContext::kNothingDisabled;
  if (painting_control == DISPLAY_LIST_PAINTING_DISABLED ||
      painting_control == DISPLAY_LIST_CONSTRUCTION_DISABLED)
    disabled_mode = GraphicsContext::kFullyDisabled;

  // Anything other than PAINTING_BEHAVIOR_NORMAL is for testing. In non-testing
  // scenarios, it is an error to call GraphicsLayer::Paint. Actual painting
  // occurs in LocalFrameView::PaintTree() which calls GraphicsLayer::Paint();
  // this method merely copies the painted output to the cc::DisplayItemList.
  if (painting_control != PAINTING_BEHAVIOR_NORMAL)
    Paint(disabled_mode);

  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();

  DCHECK(layer_state_) << "No layer state for GraphicsLayer: " << DebugName();
  PaintChunksToCcLayer::ConvertInto(
      GetPaintController().PaintChunks(), layer_state_->state,
      gfx::Vector2dF(layer_state_->offset.X(), layer_state_->offset.Y()),
      VisualRectSubpixelOffset(),
      paint_controller.GetPaintArtifact().GetDisplayItemList(), *display_list);

  paint_controller.SetDisplayItemConstructionIsDisabled(false);
  paint_controller.SetSubsequenceCachingIsDisabled(false);

  display_list->Finalize();
  return display_list;
}

size_t GraphicsLayer::GetApproximateUnsharedMemoryUsage() const {
  size_t result = sizeof(*this);
  result += GetPaintController().ApproximateUnsharedMemoryUsage();
  if (raster_invalidator_)
    result += raster_invalidator_->ApproximateUnsharedMemoryUsage();
  return result;
}

// Subpixel offset for visual rects which excluded composited layer's subpixel
// accumulation during paint invalidation.
// See PaintInvalidator::ExcludeCompositedLayerSubpixelAccumulation().
FloatSize GraphicsLayer::VisualRectSubpixelOffset() const {
  if (GetCompositingReasons() & CompositingReason::kComboAllDirectReasons)
    return FloatSize(client_.SubpixelAccumulation());
  return FloatSize();
}

}  // namespace blink
