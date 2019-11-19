// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

static const size_t kInitialDisplayItemListCapacityBytes = 512;

// FrameFirstPaint stores first-paint, text or image painted for the
// corresponding frame. They are never reset to false. First-paint is defined in
// https://github.com/WICG/paint-timing. It excludes default background paint.
struct FrameFirstPaint {
  FrameFirstPaint(const void* frame)
      : frame(frame),
        first_painted(false),
        text_painted(false),
        image_painted(false) {}

  const void* frame;
  bool first_painted : 1;
  bool text_painted : 1;
  bool image_painted : 1;
};

// Responsible for processing display items as they are produced, and producing
// a final paint artifact when complete. This class includes logic for caching,
// cache invalidation, and merging.
class PLATFORM_EXPORT PaintController {
  USING_FAST_MALLOC(PaintController);

 public:
  enum Usage {
    // The PaintController will be used for multiple paint cycles. It caches
    // display item and subsequence of the previous paint which can be used in
    // subsequent paints.
    kMultiplePaints,
    // The PaintController will be used for one paint cycle only. It doesn't
    // cache or invalidate cache.
    kTransient,
  };

  explicit PaintController(Usage = kMultiplePaints);
  ~PaintController();

  // For pre-PaintAfterPaint only.
  void InvalidateAll();
  bool CacheIsAllInvalid() const;

  // These methods are called during painting.

