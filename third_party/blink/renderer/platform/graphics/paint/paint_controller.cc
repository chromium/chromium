// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "cc/paint/paint_op.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_under_invalidation_checker.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/core/SkTextBlob.h"

#if DCHECK_IS_ON()
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "components/crash/core/common/crash_key.h"
#endif

namespace blink {

void PaintControllerPersistentData::InvalidateAllForTesting() {
  current_paint_artifact_ = MakeGarbageCollected<PaintArtifact>();
  current_subsequences_.map.clear();
  current_subsequences_.tree.clear();
  cache_is_all_invalid_ = true;
}

size_t PaintControllerPersistentData::ApproximateUnsharedMemoryUsage() const {
  size_t memory_usage = sizeof(*this);

  // Memory outside this class due to paint artifacts.
  memory_usage += current_paint_artifact_->ApproximateUnsharedMemoryUsage();

  // External objects, shared with the embedder, such as PaintRecord, should be
  // excluded to avoid double counting. It is the embedder's responsibility to
  // count such objects.

  // Memory outside this class due to current_subsequences_ and
  // new_subsequences_.
  memory_usage += current_subsequences_.map.Capacity() *
                  sizeof(decltype(current_subsequences_.map)::value_type);
  memory_usage += current_subsequences_.tree.CapacityInBytes();

  return memory_usage;
}

bool PaintControllerPersistentData::ClientCacheIsValid(
    const DisplayItemClient& client) const {
  if (cache_is_all_invalid_) {
    return false;
  }
  return client.IsValid();
}

wtf_size_t PaintControllerPersistentData::GetSubsequenceIndex(
    DisplayItemClientId client_id) const {
  auto result = current_subsequences_.map.find(client_id);
  if (result == current_subsequences_.map.end()) {
    return kNotFound;
  }
  DCHECK_EQ(client_id, current_subsequences_.tree[result->value].client_id);
  return result->value;
}

const SubsequenceMarkers* PaintControllerPersistentData::GetSubsequenceMarkers(
    DisplayItemClientId client_id) const {
  wtf_size_t index = GetSubsequenceIndex(client_id);
  if (index == kNotFound) {
    return nullptr;
  }
  return &current_subsequences_.tree[index];
}

void PaintControllerPersistentData::CommitNewDisplayItems(
    PaintArtifact& new_paint_artifact,
    SubsequencesData new_subsequences) {
  cache_is_all_invalid_ = false;
  DCHECK_EQ(new_subsequences.map.size(), new_subsequences.tree.size());
  current_paint_artifact_ = &new_paint_artifact;
  current_subsequences_ = std::move(new_subsequences);
}

PaintController::CounterForTesting* PaintController::counter_for_testing_ =
    nullptr;

PaintController::PaintController(bool record_debug_info,
                                 PaintControllerPersistentData* persistent_data,
                                 PaintBenchmarkMode benchmark_mode)
    : persistent_data_(persistent_data),
      paint_chunker_(new_paint_artifact_->GetPaintChunks(),
                     persistent_data_ ? &clients_to_validate_ : nullptr),
      record_debug_info_(record_debug_info),
      benchmark_mode_(benchmark_mode) {
  // frame_first_paints_ should have one null frame since the beginning, so
  // that PaintController is robust even if it paints outside of BeginFrame
  // and EndFrame cycles. It will also enable us to combine the first paint
  // data in this PaintController into another PaintController on which we
  // replay the recorded results in the future.
  frame_first_paints_.emplace_back(nullptr);

  // Reserve capacity.
  static constexpr wtf_size_t kDefaultDisplayItemListCapacity = 16;
  auto display_item_list_capacity = kDefaultDisplayItemListCapacity;
  if (persistent_data) {
    if (wtf_size_t current_list_size = CurrentDisplayItemList().size()) {
      display_item_list_capacity = current_list_size;
    }
    new_paint_artifact_->GetPaintChunks().reserve(CurrentPaintChunks().size());
    new_subsequences_.tree.reserve(CurrentSubsequences().tree.size());
    new_subsequences_.map.ReserveCapacityForSize(
        CurrentSubsequences().map.size());
  }
  new_paint_artifact_->GetDisplayItemList().ReserveCapacity(
      display_item_list_capacity);
}

PaintController::~PaintController() {
  if (!persistent_data_) {
    CHECK(clients_to_validate_.empty());
    return;
  }

  // CommitNewDisplayItems() should have been called.
  CHECK(!new_paint_artifact_);

  for (const auto& client : clients_to_validate_) {
    if (client->IsCacheable()) {
      client->Validate();
    }
  }
  for (auto& chunk : CurrentPaintChunks()) {
    chunk.client_is_just_created = false;
  }

  // After the cycle scope, the old paint artifact should not be referenced by
  // anyone and the backing store can be freed. This is especially helpful
  // to reduce overhead of GC. See crbug.com/330072757.
  if (old_paint_artifact_) {
    CHECK_NE(old_paint_artifact_, &CurrentPaintArtifact());
    old_paint_artifact_->clear();
  }
}

void PaintController::EnsureChunk() {
  if (paint_chunker_.EnsureChunk())
    CheckNewChunk();
}

void PaintController::RecordHitTestData(const DisplayItemClient& client,
                                        const gfx::Rect& rect,
                                        TouchAction touch_action,
                                        bool blocking_wheel,
                                        cc::HitTestOpaqueness opaqueness,
                                        DisplayItem::Type type) {
  if (rect.IsEmpty())
    return;
  PaintChunk::Id id(client.Id(), type, current_fragment_);
  CheckNewChunkId(id);
  ValidateNewChunkClient(client);
  if (paint_chunker_.AddHitTestDataToCurrentChunk(
          id, client, rect, touch_action, blocking_wheel, opaqueness)) {
    CheckNewChunk();
  }
}

void PaintController::RecordRegionCaptureData(
    const DisplayItemClient& client,
    const RegionCaptureCropId& crop_id,
    const gfx::Rect& rect) {
  DCHECK(!crop_id->is_zero());
  PaintChunk::Id id(client.Id(), DisplayItem::kRegionCapture,
                    current_fragment_);
  CheckNewChunkId(id);
  ValidateNewChunkClient(client);
  if (paint_chunker_.AddRegionCaptureDataToCurrentChunk(id, client, crop_id,
                                                        rect))
    CheckNewChunk();
}

void PaintController::RecordScrollHitTestData(
    const DisplayItemClient& client,
    DisplayItem::Type type,
    const TransformPaintPropertyNode* scroll_translation,
    const gfx::Rect& scroll_hit_test_rect,
    cc::HitTestOpaqueness hit_test_opaqueness,
    const gfx::Rect& scrolling_contents_cull_rect) {
  PaintChunk::Id id(client.Id(), type, current_fragment_);
  CheckNewChunkId(id);
  ValidateNewChunkClient(client);
  paint_chunker_.CreateScrollHitTestChunk(
      id, client, scroll_translation, scroll_hit_test_rect, hit_test_opaqueness,
      scrolling_contents_cull_rect);
  CheckNewChunk();
}

void PaintController::RecordSelection(
    std::optional<PaintedSelectionBound> start,
    std::optional<PaintedSelectionBound> end,
    String debug_info) {
  DCHECK(start.has_value() || end.has_value());
  paint_chunker_.AddSelectionToCurrentChunk(start, end, debug_info);
}

bool PaintController::UseCachedItemIfPossible(const DisplayItemClient& client,
                                              DisplayItem::Type type) {
  last_matching_item_ = kNotFound;
  last_matching_client_invalidation_reason_ = PaintInvalidationReason::kNone;

  if (!persistent_data_) {
    return false;
  }

  if (benchmark_mode_ == PaintBenchmarkMode::kCachingDisabled)
    return false;

  if (IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of a subsequence enclosing this
    // display item. Let the client continue to actually paint the display item.
    return false;
  }

  DisplayItem::Id id(client.Id(), type, current_fragment_);
#if DCHECK_IS_ON()
  last_checked_cached_item_id_ = id.AsHashKey();
#endif

  if (client.IsJustCreated() || !client.IsCacheable()) {
    return false;
  }

  auto cached_item = FindCachedItem(client, id);
  if (cached_item == kNotFound) {
    // See FindOutOfOrderCachedItemForward() for explanation of the situation.
    return false;
  }

  next_item_to_match_ = cached_item + 1;
  // Items before |next_item_to_match_| have been matched so we don't need to
  // index them.
  if (next_item_to_match_ > next_item_to_index_)
    next_item_to_index_ = next_item_to_match_;

  if (!ClientCacheIsValid(client)) {
    last_matching_item_ = cached_item;
    last_matching_client_invalidation_reason_ =
        client.GetPaintInvalidationReason();
    return false;
  }

  ++num_cached_new_items_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EnsureUnderInvalidationChecker().WouldUseCachedItem(cached_item);
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  DisplayItem& new_item =
      new_paint_artifact_->GetDisplayItemList().AppendByMoving(
          CurrentDisplayItemList()[cached_item]);
  new_item.SetPaintInvalidationReason(PaintInvalidationReason::kNone);
  ProcessNewItem(client, new_item);

  return true;
}

#if DCHECK_IS_ON()
void PaintController::AssertLastCheckedCachedItem(
    const DisplayItemClient& client,
    DisplayItem::Type type) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      benchmark_mode_ >= PaintBenchmarkMode::kForcePaint || !persistent_data_) {
    return;
  }

  DCHECK(last_checked_cached_item_id_ ==
         DisplayItem::Id(client.Id(), type, current_fragment_).AsHashKey());
}
#endif

