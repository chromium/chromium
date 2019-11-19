// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

namespace blink {

void DisplayItemRasterInvalidator::Generate() {
  struct OldAndNewDisplayItems {
    const IntRect* old_visual_rect = nullptr;
    const IntRect* new_visual_rect = nullptr;
    PaintInvalidationReason reason = PaintInvalidationReason::kNone;
  };
  // If there are multiple display items changed for a client, the map will
  // store only one (pair) of them because we only need the visual rect and the
  // the multiple items have the same visual rect.
  HashMap<const DisplayItemClient*, OldAndNewDisplayItems>
      clients_to_invalidate;

  Vector<bool> old_display_items_matched;
  old_display_items_matched.resize(old_chunk_.size());
  size_t next_old_item_to_match = old_chunk_.begin_index;
  size_t max_cached_old_index = next_old_item_to_match;

  for (const auto& new_item :
       new_paint_artifact_.GetDisplayItemList().ItemsInPaintChunk(new_chunk_)) {
    size_t matched_old_index =
        MatchNewDisplayItemInOldChunk(new_item, next_old_item_to_match);
    if (matched_old_index == kNotFound) {
      if (new_item.DrawsContent()) {
        // Will invalidate for the new display item which doesn't match any old
        // display item.
        auto& value = clients_to_invalidate
                          .insert(&new_item.Client(), OldAndNewDisplayItems())
                          .stored_value->value;
        if (!value.new_visual_rect) {
          value.new_visual_rect = &new_item.VisualRect();
          value.reason = new_item.Client().IsCacheable()
                             ? PaintInvalidationReason::kAppeared
                             : PaintInvalidationReason::kUncacheable;
        }
      }
      continue;
    }

    auto reason = new_item.Client().GetPaintInvalidationReason();
    if (!IsFullPaintInvalidationReason(reason) &&
        matched_old_index < max_cached_old_index) {
      // |new_item| has been moved above other cached items.
      reason = PaintInvalidationReason::kReordered;
    }

    const auto& old_item =
        old_paint_artifact_.GetDisplayItemList()[matched_old_index];
    if (reason != PaintInvalidationReason::kNone &&
        (old_item.DrawsContent() || new_item.DrawsContent())) {
      // The display item reordered, skipped cache or changed. Will invalidate
      // for both the old and new display items.
      auto& value = clients_to_invalidate
                        .insert(&new_item.Client(), OldAndNewDisplayItems())
                        .stored_value->value;
      if (old_item.IsTombstone() || old_item.DrawsContent())
        value.old_visual_rect = &old_item.VisualRect();
      if (new_item.DrawsContent())
        value.new_visual_rect = &new_item.VisualRect();
      value.reason = reason;
    }

    size_t offset = matched_old_index - old_chunk_.begin_index;
    DCHECK(!old_display_items_matched[offset]);
    old_display_items_matched[offset] = true;

    // |old_item.IsTombstone()| is true means that |new_item| was copied from
    // cached |old_item|.
    if (old_item.IsTombstone())
      max_cached_old_index = std::max(max_cached_old_index, matched_old_index);
  }

  // Invalidate remaining unmatched (disappeared or uncacheable) old items.
  for (size_t i = old_chunk_.begin_index; i < old_chunk_.end_index; ++i) {
    if (old_display_items_matched[i - old_chunk_.begin_index])
      continue;

    const auto& old_item = old_paint_artifact_.GetDisplayItemList()[i];
    if (old_item.DrawsContent() || old_item.IsTombstone()) {
      clients_to_invalidate.insert(&old_item.Client(), OldAndNewDisplayItems())
          .stored_value->value.old_visual_rect = &old_item.VisualRect();
    }
  }

  for (const auto& item : clients_to_invalidate) {
    GenerateRasterInvalidation(*item.key, item.value.old_visual_rect,
                               item.value.new_visual_rect, item.value.reason);
  }
}

size_t DisplayItemRasterInvalidator::MatchNewDisplayItemInOldChunk(
    const DisplayItem& new_item,
    size_t& next_old_item_to_match) {
  if (!new_item.IsCacheable())
    return kNotFound;
  for (; next_old_item_to_match < old_chunk_.end_index;
       next_old_item_to_match++) {
    const auto& old_item =
        old_paint_artifact_.GetDisplayItemList()[next_old_item_to_match];
    if (!old_item.IsCacheable())
      continue;
    if (old_item.GetId() == new_item.GetId())
      return next_old_item_to_match++;
    // Add the skipped old item into index.
    old_display_items_index_.insert(&old_item.Client(), Vector<size_t>())
        .stored_value->value.push_back(next_old_item_to_match);
  }

  // Didn't find matching old item in sequential matching. Look up the index.
  auto it = old_display_items_index_.find(&new_item.Client());
  if (it == old_display_items_index_.end())
    return kNotFound;
  for (size_t i : it->value) {
    const auto& old_item = old_paint_artifact_.GetDisplayItemList()[i];
    if (old_item.GetId() == new_item.GetId())
      return i;
  }
  return kNotFound;
}

void DisplayItemRasterInvalidator::AddRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect& rect,
    PaintInvalidationReason reason,
    RasterInvalidator::ClientIsOldOrNew old_or_new) {
  IntRect r = invalidator_.ClipByLayerBounds(mapper_.MapVisualRect(rect));
  if (r.IsEmpty())
    return;

  invalidator_.AddRasterInvalidation(r, client, reason, old_or_new);
}

