// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

namespace blink {

void DisplayItemRasterInvalidator::Generate() {
  struct OldAndNewDisplayItems {
    // Union of visual rects of all old display items of the client.
    gfx::Rect old_visual_rect;
    // Union of visual rects of all new display items of the client.
    gfx::Rect new_visual_rect;
    PaintInvalidationReason reason = PaintInvalidationReason::kNone;
    DISALLOW_NEW();
  };
  HashMap<DisplayItemClientId, OldAndNewDisplayItems> clients_to_invalidate;

  Vector<bool> old_display_items_matched;
  old_display_items_matched.resize(old_display_items_.size());
  auto next_old_item_to_match = old_display_items_.begin();
  auto latest_cached_old_item = next_old_item_to_match;

  for (const auto& new_item : new_display_items_) {
    auto matched_old_item =
        MatchNewDisplayItemInOldChunk(new_item, next_old_item_to_match);
    if (matched_old_item == old_display_items_.end()) {
      if (new_item.DrawsContent()) {
        // Will invalidate for the new display item which doesn't match any old
        // display item.
        auto& value = clients_to_invalidate
                          .insert(new_item.ClientId(), OldAndNewDisplayItems())
                          .stored_value->value;
        value.new_visual_rect.Union(new_item.VisualRect());
        if (value.reason == PaintInvalidationReason::kNone) {
          value.reason = new_item.IsCacheable()
                             ? PaintInvalidationReason::kAppeared
                             : PaintInvalidationReason::kUncacheable;
        }
      }
      continue;
    }

    auto reason = new_item.GetPaintInvalidationReason();
    if (!IsFullPaintInvalidationReason(reason) &&
        matched_old_item < latest_cached_old_item) {
      // |new_item| has been moved above other cached items.
      reason = PaintInvalidationReason::kReordered;
    }

    const auto& old_item = *matched_old_item;
    if (reason != PaintInvalidationReason::kNone &&
        (old_item.DrawsContent() || new_item.DrawsContent())) {
      // The display item reordered, skipped cache or changed. Will invalidate
      // for both the old and new display items.
      auto& value = clients_to_invalidate
                        .insert(new_item.ClientId(), OldAndNewDisplayItems())
                        .stored_value->value;
      if (old_item.IsTombstone() || old_item.DrawsContent())
        value.old_visual_rect.Union(old_item.VisualRect());
      if (new_item.DrawsContent())
        value.new_visual_rect.Union(new_item.VisualRect());
      value.reason = reason;
    }

    wtf_size_t offset =
        static_cast<wtf_size_t>(matched_old_item - old_display_items_.begin());
    DCHECK(!old_display_items_matched[offset]);
    old_display_items_matched[offset] = true;

    // |old_item.IsTombstone()| is true means that |new_item| was copied from
    // cached |old_item|.
    if (old_item.IsTombstone()) {
      latest_cached_old_item =
          std::max(latest_cached_old_item, matched_old_item);
    }
  }

  // Invalidate remaining unmatched (disappeared or uncacheable) old items.
  for (auto it = old_display_items_.begin(); it != old_display_items_.end();
       ++it) {
    if (old_display_items_matched[static_cast<wtf_size_t>(
            it - old_display_items_.begin())])
      continue;

    const auto& old_item = *it;
    if (old_item.DrawsContent() || old_item.IsTombstone()) {
      clients_to_invalidate.insert(old_item.ClientId(), OldAndNewDisplayItems())
          .stored_value->value.old_visual_rect.Union(old_item.VisualRect());
    }
  }

  for (const auto& item : clients_to_invalidate) {
    GenerateRasterInvalidation(item.key, item.value.old_visual_rect,
                               item.value.new_visual_rect, item.value.reason);
  }
}

DisplayItemIterator DisplayItemRasterInvalidator::MatchNewDisplayItemInOldChunk(
    const DisplayItem& new_item,
    DisplayItemIterator& next_old_item_to_match) {
  if (!new_item.IsCacheable())
    return old_display_items_.end();
  for (; next_old_item_to_match != old_display_items_.end();
       next_old_item_to_match++) {
    const auto& old_item = *next_old_item_to_match;
    if (!old_item.IsCacheable())
      continue;
    if (old_item.GetId() == new_item.GetId())
      return next_old_item_to_match++;
    // Add the skipped old item into index.
    old_display_items_index_
        .insert(old_item.ClientId(), Vector<DisplayItemIterator>())
        .stored_value->value.push_back(next_old_item_to_match);
  }

  // Didn't find matching old item in sequential matching. Look up the index.
  auto it = old_display_items_index_.find(new_item.ClientId());
  if (it == old_display_items_index_.end())
    return old_display_items_.end();
  for (auto i : it->value) {
    if (i->GetId() == new_item.GetId())
      return i;
  }
  return old_display_items_.end();
}

void DisplayItemRasterInvalidator::AddRasterInvalidation(
    DisplayItemClientId client_id,
    const gfx::Rect& rect,
    PaintInvalidationReason reason,
    RasterInvalidator::ClientIsOldOrNew old_or_new) {
  gfx::Rect r = invalidator_.ClipByLayerBounds(mapper_.MapVisualRect(rect));
  if (r.IsEmpty())
    return;

  invalidator_.AddRasterInvalidation(r, client_id, reason, old_or_new);
}

void DisplayItemRasterInvalidator::GenerateRasterInvalidation(
    DisplayItemClientId client_id,
    const gfx::Rect& old_visual_rect,
    const gfx::Rect& new_visual_rect,
    PaintInvalidationReason reason) {
  if (new_visual_rect.IsEmpty()) {
    if (!old_visual_rect.IsEmpty()) {
      AddRasterInvalidation(client_id, old_visual_rect,
                            PaintInvalidationReason::kDisappeared,
                            kClientIsOld);
    }
    return;
  }

  if (old_visual_rect.IsEmpty()) {
    AddRasterInvalidation(client_id, new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (reason == PaintInvalidationReason::kJustCreated) {
    // The old client has been deleted and the new client happens to be at the
    // same address. They have no relationship.
    AddRasterInvalidation(client_id, old_visual_rect,
                          PaintInvalidationReason::kDisappeared, kClientIsOld);
    AddRasterInvalidation(client_id, new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (!IsFullPaintInvalidationReason(reason) &&
      old_visual_rect.origin() != new_visual_rect.origin())
    reason = PaintInvalidationReason::kLayout;

  if (IsFullPaintInvalidationReason(reason)) {
    GenerateFullRasterInvalidation(client_id, old_visual_rect, new_visual_rect,
                                   reason);
    return;
  }

  DCHECK_EQ(old_visual_rect.origin(), new_visual_rect.origin());
  GenerateIncrementalRasterInvalidation(client_id, old_visual_rect,
                                        new_visual_rect);
}

static gfx::Rect ComputeRightDelta(const gfx::Point& location,
                                   const gfx::Size& old_size,
                                   const gfx::Size& new_size) {
  int delta = new_size.width() - old_size.width();
  if (delta > 0) {
    return gfx::Rect(location.x() + old_size.width(), location.y(), delta,
                     new_size.height());
  }
  if (delta < 0) {
    return gfx::Rect(location.x() + new_size.width(), location.y(), -delta,
                     old_size.height());
  }
  return gfx::Rect();
}

static gfx::Rect ComputeBottomDelta(const gfx::Point& location,
                                    const gfx::Size& old_size,
                                    const gfx::Size& new_size) {
  int delta = new_size.height() - old_size.height();
  if (delta > 0) {
    return gfx::Rect(location.x(), location.y() + old_size.height(),
                     new_size.width(), delta);
  }
  if (delta < 0) {
    return gfx::Rect(location.x(), location.y() + new_size.height(),
                     old_size.width(), -delta);
  }
  return gfx::Rect();
}

void DisplayItemRasterInvalidator::GenerateIncrementalRasterInvalidation(
    DisplayItemClientId client_id,
    const gfx::Rect& old_visual_rect,
    const gfx::Rect& new_visual_rect) {
  DCHECK_EQ(old_visual_rect.origin(), new_visual_rect.origin());

  gfx::Rect right_delta = ComputeRightDelta(
      new_visual_rect.origin(), old_visual_rect.size(), new_visual_rect.size());
  if (!right_delta.IsEmpty()) {
    AddRasterInvalidation(client_id, right_delta,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }

  gfx::Rect bottom_delta = ComputeBottomDelta(
      new_visual_rect.origin(), old_visual_rect.size(), new_visual_rect.size());
  if (!bottom_delta.IsEmpty()) {
    AddRasterInvalidation(client_id, bottom_delta,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }
}

void DisplayItemRasterInvalidator::GenerateFullRasterInvalidation(
    DisplayItemClientId client_id,
    const gfx::Rect& old_visual_rect,
    const gfx::Rect& new_visual_rect,
    PaintInvalidationReason reason) {
  if (!new_visual_rect.Contains(old_visual_rect)) {
    AddRasterInvalidation(client_id, old_visual_rect, reason, kClientIsNew);
    if (old_visual_rect.Contains(new_visual_rect))
      return;
  }

  AddRasterInvalidation(client_id, new_visual_rect, reason, kClientIsNew);
}

}  // namespace blink