sk_sp<SkTextBlob> PaintController::CachedTextBlob() const {
  if (!IsNonLayoutFullPaintInvalidationReason(
          last_matching_client_invalidation_reason_)) {
    return nullptr;
  }
  const auto* cached_item =
      DynamicTo<DrawingDisplayItem>(MatchingCachedItemToBeRepainted());
  if (!cached_item) {
    return nullptr;
  }
  const PaintRecord& record = cached_item->GetPaintRecord();
  if (record.size() != 1) {
    return nullptr;
  }
  const cc::PaintOp& op = record.GetFirstOp();
  if (op.GetType() != cc::PaintOpType::kDrawTextBlob) {
    return nullptr;
  }
  return static_cast<const cc::DrawTextBlobOp&>(op).blob;
}

const DisplayItem* PaintController::MatchingCachedItemToBeRepainted() const {
  if (last_matching_item_ == kNotFound) {
    return nullptr;
  }
  const DisplayItem& item = CurrentDisplayItemList()[last_matching_item_];
  DCHECK(!item.IsTombstone());
  return &item;
}

bool PaintController::UseCachedSubsequenceIfPossible(
    const DisplayItemClient& client) {
  if (!persistent_data_) {
    return false;
  }

  if (benchmark_mode_ == PaintBenchmarkMode::kCachingDisabled ||
      benchmark_mode_ == PaintBenchmarkMode::kSubsequenceCachingDisabled) {
    return false;
  }

  if (IsCheckingUnderInvalidation()) {
    // We are checking under-invalidation of an ancestor subsequence enclosing
    // this one. The ancestor subsequence is supposed to have already "copied",
    // so we should let the client continue to actually paint the descendant
    // subsequences without "copying".
    ++num_cached_new_subsequences_;
    return false;
  }

  if (client.IsJustCreated() || !client.IsCacheable()) {
    return false;
  }

  wtf_size_t subsequence_index = GetSubsequenceIndex(client.Id());
  if (subsequence_index == kNotFound)
    return false;

  const auto& markers = CurrentSubsequences().tree[subsequence_index];
  DCHECK_EQ(markers.client_id, client.Id());
  wtf_size_t start_item_index =
      CurrentPaintChunks()[markers.start_chunk_index].begin_index;
  wtf_size_t end_item_index =
      CurrentPaintChunks()[markers.end_chunk_index - 1].end_index;
  if (end_item_index > start_item_index &&
      CurrentDisplayItemList()[start_item_index].IsTombstone()) {
    // The subsequence has already been matched, indicating that the same client
    // created multiple subsequences. If DCHECK_IS_ON(), then we should have
    // encountered the DCHECK at the end of EndSubsequence() during the previous
    // paint.
    DUMP_WILL_BE_NOTREACHED();
    return false;
  }

  if (next_item_to_match_ == start_item_index) {
    // We are matching new and cached display items sequentially. Skip the
    // subsequence for later sequential matching of individual display items.
    next_item_to_match_ = end_item_index;
    // Items before |next_item_to_match_| have been matched so we don't need to
    // index them.
    if (next_item_to_match_ > next_item_to_index_)
      next_item_to_index_ = next_item_to_match_;
  }

  if (!ClientCacheIsValid(client))
    return false;

  num_cached_new_items_ += end_item_index - start_item_index;
  ++num_cached_new_subsequences_;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EnsureUnderInvalidationChecker().WouldUseCachedSubsequence(client.Id());
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  // This subsequence was moved from the cache, so client must already be
  // valid, hence we don't call MarkClientForValidation(client).
  AppendSubsequenceByMoving(client, subsequence_index,
                            markers.start_chunk_index, markers.end_chunk_index);
  return true;
}

