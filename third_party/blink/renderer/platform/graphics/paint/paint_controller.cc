// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

#include <memory>
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PaintController::PaintController(Usage usage)
    : usage_(usage),
      current_paint_artifact_(PaintArtifact::Empty()),
      new_display_item_list_(0) {
  // frame_first_paints_ should have one null frame since the beginning, so
  // that PaintController is robust even if it paints outside of BeginFrame
  // and EndFrame cycles. It will also enable us to combine the first paint
  // data in this PaintController into another PaintController on which we
  // replay the recorded results in the future.
  frame_first_paints_.push_back(FrameFirstPaint(nullptr));
}

PaintController::~PaintController() {
  // New display items should be committed before PaintController is destroyed,
  // except for transient paint controllers.
  DCHECK(usage_ == kTransient || new_display_item_list_.IsEmpty());
}

bool PaintController::UseCachedItemIfPossible(const DisplayItemClient& client,
                                              DisplayItem::Type type) {
  if (usage_ == kTransient)
    return false;

  if (DisplayItemConstructionIsDisabled())
    return false;

  if (!ClientCacheIsValid(client))
    return false;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of a subsequence enclosing this
    // display item. Let the client continue to actually paint the display item.
    return false;
  }

  size_t cached_item =
      FindCachedItem(DisplayItem::Id(client, type, current_fragment_));
  if (cached_item == kNotFound) {
    // See FindOutOfOrderCachedItemForward() for explanation of the situation.
    return false;
  }

  ++num_cached_new_items_;
  EnsureNewDisplayItemListInitialCapacity();
  // Visual rect can change without needing invalidation of the client, e.g.
  // when ancestor clip changes. Update the visual rect to the current value.
  current_paint_artifact_->GetDisplayItemList()[cached_item].UpdateVisualRect();
  if (!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    ProcessNewItem(MoveItemFromCurrentListToNewList(cached_item));

  next_item_to_match_ = cached_item + 1;
  // Items before |next_item_to_match_| have been copied so we don't need to
  // index them.
  if (next_item_to_match_ > next_item_to_index_)
    next_item_to_index_ = next_item_to_match_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    if (!IsCheckingUnderInvalidation()) {
      under_invalidation_checking_begin_ = cached_item;
      under_invalidation_checking_end_ = cached_item + 1;
      under_invalidation_message_prefix_ = "";
    }
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  return true;
}

bool PaintController::UseCachedSubsequenceIfPossible(
    const DisplayItemClient& client) {
  if (usage_ == kTransient)
    return false;

  if (DisplayItemConstructionIsDisabled() || SubsequenceCachingIsDisabled())
    return false;

  if (!ClientCacheIsValid(client))
    return false;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of an ancestor subsequence enclosing
    // this one. The ancestor subsequence is supposed to have already "copied",
    // so we should let the client continue to actually paint the descendant
    // subsequences without "copying".
    return false;
  }

  SubsequenceMarkers* markers = GetSubsequenceMarkers(client);
  if (!markers) {
    return false;
  }

  if (current_paint_artifact_->GetDisplayItemList()[markers->start]
          .IsTombstone()) {
    // The subsequence has already been copied, indicating that the same client
    // created multiple subsequences. If DCHECK_IS_ON(), then we should have
    // encountered the DCHECK at the end of EndSubsequence() during the previous
    // paint.
    NOTREACHED();
    return false;
  }

  EnsureNewDisplayItemListInitialCapacity();

  if (next_item_to_match_ == markers->start) {
    // We are matching new and cached display items sequentially. Skip the
    // subsequence for later sequential matching of individual display items.
    next_item_to_match_ = markers->end;
    // Items before |next_item_to_match_| have been copied so we don't need to
    // index them.
    if (next_item_to_match_ > next_item_to_index_)
      next_item_to_index_ = next_item_to_match_;
  }

  num_cached_new_items_ += markers->end - markers->start;
  ++num_cached_new_subsequences_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    DCHECK(!IsCheckingUnderInvalidation());
    under_invalidation_checking_begin_ = markers->start;
    under_invalidation_checking_end_ = markers->end;
    under_invalidation_message_prefix_ =
        "(In cached subsequence for " + client.DebugName() + ")";
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  size_t start = BeginSubsequence();
  CopyCachedSubsequence(markers->start, markers->end);
  EndSubsequence(client, start);
  return true;
}

