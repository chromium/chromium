// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_under_invalidation_checker.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PaintUnderInvalidationChecker::PaintUnderInvalidationChecker(
    PaintController& paint_controller)
    : paint_controller_(paint_controller) {
#if DCHECK_IS_ON()
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  DCHECK(paint_controller_.persistent_data_);
#endif
}

PaintUnderInvalidationChecker::~PaintUnderInvalidationChecker() {
  DCHECK(!IsChecking());
}

bool PaintUnderInvalidationChecker::IsChecking() const {
  if (old_item_index_ != kNotFound) {
    DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
    DCHECK(subsequence_client_id_ == kInvalidDisplayItemClientId ||
           (old_chunk_index_ != kNotFound && new_chunk_index_ != kNotFound));
    return true;
  }

  DCHECK_EQ(subsequence_client_id_, kInvalidDisplayItemClientId);
  DCHECK_EQ(old_chunk_index_, kNotFound);
  DCHECK_EQ(new_chunk_index_, kNotFound);
  return false;
}

bool PaintUnderInvalidationChecker::IsCheckingSubsequence() const {
  if (subsequence_client_id_ != kInvalidDisplayItemClientId) {
    DCHECK(IsChecking());
    return true;
  }
  return false;
}

void PaintUnderInvalidationChecker::Stop() {
  DCHECK(IsChecking());
  old_chunk_index_ = kNotFound;
  new_chunk_index_ = kNotFound;
  old_item_index_ = kNotFound;
  subsequence_client_id_ = kInvalidDisplayItemClientId;
}

void PaintUnderInvalidationChecker::WouldUseCachedItem(
    wtf_size_t old_item_index) {
  DCHECK(!IsChecking());
  old_item_index_ = old_item_index;
}

void PaintUnderInvalidationChecker::CheckNewItem() {
  DCHECK(IsChecking());

  if (paint_controller_.IsSkippingCache()) {
    // We allow cache skipping and temporary under-invalidation in cached
    // subsequences. See the usage of DisplayItemCacheSkipper in BoxPainter.
    Stop();
    // Match the remaining display items in the subsequence normally.
    paint_controller_.next_item_to_match_ = old_item_index_;
    paint_controller_.next_item_to_index_ = old_item_index_;
    return;
  }

  const auto& new_item = NewDisplayItemList().back();
  if (old_item_index_ >= OldDisplayItemList().size())
    ShowItemError("extra display item", new_item);

  auto& old_item = OldDisplayItemList()[old_item_index_];
  if (!new_item.EqualsForUnderInvalidation(old_item))
    ShowItemError("display item changed", new_item, &old_item);

  // Discard the forced repainted display item and move the cached item into
  // new_display_item_list_. This is to align with the
  // non-under-invalidation-checking path to empty the original cached slot,
  // leaving only disappeared or invalidated display items in the old list after
  // painting.
  NewDisplayItemList().ReplaceLastByMoving(old_item);
  NewDisplayItemList().back().SetPaintInvalidationReason(
      old_item.IsCacheable() ? PaintInvalidationReason::kNone
                             : PaintInvalidationReason::kUncacheable);

  if (subsequence_client_id_ != kInvalidDisplayItemClientId) {
    // We are checking under-invalidation of a cached subsequence.
    ++old_item_index_;
  } else {
    // We have checked the single item for under-invalidation.
    Stop();
  }
}

void PaintUnderInvalidationChecker::WouldUseCachedSubsequence(
    DisplayItemClientId client_id) {
  DCHECK(!IsChecking());

  const auto* markers = paint_controller_.GetSubsequenceMarkers(client_id);
  DCHECK(markers);
  old_chunk_index_ = markers->start_chunk_index;
  new_chunk_index_ = NewPaintChunks().size();
  old_item_index_ = OldPaintChunks()[markers->start_chunk_index].begin_index;
  subsequence_client_id_ = client_id;
}

void PaintUnderInvalidationChecker::CheckNewChunk() {
  DCHECK(IsChecking());
  if (!IsCheckingSubsequence())
    return;

  if (NewPaintChunks().size() > new_chunk_index_ + 1) {
    // Check the previous new chunk (pointed by new_chunk_index_, before the
    // just added chunk) which is now complete. The just added chunk will be
    // checked when it's complete later in CheckNewChunk() or
    // WillEndSubsequence().
    CheckNewChunkInternal();
  }
}

void PaintUnderInvalidationChecker::WillEndSubsequence(
    DisplayItemClientId client_id,
    wtf_size_t start_chunk_index) {
  DCHECK(IsChecking());
  if (!IsCheckingSubsequence())
    return;

  const auto* markers = paint_controller_.GetSubsequenceMarkers(client_id);
  if (!markers) {
    if (start_chunk_index != NewPaintChunks().size())
      ShowSubsequenceError("unexpected subsequence", client_id);
  } else if (markers->end_chunk_index - markers->start_chunk_index !=
             NewPaintChunks().size() - start_chunk_index) {
    ShowSubsequenceError("new subsequence wrong length", client_id);
  } else {
    // Now we know that the last chunk in the subsequence is complete. See also
    // CheckNewChunk().
    auto end_chunk_index = NewPaintChunks().size();
    if (new_chunk_index_ < end_chunk_index) {
      DCHECK_EQ(new_chunk_index_ + 1, end_chunk_index);
      CheckNewChunkInternal();
      DCHECK_EQ(new_chunk_index_, end_chunk_index);
    }
  }

  if (subsequence_client_id_ == client_id)
    Stop();
}