wtf_size_t PaintController::BeginSubsequence(const DisplayItemClient& client) {
  RecordDebugInfo(client);
  // Force new paint chunk which is required for subsequence caching.
  paint_chunker_.SetWillForceNewChunk();
  new_subsequences_.tree.push_back(
      SubsequenceMarkers{client.Id(), NumNewChunks()});
  return new_subsequences_.tree.size() - 1;
}

void PaintController::EndSubsequence(wtf_size_t subsequence_index) {
  auto& markers = new_subsequences_.tree[subsequence_index];

  if (IsCheckingUnderInvalidation()) {
    under_invalidation_checker_->WillEndSubsequence(markers.client_id,
                                                    markers.start_chunk_index);
  }

  wtf_size_t end_chunk_index = NumNewChunks();
  if (markers.start_chunk_index == end_chunk_index) {
    // Omit the empty subsequence. The WillForceNewChunk flag set in
    // BeginSubsequence() still applies, but it's useful to reduce churns of
    // raster invalidation and compositing when the subsequence switches between
    // empty and non-empty.
    new_subsequences_.tree.pop_back();
    return;
  }

  // Force new paint chunk which is required for subsequence caching.
  paint_chunker_.SetWillForceNewChunk();

#if DCHECK_IS_ON()
  DCHECK(!new_subsequences_.map.Contains(markers.client_id))
      << "Multiple subsequences for client: "
      << new_paint_artifact_->ClientDebugName(markers.client_id);

  // Check tree integrity.
  if (subsequence_index > 0) {
    DCHECK_GE(markers.start_chunk_index,
              new_subsequences_.tree[subsequence_index - 1].end_chunk_index);
  }
  for (auto i = subsequence_index + 1; i < new_subsequences_.tree.size(); i++) {
    auto& child_markers = new_subsequences_.tree[i];
    DCHECK_GE(child_markers.start_chunk_index, markers.start_chunk_index);
    DCHECK_LE(child_markers.end_chunk_index, end_chunk_index);
  }
#endif

  new_subsequences_.map.insert(markers.client_id, subsequence_index);
  markers.end_chunk_index = end_chunk_index;
}

