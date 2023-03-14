// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {

void PaintChunker::ResetChunks(Vector<PaintChunk>* chunks) {
  if (chunks_) {
    FinalizeLastChunkProperties();
    SetWillForceNewChunk(true);
    current_properties_ = PropertyTreeState::Uninitialized();
  }
  chunks_ = chunks;
#if DCHECK_IS_ON()
  DCHECK(!chunks || chunks->empty());
  DCHECK(IsInInitialState());
#endif
}

#if DCHECK_IS_ON()
bool PaintChunker::IsInInitialState() const {
  if (current_properties_ != PropertyTreeState::Uninitialized())
    return false;
  DCHECK(will_force_new_chunk_);
  DCHECK(!chunks_ || chunks_->empty());
  return true;
}
#endif

void PaintChunker::StartMarkingClientsForValidation(
    HeapVector<Member<const DisplayItemClient>>& clients_to_validate) {
#if DCHECK_IS_ON()
  DCHECK(IsInInitialState());
#endif
  DCHECK(!clients_to_validate_);
  clients_to_validate_ = &clients_to_validate;
}

void PaintChunker::MarkClientForValidation(const DisplayItemClient& client) {
  if (clients_to_validate_ && !client.IsMarkedForValidation()) {
    clients_to_validate_->push_back(&client);
    client.MarkForValidation();
  }
}

void PaintChunker::StopMarkingClientsForValidation() {
  clients_to_validate_ = nullptr;
}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PropertyTreeStateOrAlias& properties) {
  if (current_properties_ != properties) {
    next_chunk_id_ = absl::nullopt;
    current_properties_ = properties;
  }
}

void PaintChunker::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id& chunk_id,
    const DisplayItemClient& client,
    const PropertyTreeStateOrAlias& properties) {
  // If properties are the same, continue to use the previously set
  // |next_chunk_id_| because the id of the outer painting is likely to be
  // more stable to reduce invalidation because of chunk id changes.
  if (!next_chunk_id_ || current_properties_ != properties)
    next_chunk_id_.emplace(chunk_id, client);
  current_properties_ = properties;
}

void PaintChunker::AppendByMoving(PaintChunk&& chunk) {
  DCHECK(chunks_);
  FinalizeLastChunkProperties();
  wtf_size_t next_chunk_begin_index =
      chunks_->empty() ? 0 : chunks_->back().end_index;
  chunks_->emplace_back(next_chunk_begin_index, std::move(chunk));
}

bool PaintChunker::EnsureCurrentChunk(const PaintChunk::Id& id,
                                      const DisplayItemClient& client) {
#if DCHECK_IS_ON()
  DCHECK(chunks_);
  // If this DCHECKs are hit we are missing a call to update the properties.
  // See: ScopedPaintChunkProperties.
  DCHECK(!IsInInitialState());
  // At this point we should have all of the properties given to us.
  DCHECK(current_properties_.IsInitialized());
#endif

  if (WillForceNewChunk() ||
      current_properties_ != chunks_->back().properties) {
    if (!next_chunk_id_) {
      next_chunk_id_.emplace(id, client);
    }
    FinalizeLastChunkProperties();
    wtf_size_t begin = chunks_->empty() ? 0 : chunks_->back().end_index;
    MarkClientForValidation(next_chunk_id_->second);
    chunks_->emplace_back(begin, begin, next_chunk_id_->second,
                          next_chunk_id_->first, current_properties_,
                          current_effectively_invisible_);
    next_chunk_id_ = absl::nullopt;
    will_force_new_chunk_ = false;
    return true;
  }
  return false;
}

bool PaintChunker::IncrementDisplayItemIndex(const DisplayItemClient& client,
                                             const DisplayItem& item) {
  DCHECK(chunks_);

  bool item_forces_new_chunk = item.IsForeignLayer() || item.IsScrollbar();
  if (item_forces_new_chunk)
    SetWillForceNewChunk(true);

  bool created_new_chunk = EnsureCurrentChunk(item.GetId(), client);
  auto& chunk = chunks_->back();
  chunk.end_index++;

  chunk.bounds.Union(item.VisualRect());
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
    SetWillForceNewChunk(true);
  }

  return created_new_chunk;
}

