// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "ui/gfx/color_utils.h"

namespace blink {

void PaintChunker::Finish() {
  FinalizeLastChunkProperties();
#if DCHECK_IS_ON()
  finished_ = true;
#endif
}

void PaintChunker::MarkClientForValidation(const DisplayItemClient& client) {
  CheckNotFinished();
  if (clients_to_validate_ && !client.IsMarkedForValidation()) {
    clients_to_validate_->push_back(&client);
    client.MarkForValidation();
  }
}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PropertyTreeStateOrAlias& properties) {
  CheckNotFinished();
  if (current_properties_ != properties) {
    next_chunk_id_ = std::nullopt;
    current_properties_ = properties;
  }
}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id& chunk_id,
    const DisplayItemClient& client,
    const PropertyTreeStateOrAlias& properties) {
  CheckNotFinished();
  // If properties are the same, continue to use the previously set
  // |next_chunk_id_| because the id of the outer painting is likely to be
  // more stable to reduce invalidation because of chunk id changes.
  if (!next_chunk_id_ || current_properties_ != properties)
    next_chunk_id_.emplace(chunk_id, client);
  current_properties_ = properties;
}

void PaintChunker::AppendByMoving(PaintChunk&& chunk) {
  FinalizeLastChunkProperties();
  wtf_size_t next_chunk_begin_index =
      chunks_.empty() ? 0 : chunks_.back().end_index;
  chunks_.emplace_back(next_chunk_begin_index, std::move(chunk));
}

bool PaintChunker::WillCreateNewChunk() const {
  return will_force_new_chunk_ ||
         current_properties_ != chunks_.back().properties;
}

bool PaintChunker::EnsureCurrentChunk(const PaintChunk::Id& id,
                                      const DisplayItemClient& client) {
#if DCHECK_IS_ON()
  CheckNotFinished();
  // If this DCHECK is hit we are missing a call to update the properties.
  // See: ScopedPaintChunkProperties.
  // At this point we should have all of the properties given to us.
  DCHECK(current_properties_.IsInitialized());
#endif

  if (WillCreateNewChunk()) {
    if (!next_chunk_id_) {
      next_chunk_id_.emplace(id, client);
    }
    FinalizeLastChunkProperties();
    wtf_size_t begin = chunks_.empty() ? 0 : chunks_.back().end_index;
    MarkClientForValidation(next_chunk_id_->second);
    chunks_.emplace_back(begin, begin, next_chunk_id_->second,
                         next_chunk_id_->first, current_properties_,
                         current_effectively_invisible_);
    next_chunk_id_ = std::nullopt;
    will_force_new_chunk_ = false;
    return true;
  }
  return false;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItemClient& client,
                                             const DisplayItem& item) {
  CheckNotFinished();
  bool item_forces_new_chunk = item.IsForeignLayer() || item.IsScrollbar();
  if (item_forces_new_chunk) {
    SetWillForceNewChunk();
  }
  bool created_new_chunk = EnsureCurrentChunk(item.GetId(), client);
  auto& chunk = chunks_.back();
  chunk.end_index++;

  // Normally the display item's visual rect should be covered by previous
  // hit test rects, or it's treated as not hit-testable.
  UnionBounds(item.VisualRect(), cc::HitTestOpaqueness::kTransparent);
  if (item.DrawsContent())
    chunk.drawable_bounds.Union(item.VisualRect());

  ProcessBackgroundColorCandidate(item);

  if (const auto* drawing = DynamicTo<DrawingDisplayItem>(item)) {
    chunk.rect_known_to_be_opaque = gfx::MaximumCoveredRect(
        chunk.rect_known_to_be_opaque, drawing->RectKnownToBeOpaque());
    if (chunk.text_known_to_be_on_opaque_background) {
      if (drawing->GetPaintRecord().has_draw_text_ops()) {
        chunk.has_text = true;
        chunk.text_known_to_be_on_opaque_background =
            chunk.rect_known_to_be_opaque.Contains(item.VisualRect());
      }
    } else {
      // text_known_to_be_on_opaque_background should be initially true before
      // we see any text.
      DCHECK(chunk.has_text);
    }
  } else if (const auto* scrollbar = DynamicTo<ScrollbarDisplayItem>(item)) {
    if (scrollbar->IsOpaque())
      chunk.rect_known_to_be_opaque = item.VisualRect();
  }

  chunk.raster_effect_outset =
      std::max(chunk.raster_effect_outset, item.GetRasterEffectOutset());

  // When forcing a new chunk, we still need to force new chunk for the next
  // display item. Otherwise reset force_new_chunk_ to false.
  DCHECK(!will_force_new_chunk_);
  if (item_forces_new_chunk) {
    DCHECK(created_new_chunk);
    SetWillForceNewChunk();
  }

  return created_new_chunk;
}