void PaintController::CheckNewItem(DisplayItem& display_item) {
  if (!persistent_data_) {
    return;
  }

#if DCHECK_IS_ON()
  if (display_item.IsCacheable()) {
    DEFINE_STATIC_LOCAL(std::optional<PaintChunk::Id>, last_duplicated_id, ());
    DEFINE_STATIC_LOCAL(std::optional<base::debug::StackTrace>, previous_stack,
                        ());
    auto id = display_item.GetId();
    if (id == last_duplicated_id) {
      previous_stack.emplace();
    }

    auto& new_display_item_list = new_paint_artifact_->GetDisplayItemList();
    auto index = FindItemFromIdIndexMap(id, new_display_item_id_index_map_,
                                        new_display_item_list);
    if (index != kNotFound) {
      WTF::TextStream ts;
      const auto& chunks = new_paint_artifact_->GetPaintChunks();
      auto chunk =
          std::upper_bound(chunks.begin(), chunks.end(), index,
                           [](wtf_size_t index, const PaintChunk& chunk) {
                             return index < chunk.end_index;
                           });
      DCHECK_NE(chunk, chunks.end());
      ts << "DisplayItem "
         << display_item.AsDebugString(*new_paint_artifact_).Utf8()
         << " (index="
         << new_display_item_list.size()
         // The last chunk might not be the chunk the new item would
         // be in because the new item might start a new chunk.
         << " last chunk "
         << chunks.back()
                .ToString(*new_paint_artifact_, /*concise=*/true)
                .Utf8()
         << ")\n has duplicated id with previous "
         << new_display_item_list[index]
                .AsDebugString(*new_paint_artifact_)
                .Utf8()
         << " (index=" << index << " in chunk "
         << chunk->ToString(*new_paint_artifact_, /*concise=*/true).Utf8()
         << ")";
      std::string message = ts.Release().Utf8();
      LOG(ERROR) << message;
      std::string debug_data = DebugDataAsString().Utf8();
      LOG(ERROR) << debug_data;
      // Don't crash on the first encounter of duplicated item id. To collect
      // more data, we expect the issue will reproduce during the next paint
      // cycle, which will crash with more data.
      if (previous_stack) {
        if (debug_data.length() > 1024) {
          debug_data = debug_data.substr(debug_data.length() - 1024);
        }
        SCOPED_CRASH_KEY_STRING1024("DupItemId", "DebugData", debug_data);
        static crash_reporter::CrashKeyString<1024> previous_stack_key(
            "DupItemId-PrevStack");
        LOG(ERROR) << "previous stack: " << previous_stack->ToString();
        crash_reporter::SetCrashKeyStringToStackTrace(&previous_stack_key,
                                                      *previous_stack);
        NOTREACHED_IN_MIGRATION() << message;
      }
    }
    AddToIdIndexMap(id, new_display_item_list.size() - 1,
                    new_display_item_id_index_map_);
  }
#endif

  if (IsCheckingUnderInvalidation())
    under_invalidation_checker_->CheckNewItem();
}