void DisplayItemRasterInvalidator::GenerateRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect* old_visual_rect,
    const IntRect* new_visual_rect,
    PaintInvalidationReason reason) {
  if (!new_visual_rect || new_visual_rect->IsEmpty()) {
    if (old_visual_rect && !old_visual_rect->IsEmpty()) {
      AddRasterInvalidation(client, *old_visual_rect,
                            PaintInvalidationReason::kDisappeared,
                            kClientIsOld);
    }
    return;
  }

  if (!old_visual_rect || old_visual_rect->IsEmpty()) {
    AddRasterInvalidation(client, *new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (client.IsJustCreated()) {
    // The old client has been deleted and the new client happens to be at the
    // same address. They have no relationship.
    AddRasterInvalidation(client, *old_visual_rect,
                          PaintInvalidationReason::kDisappeared, kClientIsOld);
    AddRasterInvalidation(client, *new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (IsFullPaintInvalidationReason(reason)) {
    GenerateFullRasterInvalidation(client, *old_visual_rect, *new_visual_rect,
                                   reason);
    return;
  }

  GenerateIncrementalRasterInvalidation(client, *old_visual_rect,
                                        *new_visual_rect);

  IntRect partial_rect = client.PartialInvalidationVisualRect();
  if (!partial_rect.IsEmpty())
    AddRasterInvalidation(client, partial_rect, reason, kClientIsNew);
}

static IntRect ComputeRightDelta(const IntPoint& location,
                                 const IntSize& old_size,
                                 const IntSize& new_size) {
  int delta = new_size.Width() - old_size.Width();
  if (delta > 0) {
    return IntRect(location.X() + old_size.Width(), location.Y(), delta,
                   new_size.Height());
  }
  if (delta < 0) {
    return IntRect(location.X() + new_size.Width(), location.Y(), -delta,
                   old_size.Height());
  }
  return IntRect();
}

static IntRect ComputeBottomDelta(const IntPoint& location,
                                  const IntSize& old_size,
                                  const IntSize& new_size) {
  int delta = new_size.Height() - old_size.Height();
  if (delta > 0) {
    return IntRect(location.X(), location.Y() + old_size.Height(),
                   new_size.Width(), delta);
  }
  if (delta < 0) {
    return IntRect(location.X(), location.Y() + new_size.Height(),
                   old_size.Width(), -delta);
  }
  return IntRect();
}

void DisplayItemRasterInvalidator::GenerateIncrementalRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect) {
  DCHECK(old_visual_rect.Location() == new_visual_rect.Location());

  IntRect right_delta =
      ComputeRightDelta(new_visual_rect.Location(), old_visual_rect.Size(),
                        new_visual_rect.Size());
  if (!right_delta.IsEmpty()) {
    AddRasterInvalidation(client, right_delta,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }

  IntRect bottom_delta =
      ComputeBottomDelta(new_visual_rect.Location(), old_visual_rect.Size(),
                         new_visual_rect.Size());
  if (!bottom_delta.IsEmpty()) {
    AddRasterInvalidation(client, bottom_delta,
                          PaintInvalidationReason::kIncremental, kClientIsNew);
  }
}

void DisplayItemRasterInvalidator::GenerateFullRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect,
    PaintInvalidationReason reason) {
  if (!new_visual_rect.Contains(old_visual_rect)) {
    AddRasterInvalidation(client, old_visual_rect, reason, kClientIsNew);
    if (old_visual_rect.Contains(new_visual_rect))
      return;
  }

  AddRasterInvalidation(client, new_visual_rect, reason, kClientIsNew);
}

}  // namespace blink