bool PaintChunker::AddHitTestDataToCurrentChunk(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const gfx::Rect& rect,
    TouchAction touch_action,
    bool blocking_wheel,
    cc::HitTestOpaqueness hit_test_opaqueness) {
  CheckNotFinished();
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  UnionBounds(rect, hit_test_opaqueness);
  auto& chunk = chunks_.back();
  if (touch_action != TouchAction::kAuto) {
    auto& touch_action_rects = chunk.EnsureHitTestData().touch_action_rects;
    if (touch_action_rects.empty() ||
        !touch_action_rects.back().rect.Contains(rect) ||
        touch_action_rects.back().allowed_touch_action != touch_action) {
      touch_action_rects.push_back(TouchActionRect{rect, touch_action});
    }
  }
  if (blocking_wheel) {
    auto& wheel_event_rects = chunk.EnsureHitTestData().wheel_event_rects;
    if (wheel_event_rects.empty() || !wheel_event_rects.back().Contains(rect)) {
      wheel_event_rects.push_back(rect);
    }
  }
  return created_new_chunk;
}

bool PaintChunker::CurrentChunkIsNonEmptyAndTransparentToHitTest() const {
  CheckNotFinished();
  if (WillCreateNewChunk()) {
    return false;
  }
  const auto& chunk = chunks_.back();
  return !chunk.bounds.IsEmpty() &&
         chunk.hit_test_opaqueness == cc::HitTestOpaqueness::kTransparent;
}

bool PaintChunker::AddRegionCaptureDataToCurrentChunk(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const RegionCaptureCropId& crop_id,
    const gfx::Rect& rect) {
  CheckNotFinished();
  DCHECK(!crop_id->is_zero());
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  auto& chunk = chunks_.back();
  if (!chunk.region_capture_data) {
    chunk.region_capture_data = MakeGarbageCollected<RegionCaptureData>();
  }
  chunk.region_capture_data->map.insert_or_assign(crop_id, std::move(rect));
  return created_new_chunk;
}

void PaintChunker::AddSelectionToCurrentChunk(
    std::optional<PaintedSelectionBound> start,
    std::optional<PaintedSelectionBound> end,
    String debug_info) {
  // We should have painted the selection when calling this method.
  CheckNotFinished();
  DCHECK(!chunks_.empty());

  auto& chunk = chunks_.back();

#if DCHECK_IS_ON()
  gfx::Rect bounds_rect = chunk.bounds;

  // In rare cases in the wild, the bounds_rect is 1 pixel off from the
  // edge_rect below. We were unable to find the root cause, or to reproduce
  // this locally, so we're relaxing the DCHECK. See https://crbug.com/1441243.
  bounds_rect.Outset(1);

  if (start) {
    gfx::Rect edge_rect = gfx::BoundingRect(start->edge_start, start->edge_end);
    DCHECK(bounds_rect.Contains(edge_rect))
        << bounds_rect.ToString() << " does not contain "
        << edge_rect.ToString() << ", original bounds: " << debug_info;
  }

  if (end) {
    gfx::Rect edge_rect = gfx::BoundingRect(end->edge_start, end->edge_end);
    DCHECK(bounds_rect.Contains(edge_rect))
        << bounds_rect.ToString() << " does not contain "
        << edge_rect.ToString() << ", original bounds: " << debug_info;
  }
#endif

  LayerSelectionData& selection_data = chunk.EnsureLayerSelectionData();
  if (start) {
    DCHECK(!selection_data.start);
    selection_data.start = start;
  }

  if (end) {
    DCHECK(!selection_data.end);
    selection_data.end = end;
  }
}