void PaintController::MarkClientForValidation(const DisplayItemClient& client) {
  if (persistent_data_ && !client.IsMarkedForValidation()) {
    clients_to_validate_.push_back(&client);
    client.MarkForValidation();
  }
}

void PaintController::ProcessNewItem(const DisplayItemClient& client,
                                     DisplayItem& display_item) {
  if (IsSkippingCache() && persistent_data_) {
    client.Invalidate(PaintInvalidationReason::kUncacheable);
    display_item.SetPaintInvalidationReason(
        PaintInvalidationReason::kUncacheable);
  }

  RecordDebugInfo(client);
  if (paint_chunker_.IncrementDisplayItemIndex(client, display_item))
    CheckNewChunk();

  if (!frame_first_paints_.back().first_painted && display_item.IsDrawing() &&
      // Here we ignore all document-background paintings because we don't
      // know if the background is default. ViewPainter should have called
      // setFirstPainted() if this display item is for non-default
      // background.
      display_item.GetType() != DisplayItem::kDocumentBackground &&
      display_item.DrawsContent()) {
    SetFirstPainted();
  }

  CheckNewItem(display_item);
}

void PaintController::CheckNewChunkId(const PaintChunk::Id& id) {
#if DCHECK_IS_ON()
  if (DisplayItem::IsForeignLayerType(id.type))
    return;

  DEFINE_STATIC_LOCAL(std::optional<PaintChunk::Id>, last_duplicated_id, ());
  DEFINE_STATIC_LOCAL(std::optional<base::debug::StackTrace>, previous_stack,
                      ());
  if (id == last_duplicated_id) {
    previous_stack.emplace();
  }
  auto it = new_paint_chunk_id_index_map_.find(id.AsHashKey());
  if (it != new_paint_chunk_id_index_map_.end()) {
    WTF::TextStream ts;
    ts << "New paint chunk id " << id.ToString(*new_paint_artifact_)
       << " is already used by a previous chuck "
       << new_paint_artifact_->GetPaintChunks()[it->value].ToString(
              *new_paint_artifact_);
    std::string message = ts.Release().Utf8();
    LOG(ERROR) << message;
    std::string debug_data = DebugDataAsString().Utf8();
    LOG(ERROR) << debug_data;
    // Don't crash on the first encounter of duplicated chunk id. To collect
    // more data, we expect the issue will reproduce during the next paint
    // cycle, which will crash with more data.
    if (previous_stack) {
      if (debug_data.length() > 1024) {
        debug_data = debug_data.substr(debug_data.length() - 1024);
      }
      SCOPED_CRASH_KEY_STRING1024("DupChunkId", "DebugData", debug_data);
      static crash_reporter::CrashKeyString<1024> previous_stack_key(
          "DupChunkId-PrevStack");
      LOG(ERROR) << "previous stack: " << previous_stack->ToString();
      crash_reporter::SetCrashKeyStringToStackTrace(&previous_stack_key,
                                                    *previous_stack);
      DUMP_WILL_BE_NOTREACHED() << message;
    }
    last_duplicated_id.emplace(id.client_id, id.type, id.fragment);
  }
#endif
}