PaintController::SubsequenceMarkers* PaintController::GetSubsequenceMarkers(
    const DisplayItemClient& client) {
  auto result = current_cached_subsequences_.find(&client);
  if (result == current_cached_subsequences_.end())
    return nullptr;
  return &result->value;
}

size_t PaintController::BeginSubsequence() {
  // Force new paint chunk which is required for subsequence caching.
  new_paint_chunks_.ForceNewChunk();
  return new_display_item_list_.size();
}

void PaintController::EndSubsequence(const DisplayItemClient& client,
                                     size_t start) {
  size_t end = new_display_item_list_.size();

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      IsCheckingUnderInvalidation()) {
    SubsequenceMarkers* markers = GetSubsequenceMarkers(client);
    if (!markers && start != end) {
      ShowSequenceUnderInvalidationError(
          "under-invalidation : unexpected subsequence", client, start, end);
      CHECK(false);
    }
    if (markers && markers->end - markers->start != end - start) {
      ShowSequenceUnderInvalidationError(
          "under-invalidation: new subsequence wrong length", client, start,
          end);
      CHECK(false);
    }
  }

  if (start == end) {
    // Omit the empty subsequence. The forcing-new-chunk flag set by
    // BeginSubsequence() still applies, but this not a big deal because empty
    // subsequences are not common. Also we should not clear the flag because
    // there might be unhandled flag that was set before this empty subsequence.
    return;
  }

  // Force new paint chunk which is required for subsequence caching.
  new_paint_chunks_.ForceNewChunk();

  DCHECK(!new_cached_subsequences_.Contains(&client))
      << "Multiple subsequences for client: " << client.DebugName();

  new_cached_subsequences_.insert(&client, SubsequenceMarkers(start, end));
}

void PaintController::ProcessNewItem(DisplayItem& display_item) {
  DCHECK(!construction_disabled_);

  if (IsSkippingCache() && usage_ == kMultiplePaints) {
    display_item.Client().Invalidate(PaintInvalidationReason::kUncacheable);
    display_item.SetUncacheable();
  }

  bool chunk_added = new_paint_chunks_.IncrementDisplayItemIndex(display_item);

#if DCHECK_IS_ON()
  if (chunk_added && CurrentPaintChunk().is_cacheable) {
    AddToIndicesByClientMap(CurrentPaintChunk().id.client,
                            new_paint_chunks_.LastChunkIndex(),
                            new_paint_chunk_indices_by_client_);
  }

  if (usage_ == kMultiplePaints && display_item.IsCacheable()) {
    size_t index = FindMatchingItemFromIndex(
        display_item.GetId(), new_display_item_indices_by_client_,
        new_display_item_list_);
    if (index != kNotFound) {
      ShowDebugData();
      NOTREACHED() << "DisplayItem " << display_item.AsDebugString().Utf8()
                   << " has duplicated id with previous "
                   << new_display_item_list_[index].AsDebugString().Utf8()
                   << " (index=" << index << ")";
    }
    AddToIndicesByClientMap(display_item.Client(),
                            new_display_item_list_.size() - 1,
                            new_display_item_indices_by_client_);
  }
#else  // DCHECK_IS_ON()
  std::ignore = chunk_added;
#endif

  if (usage_ == kMultiplePaints &&
      RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    CheckUnderInvalidation();

  if (!frame_first_paints_.back().first_painted && display_item.IsDrawing() &&
      // Here we ignore all document-background paintings because we don't
      // know if the background is default. ViewPainter should have called
      // setFirstPainted() if this display item is for non-default
      // background.
      display_item.GetType() != DisplayItem::kDocumentBackground &&
      display_item.DrawsContent()) {
    SetFirstPainted();
  }
}

DisplayItem& PaintController::MoveItemFromCurrentListToNewList(size_t index) {
  return new_display_item_list_.AppendByMoving(
      current_paint_artifact_->GetDisplayItemList()[index]);
}

void PaintController::InvalidateAll() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  InvalidateAllInternal();
}