void PaintChunker::RecordAnySelectionWasPainted() {
  CheckNotFinished();
  DCHECK(!chunks_.empty());

  auto& chunk = chunks_.back();
  LayerSelectionData& selection_data = chunk.EnsureLayerSelectionData();
  selection_data.any_selection_was_painted = true;
}

void PaintChunker::CreateScrollHitTestChunk(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const TransformPaintPropertyNode* scroll_translation,
    const gfx::Rect& scroll_hit_test_rect,
    cc::HitTestOpaqueness hit_test_opaqueness,
    const gfx::Rect& scrolling_contents_cull_rect) {
#if DCHECK_IS_ON()
  CheckNotFinished();
  if (id.type == DisplayItem::Type::kResizerScrollHitTest ||
      id.type == DisplayItem::Type::kWebPluginHitTest ||
      id.type == DisplayItem::Type::kScrollbarHitTest) {
    // Resizer, plugin, and scrollbar hit tests are only used to prevent
    // composited scrolling and should not have a scroll offset node.
    DCHECK(!scroll_translation);
  } else if (id.type == DisplayItem::Type::kScrollHitTest) {
    // We might not have a scroll_translation node.  This indicates that
    // (due to complex pointer-events cases) we need to do main thread
    // scroll hit testing for this scroller.
    if (scroll_translation) {
      // The scroll offset transform node should have an associated scroll node.
      DCHECK(scroll_translation->ScrollNode());
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }
#endif

  SetWillForceNewChunk();
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  DCHECK(created_new_chunk);

  auto& chunk = chunks_.back();
  UnionBounds(scroll_hit_test_rect, hit_test_opaqueness);
  auto& hit_test_data = chunk.EnsureHitTestData();
  hit_test_data.scroll_translation = scroll_translation;
  hit_test_data.scroll_hit_test_rect = scroll_hit_test_rect;
  hit_test_data.scrolling_contents_cull_rect = scrolling_contents_cull_rect;
  SetWillForceNewChunk();
}

void PaintChunker::UnionBounds(const gfx::Rect& rect,
                               cc::HitTestOpaqueness hit_test_opaqueness) {
  CheckNotFinished();
  auto& chunk = chunks_.back();
  chunk.hit_test_opaqueness = cc::UnionHitTestOpaqueness(
      chunk.bounds, chunk.hit_test_opaqueness, rect, hit_test_opaqueness);
  chunk.bounds.Union(rect);
}

void PaintChunker::ProcessBackgroundColorCandidate(const DisplayItem& item) {
  CheckNotFinished();
  // If this paints the background and it's larger than our current candidate,
  // set the candidate to be this item.
  auto& chunk = chunks_.back();
  if (item.IsDrawing() && item.DrawsContent()) {
    PaintChunk::BackgroundColorInfo item_background_color =
        To<DrawingDisplayItem>(item).BackgroundColor();
    float min_background_area = kMinBackgroundColorCoverageRatio *
                                chunk.bounds.width() * chunk.bounds.height();
    if (item_background_color.area >= chunk.background_color.area ||
        item_background_color.area >= min_background_area) {
      if (chunk.background_color.area >= min_background_area &&
          !item_background_color.color.isOpaque()) {
        chunk.background_color.area = item_background_color.area;
        chunk.background_color.color =
            SkColor4f::FromColor(color_utils::GetResultingPaintColor(
                item_background_color.color.toSkColor(),
                chunk.background_color.color.toSkColor()));
      } else {
        chunk.background_color = item_background_color;
      }
    }
  }
}

void PaintChunker::FinalizeLastChunkProperties() {
  CheckNotFinished();
  if (chunks_.empty() || chunks_.back().is_moved_from_cached_subsequence) {
    return;
  }

  auto& chunk = chunks_.back();
  if (chunk.size() > 1 ||
      chunk.background_color.area !=
          static_cast<float>(chunk.bounds.width()) * chunk.bounds.height()) {
    chunk.background_color.is_solid_color = false;
  }
}

}  // namespace blink