void PaintController::CheckNewChunk() {
#if DCHECK_IS_ON()
  if (persistent_data_) {
    auto& chunks = new_paint_artifact_->GetPaintChunks();
    if (chunks.back().is_cacheable) {
      AddToIdIndexMap(chunks.back().id, chunks.size() - 1,
                      new_paint_chunk_id_index_map_);
    }
  }
#endif

  if (IsCheckingUnderInvalidation())
    under_invalidation_checker_->CheckNewChunk();
}

void PaintController::RecordDebugInfo(const DisplayItemClient& client) {
  if (record_debug_info_) {
    new_paint_artifact_->RecordDebugInfo(client.Id(), client.DebugName(),
                                         client.OwnerNodeId());
  }
}

void PaintController::UpdateCurrentPaintChunkProperties(
    const PaintChunk::Id& id,
    const DisplayItemClient& client,
    const PropertyTreeStateOrAlias& properties) {
  PaintChunk::Id id_with_fragment(id, current_fragment_);
  CheckNewChunkId(id_with_fragment);
  ValidateNewChunkClient(client);
  paint_chunker_.UpdateCurrentPaintChunkProperties(id_with_fragment, client,
                                                   properties);
}

void PaintController::UpdateCurrentPaintChunkProperties(
    const PropertyTreeStateOrAlias& properties) {
  paint_chunker_.UpdateCurrentPaintChunkProperties(properties);
}

bool PaintController::ClientCacheIsValid(
    const DisplayItemClient& client) const {
  if (IsSkippingCache()) {
    return false;
  }
  return persistent_data_->ClientCacheIsValid(client);
}

wtf_size_t PaintController::FindItemFromIdIndexMap(
    const DisplayItem::Id& id,
    const IdIndexMap& display_item_id_index_map,
    const DisplayItemList& list) {
  auto it = display_item_id_index_map.find(id.AsHashKey());
  if (it == display_item_id_index_map.end())
    return kNotFound;

  wtf_size_t index = it->value;
  const DisplayItem& existing_item = list[index];
  if (existing_item.IsTombstone())
    return kNotFound;
  DCHECK_EQ(existing_item.GetId(), id);
  DCHECK(existing_item.IsCacheable());
  return index;
}

void PaintController::AddToIdIndexMap(const DisplayItem::Id& id,
                                      wtf_size_t index,
                                      IdIndexMap& map) {
  DCHECK(!map.Contains(id.AsHashKey()));
  map.insert(id.AsHashKey(), index);
}

wtf_size_t PaintController::FindCachedItem(const DisplayItemClient& client,
                                           const DisplayItem::Id& id) {
  if (next_item_to_match_ < CurrentDisplayItemList().size()) {
    // If current_list[next_item_to_match_] matches the new item, we don't need
    // to update and lookup the index, which is fast. This is the common case
    // that the current list and the new list are in the same order around the
    // new item.
    const DisplayItem& item = CurrentDisplayItemList()[next_item_to_match_];
    // We encounter an item that has already been moved which indicates we
    // can't do sequential matching.
    if (!item.IsTombstone() && id == item.GetId()) {
#if DCHECK_IS_ON()
      ++num_sequential_matches_;
#endif
      return next_item_to_match_;
    }
  }

  wtf_size_t found_index = FindItemFromIdIndexMap(
      id, out_of_order_item_id_index_map_, CurrentDisplayItemList());
  if (found_index != kNotFound) {
#if DCHECK_IS_ON()
    ++num_out_of_order_matches_;
#endif
    return found_index;
  }

  return FindOutOfOrderCachedItemForward(client, id);
}