void PaintController::InvalidateAllInternal() {
  // TODO(wangxianzhu): Rename this to InvalidateAllForTesting() for CAP.
  // Can only be called during layout/paintInvalidation, not during painting.
  DCHECK(new_display_item_list_.IsEmpty());
  current_paint_artifact_ = PaintArtifact::Empty();
  current_cached_subsequences_.clear();
  cache_is_all_invalid_ = true;
}

bool PaintController::CacheIsAllInvalid() const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(!cache_is_all_invalid_ || current_paint_artifact_->IsEmpty());
  return cache_is_all_invalid_;
}

bool PaintController::ClientCacheIsValid(
    const DisplayItemClient& client) const {
#if DCHECK_IS_ON()
  DCHECK(client.IsAlive());
#endif
  if (IsSkippingCache() || cache_is_all_invalid_)
    return false;
  return client.IsValid();
}

size_t PaintController::FindMatchingItemFromIndex(
    const DisplayItem::Id& id,
    const IndicesByClientMap& display_item_indices_by_client,
    const DisplayItemList& list) {
  IndicesByClientMap::const_iterator it =
      display_item_indices_by_client.find(&id.client);
  if (it == display_item_indices_by_client.end())
    return kNotFound;

  const Vector<size_t>& indices = it->value;
  for (size_t index : indices) {
    const DisplayItem& existing_item = list[index];
    if (existing_item.IsTombstone())
      continue;
    DCHECK(existing_item.Client() == id.client);
    if (id == existing_item.GetId())
      return index;
  }

  return kNotFound;
}

void PaintController::AddToIndicesByClientMap(const DisplayItemClient& client,
                                              size_t index,
                                              IndicesByClientMap& map) {
  auto it = map.find(&client);
  auto& indices =
      it == map.end()
          ? map.insert(&client, Vector<size_t>()).stored_value->value
          : it->value;
  indices.push_back(index);
}

size_t PaintController::FindCachedItem(const DisplayItem::Id& id) {
  DCHECK(ClientCacheIsValid(id.client));

  if (next_item_to_match_ <
      current_paint_artifact_->GetDisplayItemList().size()) {
    // If current_list[next_item_to_match_] matches the new item, we don't need
    // to update and lookup the index, which is fast. This is the common case
    // that the current list and the new list are in the same order around the
    // new item.
    const DisplayItem& item =
        current_paint_artifact_->GetDisplayItemList()[next_item_to_match_];
    // We encounter an item that has already been copied which indicates we
    // can't do sequential matching.
    if (!item.IsTombstone() && id == item.GetId()) {
#if DCHECK_IS_ON()
      ++num_sequential_matches_;
#endif
      return next_item_to_match_;
    }
  }

  size_t found_index =
      FindMatchingItemFromIndex(id, out_of_order_item_indices_,
                                current_paint_artifact_->GetDisplayItemList());
  if (found_index != kNotFound) {
#if DCHECK_IS_ON()
    ++num_out_of_order_matches_;
#endif
    return found_index;
  }

  return FindOutOfOrderCachedItemForward(id);
}