void PaintUnderInvalidationChecker::CheckNewChunkInternal() {
  DCHECK_NE(subsequence_client_id_, kInvalidDisplayItemClientId);
  const auto* markers =
      paint_controller_.GetSubsequenceMarkers(subsequence_client_id_);
  DCHECK(markers);
  const auto& new_chunk = NewPaintChunks()[new_chunk_index_];
  if (old_chunk_index_ >= markers->end_chunk_index) {
    ShowSubsequenceError("extra chunk", kInvalidDisplayItemClientId,
                         &new_chunk);
  } else {
    const auto& old_chunk = OldPaintChunks()[old_chunk_index_];
    if (!old_chunk.EqualsForUnderInvalidationChecking(new_chunk)) {
      ShowSubsequenceError("chunk changed", kInvalidDisplayItemClientId,
                           &new_chunk, &old_chunk);
    }
  }
  new_chunk_index_++;
  old_chunk_index_++;
}

void PaintUnderInvalidationChecker::ShowItemError(
    const char* reason,
    const DisplayItem& new_item,
    const DisplayItem* old_item) const {
  if (subsequence_client_id_ != kInvalidDisplayItemClientId) {
    LOG(ERROR) << "(In cached subsequence for "
               << paint_controller_.new_paint_artifact_->ClientDebugName(
                      subsequence_client_id_)
               << ")";
  }
  LOG(ERROR) << "Under-invalidation: " << reason;
#if DCHECK_IS_ON()
  LOG(ERROR) << "New display item: "
             << new_item.AsDebugString(*paint_controller_.new_paint_artifact_);
  if (old_item) {
    LOG(ERROR) << "Old display item: "
               << old_item->AsDebugString(
                      paint_controller_.CurrentPaintArtifact());
  }
  LOG(ERROR) << "See http://crbug.com/619103.";

  if (auto* new_drawing = DynamicTo<DrawingDisplayItem>(new_item)) {
    LOG(INFO) << "new record:\n"
              << RecordAsDebugString(new_drawing->GetPaintRecord()).Utf8();
  }
  if (auto* old_drawing = DynamicTo<DrawingDisplayItem>(old_item)) {
    LOG(INFO) << "old record:\n"
              << RecordAsDebugString(old_drawing->GetPaintRecord()).Utf8();
  }

  paint_controller_.ShowDebugData();
#else
  LOG(ERROR) << "Run a build with DCHECK on to get more details.";
#endif
  LOG(FATAL) << "See https://crbug.com/619103.";
}

void PaintUnderInvalidationChecker::ShowSubsequenceError(
    const char* reason,
    DisplayItemClientId client_id,
    const PaintChunk* new_chunk,
    const PaintChunk* old_chunk) {
  if (subsequence_client_id_ != kInvalidDisplayItemClientId) {
    LOG(ERROR) << "(In cached subsequence for "
               << paint_controller_.new_paint_artifact_->ClientDebugName(
                      subsequence_client_id_)
               << ")";
  }
  LOG(ERROR) << "Under-invalidation: " << reason;
  if (client_id != kInvalidDisplayItemClientId) {
    // |client_id| may be different from |subsequence_client_id_| if the error
    // occurs in a descendant subsequence of the cached subsequence.
    LOG(ERROR) << "Subsequence client: "
               << paint_controller_.new_paint_artifact_->ClientDebugName(
                      client_id);
  }
  if (new_chunk) {
    LOG(ERROR) << "New paint chunk: "
               << new_chunk->ToString(*paint_controller_.new_paint_artifact_);
  }
  if (old_chunk) {
    LOG(ERROR) << "Old paint chunk: "
               << old_chunk->ToString(paint_controller_.CurrentPaintArtifact());
  }
#if DCHECK_IS_ON()
  paint_controller_.ShowDebugData();
#else
  LOG(ERROR) << "Run a build with DCHECK on to get more details.";
#endif
  LOG(FATAL) << "See https://crbug.com/619103.";
}

const PaintChunks& PaintUnderInvalidationChecker::OldPaintChunks() const {
  return paint_controller_.CurrentPaintChunks();
}

const PaintChunks& PaintUnderInvalidationChecker::NewPaintChunks() const {
  return paint_controller_.new_paint_artifact_->GetPaintChunks();
}

DisplayItemList& PaintUnderInvalidationChecker::OldDisplayItemList() {
  return paint_controller_.CurrentDisplayItemList();
}

DisplayItemList& PaintUnderInvalidationChecker::NewDisplayItemList() {
  return paint_controller_.new_paint_artifact_->GetDisplayItemList();
}

}  // namespace blink