// Find forward for the item and index all skipped indexable items.
wtf_size_t PaintController::FindOutOfOrderCachedItemForward(
    const DisplayItemClient& client,
    const DisplayItem::Id& id) {
  for (auto i = next_item_to_index_; i < CurrentDisplayItemList().size(); ++i) {
    const DisplayItem& item = CurrentDisplayItemList()[i];
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
      AddToIdIndexMap(item.GetId(), i, out_of_order_item_id_index_map_);
      next_item_to_index_ = i + 1;
    }
  }

#if DCHECK_IS_ON()
  // The display item newly appears while the client is not invalidated. The
  // situation alone (without other kinds of under-invalidations) won't corrupt
  // rendering, but causes AddItemToIndexIfNeeded() for all remaining display
  // item, which is not the best for performance. In this case, the caller
  // should fall back to repaint the display item.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      ClientCacheIsValid(client)) {
    // Ensure our paint invalidation tests don't trigger the less performant
    // situation which should be rare.
    RecordDebugInfo(client);
    DLOG(WARNING) << "Can't find cached display item: "
                  << id.ToString(*new_paint_artifact_);
    ShowDebugData();
  }
#endif
  return kNotFound;
}

void PaintController::AppendSubsequenceByMoving(const DisplayItemClient& client,
                                                wtf_size_t subsequence_index,
                                                wtf_size_t start_chunk_index,
                                                wtf_size_t end_chunk_index) {
#if DCHECK_IS_ON()
  DCHECK(!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  DCHECK_EQ(client.Id(),
            CurrentSubsequences().tree[subsequence_index].client_id);
  DCHECK_GT(end_chunk_index, start_chunk_index);
  auto properties_before_subsequence = CurrentPaintChunkProperties();
#endif

  auto new_start_chunk_index = NumNewChunks();
  auto new_subsequence_index = BeginSubsequence(client);

  auto& current_chunks = CurrentPaintChunks();
  for (auto chunk_index = start_chunk_index; chunk_index < end_chunk_index;
       ++chunk_index) {
    auto& cached_chunk = current_chunks[chunk_index];
    CheckNewChunkId(cached_chunk.id);
    paint_chunker_.AppendByMoving(std::move(cached_chunk));
    if (record_debug_info_) {
      auto& new_chunk = new_paint_artifact_->GetPaintChunks().back();
      new_paint_artifact_->RecordDebugInfo(
          new_chunk.id.client_id,
          CurrentPaintArtifact().ClientDebugName(new_chunk.id.client_id),
          CurrentPaintArtifact().ClientOwnerNodeId(new_chunk.id.client_id));
    }
    CheckNewChunk();
  }

  auto& new_display_item_list = new_paint_artifact_->GetDisplayItemList();
  wtf_size_t new_item_start_index = new_display_item_list.size();
  new_display_item_list.AppendSubsequenceByMoving(
      CurrentDisplayItemList(), current_chunks[start_chunk_index].begin_index,
      current_chunks[end_chunk_index - 1].end_index);

  bool skip_cache = IsSkippingCache();
  for (auto& item : new_display_item_list.ItemsInRange(
           new_item_start_index, new_display_item_list.size())) {
    DCHECK(!item.IsTombstone());
    // This item was copied from the cache, so client must already be valid,
    // hence we don't call MarkClientForValidation(client).
    item.SetPaintInvalidationReason(skip_cache || !item.IsCacheable()
                                        ? PaintInvalidationReason::kUncacheable
                                        : PaintInvalidationReason::kNone);
    if (record_debug_info_) {
      new_paint_artifact_->RecordDebugInfo(
          item.ClientId(),
          CurrentPaintArtifact().ClientDebugName(item.ClientId()),
          CurrentPaintArtifact().ClientOwnerNodeId(item.ClientId()));
    }
#if DCHECK_IS_ON()
    CheckNewItem(item);
#endif
  }

  // Keep descendant subsequence entries.
  for (wtf_size_t i = subsequence_index + 1;
       i < CurrentSubsequences().tree.size(); i++) {
    auto& markers = CurrentSubsequences().tree[i];
    if (markers.start_chunk_index >= end_chunk_index)
      break;
    DCHECK(!new_subsequences_.map.Contains(markers.client_id))
        << "Multiple subsequences for client: "
        << CurrentPaintArtifact().ClientDebugName(markers.client_id);
    new_subsequences_.map.insert(markers.client_id,
                                 new_subsequences_.tree.size());
    new_subsequences_.tree.push_back(SubsequenceMarkers{
        markers.client_id,
        markers.start_chunk_index + new_start_chunk_index - start_chunk_index,
        markers.end_chunk_index + new_start_chunk_index - start_chunk_index,
        /*is_moved_from_cached_subsequence*/ true});
    ++num_cached_new_subsequences_;
  }

  EndSubsequence(new_subsequence_index);
  new_subsequences_.tree[new_subsequence_index]
      .is_moved_from_cached_subsequence = true;

#if DCHECK_IS_ON()
  DCHECK_EQ(properties_before_subsequence, CurrentPaintChunkProperties());
#endif
}

DISABLE_CFI_PERF
const PaintArtifact& PaintController::CommitNewDisplayItems() {
  TRACE_EVENT2(
      "blink,benchmark", "PaintController::commitNewDisplayItems",
      "current_display_list_size",
      persistent_data_ ? CurrentDisplayItemList().size() : 0,
      "num_non_cached_new_items",
      new_paint_artifact_->GetDisplayItemList().size() - num_cached_new_items_);

  if (counter_for_testing_) {
    counter_for_testing_->num_cached_items += num_cached_new_items_;
    counter_for_testing_->num_cached_subsequences +=
        num_cached_new_subsequences_;
  }

  paint_chunker_.Finish();

  PaintArtifact& paint_artifact = *new_paint_artifact_;
  // Any new paint operation will crash on nullptr.
  new_paint_artifact_ = nullptr;
  if (persistent_data_) {
    old_paint_artifact_ = persistent_data_->current_paint_artifact_.Get();
    persistent_data_->CommitNewDisplayItems(paint_artifact,
                                            std::move(new_subsequences_));
  }

#if DCHECK_IS_ON()
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "PaintController::CommitNewDisplayItems() completed";
    if (VLOG_IS_ON(3)) {
      ShowDebugDataWithPaintRecords();
    } else if (VLOG_IS_ON(2)) {
      ShowDebugData();
    } else if (VLOG_IS_ON(1)) {
      ShowCompactDebugData();
    }
  }
#endif
  return paint_artifact;
}

PaintUnderInvalidationChecker&
PaintController::EnsureUnderInvalidationChecker() {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  if (!under_invalidation_checker_) {
    under_invalidation_checker_.emplace(*this);
  }
  return *under_invalidation_checker_;
}

bool PaintController::IsCheckingUnderInvalidationForTesting() const {
  return IsCheckingUnderInvalidation();
}

bool PaintController::IsCheckingUnderInvalidation() const {
  if (under_invalidation_checker_) {
    DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
    return under_invalidation_checker_->IsChecking();
  }
  return false;
}

void PaintController::SetFirstPainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
    frame_first_paints_.back().first_painted = true;
}

void PaintController::SetTextPainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
    frame_first_paints_.back().text_painted = true;
}

void PaintController::SetImagePainted() {
  if (!IgnorePaintTimingScope::IgnoreDepth())
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

void PaintController::ValidateNewChunkClient(const DisplayItemClient& client) {
  RecordDebugInfo(client);
  if (IsSkippingCache() && persistent_data_) {
    client.Invalidate(PaintInvalidationReason::kUncacheable);
  }
}

}  // namespace blink