// Find forward for the item and index all skipped indexable items.
size_t PaintController::FindOutOfOrderCachedItemForward(
    const DisplayItem::Id& id) {
  for (size_t i = next_item_to_index_;
       i < current_paint_artifact_->GetDisplayItemList().size(); ++i) {
    const DisplayItem& item = current_paint_artifact_->GetDisplayItemList()[i];
    if (item.IsTombstone())
      continue;
    if (id == item.GetId()) {
#if DCHECK_IS_ON()
      ++num_sequential_matches_;
#endif
      return i;
    }
    if (item.IsCacheable()) {
#if DCHECK_IS_ON()
      ++num_indexed_items_;
#endif
      AddToIndicesByClientMap(item.Client(), i, out_of_order_item_indices_);
      next_item_to_index_ = i + 1;
    }
  }

  // The display item newly appears while the client is not invalidated. The
  // situation alone (without other kinds of under-invalidations) won't corrupt
  // rendering, but causes AddItemToIndexIfNeeded() for all remaining display
  // item, which is not the best for performance. In this case, the caller
  // should fall back to repaint the display item.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
#if DCHECK_IS_ON()
    ShowDebugData();
#endif
    // Ensure our paint invalidation tests don't trigger the less performant
    // situation which should be rare.
    DLOG(WARNING) << "Can't find cached display item: " << id;
  }
  return kNotFound;
}

// Copies a cached subsequence from current list to the new list.
// When paintUnderInvaldiationCheckingEnabled() we'll not actually
// copy the subsequence, but mark the begin and end of the subsequence for
// under-invalidation checking.
void PaintController::CopyCachedSubsequence(size_t begin_index,
                                            size_t end_index) {
  DCHECK(!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());

  const DisplayItem* cached_item =
      &current_paint_artifact_->GetDisplayItemList()[begin_index];

  auto* cached_chunk =
      current_paint_artifact_->FindChunkByDisplayItemIndex(begin_index);
  DCHECK(cached_chunk != current_paint_artifact_->PaintChunks().end());
  auto properties_before_subsequence =
      new_paint_chunks_.CurrentPaintChunkProperties();
  UpdateCurrentPaintChunkPropertiesUsingIdWithFragment(
      cached_chunk->id, cached_chunk->properties.GetPropertyTreeState());

  for (size_t current_index = begin_index; current_index < end_index;
       ++current_index) {
    cached_item = &current_paint_artifact_->GetDisplayItemList()[current_index];
    SECURITY_CHECK(!cached_item->IsTombstone());
#if DCHECK_IS_ON()
    DCHECK(cached_item->Client().IsAlive());
#endif

    if (current_index == cached_chunk->end_index) {
      ++cached_chunk;
      DCHECK(cached_chunk != current_paint_artifact_->PaintChunks().end());
      new_paint_chunks_.ForceNewChunk();
      UpdateCurrentPaintChunkPropertiesUsingIdWithFragment(
          cached_chunk->id, cached_chunk->properties.GetPropertyTreeState());
    }

#if DCHECK_IS_ON()
    // Visual rect change should not happen in a cached subsequence.
    // However, because of different method of pixel snapping in different
    // paths, there are false positives. Just log an error.
    if (cached_item->VisualRect() != cached_item->Client().VisualRect()) {
      DLOG(ERROR) << "Visual rect changed in a cached subsequence: "
                  << cached_item->Client().DebugName()
                  << " old=" << cached_item->VisualRect()
                  << " new=" << cached_item->Client().VisualRect();
    }
#endif

    ProcessNewItem(MoveItemFromCurrentListToNewList(current_index));
    DCHECK((!CurrentPaintChunk().is_cacheable && !cached_chunk->is_cacheable) ||
           CurrentPaintChunk().Matches(*cached_chunk));
  }

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    under_invalidation_checking_end_ = end_index;
    DCHECK(IsCheckingUnderInvalidation());
  } else {
    // Restore properties and force new chunk for any trailing display items
    // after the cached subsequence without new properties.
    new_paint_chunks_.ForceNewChunk();
    UpdateCurrentPaintChunkProperties(base::nullopt,
                                      properties_before_subsequence);
  }
}

void PaintController::ResetCurrentListIndices() {
  next_item_to_match_ = 0;
  next_item_to_index_ = 0;
  under_invalidation_checking_begin_ = 0;
  under_invalidation_checking_end_ = 0;
}