  // Provide a new set of paint chunk properties to apply to recorded display
  // items.
  void UpdateCurrentPaintChunkProperties(
      const base::Optional<PaintChunk::Id>& id,
      const PropertyTreeState& properties) {
    if (id) {
      PaintChunk::Id id_with_fragment(*id, current_fragment_);
      UpdateCurrentPaintChunkPropertiesUsingIdWithFragment(id_with_fragment,
                                                           properties);
#if DCHECK_IS_ON()
      CheckDuplicatePaintChunkId(id_with_fragment);
#endif
    } else {
      new_paint_chunks_.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                          properties);
    }
  }

  const PropertyTreeState& CurrentPaintChunkProperties() const {
    return new_paint_chunks_.CurrentPaintChunkProperties();
  }

  PaintChunk& CurrentPaintChunk() { return new_paint_chunks_.LastChunk(); }

  void ForceNewChunk(const DisplayItemClient& client, DisplayItem::Type type) {
    new_paint_chunks_.ForceNewChunk();
    new_paint_chunks_.UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(client, type), CurrentPaintChunkProperties());
  }

  template <typename DisplayItemClass, typename... Args>
  void CreateAndAppend(Args&&... args) {
    static_assert(WTF::IsSubclass<DisplayItemClass, DisplayItem>::value,
                  "Can only createAndAppend subclasses of DisplayItem.");
    static_assert(
        sizeof(DisplayItemClass) <= kMaximumDisplayItemSize,
        "DisplayItem subclass is larger than kMaximumDisplayItemSize.");
    static_assert(kDisplayItemAlignment % alignof(DisplayItemClass) == 0,
                  "DisplayItem subclass alignment is not a factor of "
                  "kDisplayItemAlignment.");

    if (DisplayItemConstructionIsDisabled())
      return;

    EnsureNewDisplayItemListInitialCapacity();
    DisplayItemClass& display_item =
        new_display_item_list_.AllocateAndConstruct<DisplayItemClass>(
            std::forward<Args>(args)...);
    display_item.SetFragment(current_fragment_);
    ProcessNewItem(display_item);
  }

  // Tries to find the cached display item corresponding to the given
  // parameters. If found, appends the cached display item to the new display
  // list and returns true. Otherwise returns false.
  bool UseCachedItemIfPossible(const DisplayItemClient&, DisplayItem::Type);

  // Tries to find the cached subsequence corresponding to the given parameters.
  // If found, copies the cache subsequence to the new display list and returns
  // true. Otherwise returns false.
  bool UseCachedSubsequenceIfPossible(const DisplayItemClient&);

  size_t BeginSubsequence();
  // The |start| parameter should be the return value of the corresponding
  // BeginSubsequence().
  void EndSubsequence(const DisplayItemClient&, size_t start);

  void BeginSkippingCache() {
    if (usage_ == kTransient)
      return;
    ++skipping_cache_count_;
  }
  void EndSkippingCache() {
    if (usage_ == kTransient)
      return;
    DCHECK(skipping_cache_count_ > 0);
    --skipping_cache_count_;
  }
  bool IsSkippingCache() const {
    return usage_ == kTransient || skipping_cache_count_;
  }

  // Must be called when a painting is finished. Updates the current paint
  // artifact with the new paintings.
  void CommitNewDisplayItems();

  // Called when the caller finishes updating a full document life cycle.
  // The PaintController will cleanup data that will no longer be used for the
  // next cycle, and update status to be ready for the next cycle.
  // It updates caching status of DisplayItemClients, so if there are
  // DisplayItemClients painting on multiple PaintControllers, we should call
  // there FinishCycle() at the same time to ensure consistent caching status.
  void FinishCycle();

  // |FinishCycle| clears the property tree changed state but only does this for
  // non-transient controllers. Until CompositeAfterPaint, the root paint
  // controller is transient with and this function provides a hook for clearing
  // the property tree changed state after paint.
  // TODO(pdr): Remove this when CompositeAfterPaint ships.
  void ClearPropertyTreeChangedStateTo(const PropertyTreeState&);

  // Returns the approximate memory usage, excluding memory likely to be
  // shared with the embedder after copying to WebPaintController.
  // Should only be called after a full document life cycle update.
  size_t ApproximateUnsharedMemoryUsage() const;

  // Get the artifact generated after the last commit.
  const PaintArtifact& GetPaintArtifact() const {
#if DCHECK_IS_ON()
    DCHECK(new_display_item_list_.IsEmpty());
    DCHECK(new_paint_chunks_.IsInInitialState());
    DCHECK(current_paint_artifact_);
#endif
    return *current_paint_artifact_;
  }
  scoped_refptr<const PaintArtifact> GetPaintArtifactShared() const {
    return base::WrapRefCounted(&GetPaintArtifact());
  }
  const DisplayItemList& GetDisplayItemList() const {
    return GetPaintArtifact().GetDisplayItemList();
  }
  const Vector<PaintChunk>& PaintChunks() const {
    return GetPaintArtifact().PaintChunks();
  }

  // For micro benchmarking of record time. If true, display item construction
  // is disabled to isolate the costs of construction in performance metrics.
  bool DisplayItemConstructionIsDisabled() const {
    return construction_disabled_;
  }
  void SetDisplayItemConstructionIsDisabled(bool disable) {
    construction_disabled_ = disable;
  }

  // For micro benchmarking of record time. If true, subsequence caching is
  // disabled to test the cost of display item caching.
  bool SubsequenceCachingIsDisabled() const {
    return subsequence_caching_disabled_;
  }
  void SetSubsequenceCachingIsDisabled(bool disable) {
    subsequence_caching_disabled_ = disable;
  }

  void SetFirstPainted();
  void SetTextPainted();
  void SetImagePainted();

  // Returns DisplayItemList added using CreateAndAppend() since beginning or
  // the last CommitNewDisplayItems(). Use with care.
  DisplayItemList& NewDisplayItemList() { return new_display_item_list_; }

  void AppendDebugDrawingAfterCommit(sk_sp<const PaintRecord>,
                                     const PropertyTreeState&);

#if DCHECK_IS_ON()
  void ShowCompactDebugData() const;
  void ShowDebugData() const;
  void ShowDebugDataWithPaintRecords() const;
