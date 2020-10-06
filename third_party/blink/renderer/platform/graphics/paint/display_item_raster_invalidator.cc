// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

namespace blink {

void DisplayItemRasterInvalidator::Generate() {
  struct OldAndNewDisplayItems {
    // Union of visual rects of all old display items of the client.
    IntRect old_visual_rect;
    // Union of visual rects of all new display items of the client.
    IntRect new_visual_rect;
    PaintInvalidationReason reason = PaintInvalidationReason::kNone;
  };
  HashMap<const DisplayItemClient*, OldAndNewDisplayItems>
      clients_to_invalidate;

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
                          .insert(&new_item.Client(), OldAndNewDisplayItems())
                          .stored_value->value;
        value.new_visual_rect.Unite(new_item.VisualRect());
        if (value.reason == PaintInvalidationReason::kNone) {
          value.reason = new_item.Client().IsCacheable()
                             ? PaintInvalidationReason::kAppeared
                             : PaintInvalidationReason::kUncacheable;
        }
      }
      continue;
    }

    auto reason = new_item.Client().GetPaintInvalidationReason();
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
                        .insert(&new_item.Client(), OldAndNewDisplayItems())
                        .stored_value->value;
      if (old_item.IsTombstone() || old_item.DrawsContent())
        value.old_visual_rect.Unite(old_item.VisualRect());
      if (new_item.DrawsContent())
        value.new_visual_rect.Unite(new_item.VisualRect());
      value.reason = reason;
    }

    wtf_size_t offset = matched_old_item - old_display_items_.begin();
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
    if (old_display_items_matched[it - old_display_items_.begin()])
      continue;

    const auto& old_item = *it;
    if (old_item.DrawsContent() || old_item.IsTombstone()) {
      clients_to_invalidate.insert(&old_item.Client(), OldAndNewDisplayItems())
          .stored_value->value.old_visual_rect.Unite(old_item.VisualRect());
    }
  }

  for (const auto& item : clients_to_invalidate) {
    GenerateRasterInvalidation(*item.key, item.value.old_visual_rect,
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
        .insert(&old_item.Client(), Vector<DisplayItemIterator>())
        .stored_value->value.push_back(next_old_item_to_match);
  }

  // Didn't find matching old item in sequential matching. Look up the index.
  auto it = old_display_items_index_.find(&new_item.Client());
  if (it == old_display_items_index_.end())
    return old_display_items_.end();
  for (auto i : it->value) {
    if (i->GetId() == new_item.GetId())
      return i;
  }
  return old_display_items_.end();
}

void DisplayItemRasterInvalidator::AddRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect& rect,
    PaintInvalidationReason reason,
    RasterInvalidator::ClientIsOldOrNew old_or_new) {
  IntRect r = invalidator_.ClipByLayerBounds(mapper_.MapVisualRect(rect));
  if (r.IsEmpty())
    return;

  invalidator_.AddRasterInvalidation(raster_invalidation_function_, r, client,
                                     reason, old_or_new);
}

void DisplayItemRasterInvalidator::GenerateRasterInvalidation(
    const DisplayItemClient& client,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect,
    PaintInvalidationReason reason) {
  if (new_visual_rect.IsEmpty()) {
    if (!old_visual_rect.IsEmpty()) {
      AddRasterInvalidation(client, old_visual_rect,
                            PaintInvalidationReason::kDisappeared,
                            kClientIsOld);
    }
    return;
  }

  if (old_visual_rect.IsEmpty()) {
    AddRasterInvalidation(client, new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (client.IsJustCreated()) {
    // The old client has been deleted and the new client happens to be at the
    // same address. They have no relationship.
    AddRasterInvalidation(client, old_visual_rect,
                          PaintInvalidationReason::kDisappeared, kClientIsOld);
    AddRasterInvalidation(client, new_visual_rect,
                          PaintInvalidationReason::kAppeared, kClientIsNew);
    return;
  }

  if (!IsFullPaintInvalidationReason(reason) &&
      old_visual_rect.Location() != new_visual_rect.Location())
    reason = PaintInvalidationReason::kGeometry;

  if (IsFullPaintInvalidationReason(reason)) {
    GenerateFullRasterInvalidation(client, old_visual_rect, new_visual_rect,
                                   reason);
    return;
  }

  DCHECK_EQ(old_visual_rect.Location(), new_visual_rect.Location());
  GenerateIncrementalRasterInvalidation(client, old_visual_rect,
                                        new_visual_rect);

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