DISABLE_CFI_PERF
void PaintController::CommitNewDisplayItems() {
  TRACE_EVENT2("blink,benchmark", "PaintController::commitNewDisplayItems",
               "current_display_list_size",
               (int)current_paint_artifact_->GetDisplayItemList().size(),
               "num_non_cached_new_items",
               (int)new_display_item_list_.size() - num_cached_new_items_);

  if (usage_ == kMultiplePaints)
    UpdateUMACounts();

  num_cached_new_items_ = 0;
  num_cached_new_subsequences_ = 0;
#if DCHECK_IS_ON()
  new_display_item_indices_by_client_.clear();
  new_paint_chunk_indices_by_client_.clear();
#endif

  cache_is_all_invalid_ = false;
  committed_ = true;

  new_cached_subsequences_.swap(current_cached_subsequences_);
  new_cached_subsequences_.clear();

  // The new list will not be appended to again so we can release unused memory.
  new_display_item_list_.ShrinkToFit();

  current_paint_artifact_ =
      PaintArtifact::Create(std::move(new_display_item_list_),
                            new_paint_chunks_.ReleasePaintChunks());

  ResetCurrentListIndices();
  out_of_order_item_indices_.clear();

  // We'll allocate the initial buffer when we start the next paint.
  new_display_item_list_ = DisplayItemList(0);

#if DCHECK_IS_ON()
  num_indexed_items_ = 0;
  num_sequential_matches_ = 0;
  num_out_of_order_matches_ = 0;
#endif
}

void PaintController::FinishCycle() {
  if (usage_ != kTransient) {
#if DCHECK_IS_ON()
    DCHECK(new_display_item_list_.IsEmpty());
    DCHECK(new_paint_chunks_.IsInInitialState());
#endif

    if (committed_) {
      committed_ = false;

      // Validate display item clients that have validly cached subsequence or
      // display items in this PaintController.
      for (auto& item : current_cached_subsequences_) {
        if (item.key->IsCacheable())
          item.key->Validate();
      }
      for (const auto& item : current_paint_artifact_->GetDisplayItemList()) {
        const auto& client = item.Client();
        client.ClearPartialInvalidationVisualRect();
        if (client.IsCacheable())
          client.Validate();
      }
      for (const auto& chunk : current_paint_artifact_->PaintChunks()) {
        if (chunk.id.client.IsCacheable())
          chunk.id.client.Validate();
      }
    }

    current_paint_artifact_->FinishCycle();
  }

  if (VLOG_IS_ON(1)) {
    // Only log for non-transient paint controllers. Before CompositeAfterPaint,
    // there is an additional paint controller used to collect foreign layers,
    // and this can be logged by removing the "usage_ != kTransient" condition.
    if (usage_ != kTransient) {
      LOG(ERROR) << "PaintController::FinishCycle() completed";
#if DCHECK_IS_ON()
      if (VLOG_IS_ON(3))
        ShowDebugDataWithPaintRecords();
      else if (VLOG_IS_ON(2))
        ShowDebugData();
      else if (VLOG_IS_ON(1))
        ShowCompactDebugData();
#endif
    }
  }
}

void PaintController::ClearPropertyTreeChangedStateTo(
    const PropertyTreeState& to) {
  // Calling |ClearChangedTo| for every chunk is O(|property nodes|^2) and
  // could be optimized by caching which nodes that have already been cleared.
  for (const auto& chunk : current_paint_artifact_->PaintChunks()) {
    chunk.properties.Transform().ClearChangedTo(&to.Transform());
    chunk.properties.Clip().ClearChangedTo(&to.Clip());
    chunk.properties.Effect().ClearChangedTo(&to.Effect());
  }
}