#endif

  void BeginFrame(const void* frame);
  FrameFirstPaint EndFrame(const void* frame);

  // The current fragment will be part of the ids of all display items and
  // paint chunks, to uniquely identify display items in different fragments
  // for the same client and type.
  unsigned CurrentFragment() const { return current_fragment_; }
  void SetCurrentFragment(unsigned fragment) { current_fragment_ = fragment; }

  // The client may skip a paint when nothing changed. In the case, the client
  // calls this method to update UMA counts as a fully cached paint.
  void UpdateUMACountsOnFullyCached();
  // Reports the accumulated counts as UMA metrics, and reset them, if we have
  // enough data to report.
  static void ReportUMACounts();

 private:
  friend class PaintControllerTestBase;
  friend class PaintControllerPaintTestBase;

  // True if all display items associated with the client are validly cached.
  // However, the current algorithm allows the following situations even if
  // ClientCacheIsValid() is true for a client during painting:
  // 1. The client paints a new display item that is not cached:
  //    UseCachedItemIfPossible() returns false for the display item and the
  //    newly painted display item will be added into the cache. This situation
  //    has slight performance hit (see FindOutOfOrderCachedItemForward()) so we
  //    print a warning in the situation and should keep it rare.
  // 2. the client no longer paints a display item that is cached: the cached
  //    display item will be removed. This doesn't affect performance.
  bool ClientCacheIsValid(const DisplayItemClient&) const;

  void InvalidateAllForTesting() { InvalidateAllInternal(); }
  void InvalidateAllInternal();

  void EnsureNewDisplayItemListInitialCapacity() {
    if (new_display_item_list_.IsEmpty()) {
      // TODO(wangxianzhu): Consider revisiting this heuristic.
      new_display_item_list_ = DisplayItemList(
          current_paint_artifact_->GetDisplayItemList().IsEmpty()
              ? kInitialDisplayItemListCapacityBytes
              : current_paint_artifact_->GetDisplayItemList()
                    .UsedCapacityInBytes());
    }
  }

  // Set new item state (cache skipping, etc) for a new item.
  void ProcessNewItem(DisplayItem&);
  DisplayItem& MoveItemFromCurrentListToNewList(size_t);

  // Maps clients to indices of display items or chunks of each client.
  using IndicesByClientMap = HashMap<const DisplayItemClient*, Vector<size_t>>;

  static size_t FindMatchingItemFromIndex(const DisplayItem::Id&,
                                          const IndicesByClientMap&,
                                          const DisplayItemList&);
  static void AddToIndicesByClientMap(const DisplayItemClient&,
                                      size_t index,
                                      IndicesByClientMap&);

  size_t FindCachedItem(const DisplayItem::Id&);
  size_t FindOutOfOrderCachedItemForward(const DisplayItem::Id&);
  void CopyCachedSubsequence(size_t begin_index, size_t end_index);

  void UpdateCurrentPaintChunkPropertiesUsingIdWithFragment(
      const PaintChunk::Id& id_with_fragment,
      const PropertyTreeState& properties) {
    new_paint_chunks_.UpdateCurrentPaintChunkProperties(id_with_fragment,
                                                        properties);
  }

  // Resets the indices (e.g. next_item_to_match_) of
  // current_paint_artifact_.GetDisplayItemList() to their initial values. This
  // should be called when the DisplayItemList in current_paint_artifact_ is
  // newly created, or is changed causing the previous indices to be invalid.
  void ResetCurrentListIndices();

  // The following two methods are for checking under-invalidations
  // (when RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled).
  void ShowUnderInvalidationError(const char* reason,
                                  const DisplayItem& new_item,
                                  const DisplayItem* old_item) const;

  void ShowSequenceUnderInvalidationError(const char* reason,
                                          const DisplayItemClient&,
                                          int start,
                                          int end);

  void CheckUnderInvalidation();
  bool IsCheckingUnderInvalidation() const {
    return under_invalidation_checking_end_ >
           under_invalidation_checking_begin_;
  }

  struct SubsequenceMarkers {
    SubsequenceMarkers() : start(0), end(0) {}
    SubsequenceMarkers(size_t start_arg, size_t end_arg)
        : start(start_arg), end(end_arg) {}
    // The start and end (not included) index within current_paint_artifact_
    // of this subsequence.
    size_t start;
    size_t end;
  };

  SubsequenceMarkers* GetSubsequenceMarkers(const DisplayItemClient&);

