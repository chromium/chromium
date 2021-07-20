// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_UNDER_INVALIDATION_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_UNDER_INVALIDATION_CHECKER_H_

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DisplayItem;
class DisplayItemClient;
class DisplayItemList;
class PaintController;
struct PaintChunk;

// If RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled(),
// when PaintController can use a cached display item or a cached subsequence,
// it lets the client paint instead of using the cache, and this class checks
// whether the painting is the same as the cache.
class PaintUnderInvalidationChecker {
 public:
  explicit PaintUnderInvalidationChecker(PaintController& paint_controller);
  ~PaintUnderInvalidationChecker();

  bool IsChecking() const;

  // Called from PaintController::UseCachedItemIfPossible() to inform that
  // PaintController would use a cached display item if we were not checking
  // under-invalidations.
  void WouldUseCachedItem(wtf_size_t old_item_index);
  void CheckNewItem();

  // Called from PaintController::UseCachedSubsequenceIfPossible() to inform
  // that PaintController would use a cached subsequence if we were not checking
  // under-invalidations.
  void WouldUseCachedSubsequence(const DisplayItemClient&);
  void CheckNewChunk();
  void WillEndSubsequence(const DisplayItemClient& client,
                          wtf_size_t start_chunk_index);

 private:
  bool IsCheckingSubsequence() const;
  void Stop();
  void CheckNewChunkInternal();
  void ShowItemError(const char* reason,
                     const DisplayItem& new_item,
                     const DisplayItem* old_item = nullptr) const;
  void ShowSubsequenceError(const char* reason,
                            const DisplayItemClient* = nullptr,
                            const PaintChunk* new_chunk = nullptr,
                            const PaintChunk* old_chunk = nullptr);

  const Vector<PaintChunk>& OldPaintChunks() const;
  const Vector<PaintChunk>& NewPaintChunks() const;
  DisplayItemList& OldDisplayItemList();
  DisplayItemList& NewDisplayItemList();

  PaintController& paint_controller_;

  // Points to the cached display item which is expected to match the nextnew
  // display item.
  wtf_size_t old_item_index_ = kNotFound;
  // Points to the cached paint chunk which is expected to match the next
  // complete new paint chunk.
  wtf_size_t old_chunk_index_ = kNotFound;
  // Points to the next new paint chunk which will be checked when it's
  // complete.
  wtf_size_t new_chunk_index_ = kNotFound;
  const DisplayItemClient* subsequence_client_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_UNDER_INVALIDATION_CHECKER_H_