size_t PaintController::ApproximateUnsharedMemoryUsage() const {
  size_t memory_usage = sizeof(*this);

  // Memory outside this class due to current_paint_artifact_.
  memory_usage += current_paint_artifact_->ApproximateUnsharedMemoryUsage();

  // External objects, shared with the embedder, such as PaintRecord, should be
  // excluded to avoid double counting. It is the embedder's responsibility to
  // count such objects.

  // Memory outside this class due to new_display_item_list_.
  DCHECK(new_display_item_list_.IsEmpty());
  memory_usage += new_display_item_list_.MemoryUsageInBytes();

  // Memory outside this class due to current_cached_subsequences_ and
  // new_cached_subsequences_.
  memory_usage += current_cached_subsequences_.Capacity() *
                  sizeof(*current_cached_subsequences_.begin());
  DCHECK(new_cached_subsequences_.IsEmpty());
  memory_usage += new_cached_subsequences_.Capacity() *
                  sizeof(*new_cached_subsequences_.begin());

  return memory_usage;
}

void PaintController::AppendDebugDrawingAfterCommit(
    sk_sp<const PaintRecord> record,
    const PropertyTreeState& property_tree_state) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(new_display_item_list_.IsEmpty());
  current_paint_artifact_->AppendDebugDrawing(record, property_tree_state);
}

void PaintController::ShowUnderInvalidationError(
    const char* reason,
    const DisplayItem& new_item,
    const DisplayItem* old_item) const {
  LOG(ERROR) << under_invalidation_message_prefix_ << " " << reason;
#if DCHECK_IS_ON()
  LOG(ERROR) << "New display item: " << new_item.AsDebugString();
  LOG(ERROR) << "Old display item: "
             << (old_item ? old_item->AsDebugString() : "None");
  LOG(ERROR) << "See http://crbug.com/619103.";

  const PaintRecord* new_record = nullptr;
  if (new_item.IsDrawing()) {
    new_record =
        static_cast<const DrawingDisplayItem&>(new_item).GetPaintRecord().get();
  }
  const PaintRecord* old_record = nullptr;
  if (old_item->IsDrawing()) {
    old_record = static_cast<const DrawingDisplayItem*>(old_item)
                     ->GetPaintRecord()
                     .get();
  }
  LOG(INFO) << "new record:\n"
            << (new_record ? RecordAsDebugString(*new_record).Utf8() : "None");
  LOG(INFO) << "old record:\n"
            << (old_record ? RecordAsDebugString(*old_record).Utf8() : "None");

  ShowDebugData();
#else
  LOG(ERROR) << "Run a build with DCHECK on to get more details.";
  LOG(ERROR) << "See http://crbug.com/619103.";
#endif
}

void PaintController::ShowSequenceUnderInvalidationError(
    const char* reason,
    const DisplayItemClient& client,
    int start,
    int end) {
  LOG(ERROR) << under_invalidation_message_prefix_ << " " << reason;
  LOG(ERROR) << "Subsequence client: " << client.DebugName();
#if DCHECK_IS_ON()
  ShowDebugData();
#else
  LOG(ERROR) << "Run a build with DCHECK on to get more details.";
#endif
  LOG(ERROR) << "See http://crbug.com/619103.";
}

void PaintController::CheckUnderInvalidation() {
  DCHECK_EQ(usage_, kMultiplePaints);
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());

  if (!IsCheckingUnderInvalidation())
    return;

  if (IsSkippingCache()) {
    // We allow cache skipping and temporary under-invalidation in cached
    // subsequences. See the usage of DisplayItemCacheSkipper in BoxPainter.
    under_invalidation_checking_end_ = 0;
    // Match the remaining display items in the subsequence normally.
    next_item_to_match_ = next_item_to_index_ =
        under_invalidation_checking_begin_;
    return;
  }

  const DisplayItem& new_item = new_display_item_list_.Last();
  size_t old_item_index = under_invalidation_checking_begin_;
  DisplayItem* old_item =
      old_item_index < current_paint_artifact_->GetDisplayItemList().size()
          ? &current_paint_artifact_->GetDisplayItemList()[old_item_index]
          : nullptr;

  if (!old_item || !new_item.Equals(*old_item)) {
    // If we ever skipped reporting any under-invalidations, report the earliest
    // one.
    ShowUnderInvalidationError(
        "under-invalidation: display item changed",
        new_display_item_list_.Last(),
        &current_paint_artifact_
             ->GetDisplayItemList()[under_invalidation_checking_begin_]);
    CHECK(false);
  }

  // Discard the forced repainted display item and move the cached item into
  // new_display_item_list_. This is to align with the
  // non-under-invalidation-checking path to empty the original cached slot,
  // leaving only disappeared or invalidated display items in the old list after
  // painting.
  new_display_item_list_.RemoveLast();
  MoveItemFromCurrentListToNewList(old_item_index);

  ++under_invalidation_checking_begin_;
}