#if DCHECK_IS_ON()
  void CheckDuplicatePaintChunkId(const PaintChunk::Id&);
  void ShowDebugDataInternal(DisplayItemList::JsonFlags) const;
#endif

  void UpdateUMACounts();

  Usage usage_;

  // The last paint artifact after CommitNewDisplayItems().
  // It includes paint chunks as well as display items.
  scoped_refptr<PaintArtifact> current_paint_artifact_;

  // Data being used to build the next paint artifact.
  DisplayItemList new_display_item_list_;
  PaintChunker new_paint_chunks_;

  bool construction_disabled_ = false;
  bool subsequence_caching_disabled_ = false;

  bool cache_is_all_invalid_ = true;
  bool committed_ = false;

  // A stack recording current frames' first paints.
  Vector<FrameFirstPaint> frame_first_paints_;

  unsigned skipping_cache_count_ = 0;

  size_t num_cached_new_items_ = 0;
  size_t num_cached_new_subsequences_ = 0;

  // Stores indices to valid cacheable display items in
  // current_paint_artifact_.GetDisplayItemList() that have not been matched by
  // requests of cached display items (using UseCachedItemIfPossible() and
  // UseCachedSubsequenceIfPossible()) during sequential matching. The indexed
  // items will be matched by later out-of-order requests of cached display
  // items. This ensures that when out-of-order cached display items are
  // requested, we only traverse at most once over the current display list
  // looking for potential matches. Thus we can ensure that the algorithm runs
  // in linear time.
  IndicesByClientMap out_of_order_item_indices_;

  // The next item in the current list for sequential match.
  size_t next_item_to_match_ = 0;

  // The next item in the current list to be indexed for out-of-order cache
  // requests.
  size_t next_item_to_index_ = 0;

#if DCHECK_IS_ON()
  size_t num_indexed_items_ = 0;
  size_t num_sequential_matches_ = 0;
  size_t num_out_of_order_matches_ = 0;

  // This is used to check duplicated ids during CreateAndAppend().
  IndicesByClientMap new_display_item_indices_by_client_;
  // This is used to check duplicated ids for new paint chunks.
  IndicesByClientMap new_paint_chunk_indices_by_client_;
#endif

  // These are set in UseCachedItemIfPossible() and
  // UseCachedSubsequenceIfPossible() when we could use cached drawing or
  // subsequence and under-invalidation checking is on, indicating the begin and
  // end of the cached drawing or subsequence in the current list. The functions
  // return false to let the client do actual painting, and PaintController will
  // check if the actual painting results are the same as the cached.
  size_t under_invalidation_checking_begin_ = 0;
  size_t under_invalidation_checking_end_ = 0;

  String under_invalidation_message_prefix_;

  using CachedSubsequenceMap =
      HashMap<const DisplayItemClient*, SubsequenceMarkers>;
  CachedSubsequenceMap current_cached_subsequences_;
  CachedSubsequenceMap new_cached_subsequences_;
  size_t last_cached_subsequence_end_ = 0;

  unsigned current_fragment_ = 0;

  // Accumulated counts for UMA metrics. Updated by UpdateUMACounts() and
  // UpdateUMACountsOnFullyCached(), and reported as UMA metrics and reset by
  // ReportUMACounts(). The accumulation is mainly for pre-CompositeAfterPaint
  // to sum up the data from multiple PaintControllers during a paint in
  // document life cycle update.
  static size_t sum_num_items_;
  static size_t sum_num_cached_items_;
  static size_t sum_num_subsequences_;
  static size_t sum_num_cached_subsequences_;

  class DisplayItemListAsJSON;

  DISALLOW_COPY_AND_ASSIGN(PaintController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_