bool PaintChunker::AddHitTestDataToCurrentChunk(const PaintChunk::Id& id,
                                                const DisplayItemClient& client,
                                                const gfx::Rect& rect,
                                                TouchAction touch_action,
                                                bool blocking_wheel) {
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  auto& chunk = chunks_->back();
  chunk.bounds.Union(rect);
  if (touch_action != TouchAction::kAuto) {
    chunk.EnsureHitTestData().touch_action_rects.push_back(
        TouchActionRect{rect, touch_action});
  }
  if (blocking_wheel)
    chunk.EnsureHitTestData().wheel_event_rects.push_back(rect);
  return created_new_chunk;
}

bool PaintChunker::AddRegionCaptureDataToCurrentChunk(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const RegionCaptureCropId& crop_id,
    const gfx::Rect& rect) {
  DCHECK(!crop_id->is_zero());
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  auto& chunk = chunks_->back();
  if (!chunk.region_capture_data) {
    chunk.region_capture_data = std::make_unique<RegionCaptureData>();
  }
  chunk.region_capture_data->insert_or_assign(crop_id, std::move(rect));
  return created_new_chunk;
}

void PaintChunker::AddSelectionToCurrentChunk(
    absl::optional<PaintedSelectionBound> start,
    absl::optional<PaintedSelectionBound> end) {
  // We should have painted the selection when calling this method.
  DCHECK(chunks_);
  DCHECK(!chunks_->empty());

  auto& chunk = chunks_->back();

#if DCHECK_IS_ON()
  if (start) {
    gfx::Rect edge_rect = gfx::BoundingRect(start->edge_start, start->edge_end);
    DCHECK(chunk.bounds.Contains(edge_rect));
  }

  if (end) {
    gfx::Rect edge_rect = gfx::BoundingRect(end->edge_start, end->edge_end);
    DCHECK(chunk.bounds.Contains(edge_rect));
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
  DCHECK(chunks_);
  DCHECK(!chunks_->empty());

  auto& chunk = chunks_->back();
  LayerSelectionData& selection_data = chunk.EnsureLayerSelectionData();
  selection_data.any_selection_was_painted = true;
}

void PaintChunker::CreateScrollHitTestChunk(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const TransformPaintPropertyNode* scroll_translation,
    const gfx::Rect& rect) {
#if DCHECK_IS_ON()
  if (id.type == DisplayItem::Type::kResizerScrollHitTest ||
      id.type == DisplayItem::Type::kPluginScrollHitTest ||
      id.type == DisplayItem::Type::kScrollbarHitTest) {
    // Resizer, plugin, and scrollbar hit tests are only used to prevent
    // composited scrolling and should not have a scroll offset node.
    DCHECK(!scroll_translation);
  } else if (id.type == DisplayItem::Type::kScrollHitTest) {
    DCHECK(scroll_translation);
    // The scroll offset transform node should have an associated scroll node.
    DCHECK(scroll_translation->ScrollNode());
  } else {
    NOTREACHED();
  }
#endif

  SetWillForceNewChunk(true);
  bool created_new_chunk = EnsureCurrentChunk(id, client);
  DCHECK(created_new_chunk);

  auto& chunk = chunks_->back();
  chunk.bounds.Union(rect);
  auto& hit_test_data = chunk.EnsureHitTestData();
  hit_test_data.scroll_translation = scroll_translation;
  hit_test_data.scroll_hit_test_rect = rect;
  if (id.type == DisplayItem::Type::kScrollbarHitTest ||
      id.type == DisplayItem::Type::kResizerScrollHitTest) {
    hit_test_data.touch_action_rects.push_back(
        TouchActionRect{rect, TouchAction::kNone});
  }
  SetWillForceNewChunk(true);
}

void PaintChunker::ProcessBackgroundColorCandidate(const DisplayItem& item) {
  // If this paints the background and it's larger than our current candidate,
  // set the candidate to be this item.
  auto& chunk = chunks_->back();
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
        if (auto filter = SkColorFilters::Blend(
                item_background_color.color, nullptr, SkBlendMode::kSrcOver)) {
          chunk.background_color.color = filter->filterColor4f(
              chunk.background_color.color, nullptr, nullptr);
        }
      } else {
        chunk.background_color = item_background_color;
      }
    }
  }
}

void PaintChunker::FinalizeLastChunkProperties() {
  DCHECK(chunks_);
  if (chunks_->empty() || chunks_->back().is_moved_from_cached_subsequence)
    return;

  auto& chunk = chunks_->back();
  if (chunk.size() > 1 ||
      chunk.background_color.area !=
          static_cast<float>(chunk.bounds.width()) * chunk.bounds.height()) {
    chunk.background_color.is_solid_color = false;
  }
}

}  // namespace blink