void PaintController::SetFirstPainted() {
  frame_first_paints_.back().first_painted = true;
}

void PaintController::SetTextPainted() {
  frame_first_paints_.back().text_painted = true;
}

void PaintController::SetImagePainted() {
  frame_first_paints_.back().image_painted = true;
}

void PaintController::BeginFrame(const void* frame) {
  frame_first_paints_.push_back(FrameFirstPaint(frame));
}

FrameFirstPaint PaintController::EndFrame(const void* frame) {
  FrameFirstPaint result = frame_first_paints_.back();
  DCHECK(result.frame == frame);
  frame_first_paints_.pop_back();
  return result;
}

#if DCHECK_IS_ON()
void PaintController::CheckDuplicatePaintChunkId(const PaintChunk::Id& id) {
  if (IsSkippingCache())
    return;

  if (DisplayItem::IsForeignLayerType(id.type))
    return;

  auto it = new_paint_chunk_indices_by_client_.find(&id.client);
  if (it != new_paint_chunk_indices_by_client_.end()) {
    const auto& indices = it->value;
    for (auto index : indices) {
      const auto& chunk = new_paint_chunks_.PaintChunkAt(index);
      if (chunk.id == id) {
        ShowDebugData();
        NOTREACHED() << "New paint chunk id " << id
                     << " has duplicated id with previous chuck " << chunk;
      }
    }
  }
}
#endif

size_t PaintController::sum_num_items_ = 0;
size_t PaintController::sum_num_cached_items_ = 0;
size_t PaintController::sum_num_subsequences_ = 0;
size_t PaintController::sum_num_cached_subsequences_ = 0;

void PaintController::UpdateUMACounts() {
  DCHECK_EQ(usage_, kMultiplePaints);
  sum_num_items_ += new_display_item_list_.size();
  sum_num_cached_items_ += num_cached_new_items_;
  sum_num_subsequences_ += new_cached_subsequences_.size();
  sum_num_cached_subsequences_ += num_cached_new_subsequences_;
}

void PaintController::UpdateUMACountsOnFullyCached() {
  DCHECK_EQ(usage_, kMultiplePaints);
  int num_items = GetDisplayItemList().size();
  sum_num_items_ += num_items;
  sum_num_cached_items_ += num_items;

  int num_subsequences = current_cached_subsequences_.size();
  sum_num_subsequences_ += num_subsequences;
  sum_num_cached_subsequences_ += num_subsequences;
}

void PaintController::ReportUMACounts() {
  if (sum_num_items_ == 0)
    return;

  UMA_HISTOGRAM_PERCENTAGE("Blink.Paint.CachedItemPercentage",
                           sum_num_cached_items_ * 100 / sum_num_items_);
  if (sum_num_subsequences_) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Blink.Paint.CachedSubsequencePercentage",
        sum_num_cached_subsequences_ * 100 / sum_num_subsequences_);
  }
  sum_num_items_ = 0;
  sum_num_cached_items_ = 0;
  sum_num_subsequences_ = 0;
  sum_num_cached_subsequences_ = 0;
}

}  // namespace blink
