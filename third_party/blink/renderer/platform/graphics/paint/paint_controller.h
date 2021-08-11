// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "cc/input/layer_selection_bound.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PaintUnderInvalidationChecker;

enum class PaintBenchmarkMode {
  kNormal,
  kForceRasterInvalidationAndConvert,
  kForcePaintArtifactCompositorUpdate,
  kForcePaint,
  // The above modes don't additionally invalidate paintings, i.e. during
  // repeated benchmarking, the PaintController is fully cached.
  kPartialInvalidation,
  kSmallInvalidation,
  kSubsequenceCachingDisabled,
  kCachingDisabled,
};

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
  PaintController(const PaintController&) = delete;
  PaintController& operator=(const PaintController&) = delete;
  ~PaintController();

#if DCHECK_IS_ON()
  Usage GetUsage() const { return usage_; }
#endif

  class PLATFORM_EXPORT CycleScope {
    STACK_ALLOCATED();

   public:
    CycleScope() = default;
    explicit CycleScope(PaintController& controller) {
      AddController(controller);
    }
    void AddController(PaintController& controller) {
      controller.StartCycle(clients_to_validate_);
      controllers_.push_back(&controller);
    }
    ~CycleScope();

   protected:
    Vector<PaintController*> controllers_;

   private:
    Vector<const DisplayItemClient*> clients_to_validate_;
  };
  friend class CycleScope;

  // These methods are called during painting.

  // Provide a new set of paint chunk properties to apply to recorded display
  // items. If id is nullptr, the id of the first display item will be used as
  // the id of the paint chunk if needed.
  void UpdateCurrentPaintChunkProperties(const PaintChunk::Id*,
                                         const PropertyTreeStateOrAlias&);
  const PropertyTreeStateOrAlias& CurrentPaintChunkProperties() const {
    return paint_chunker_.CurrentPaintChunkProperties();
  }
  // See PaintChunker for documentation of the following methods.
  void SetWillForceNewChunk(bool force) {
    paint_chunker_.SetWillForceNewChunk(force);
  }
  bool WillForceNewChunk() const { return paint_chunker_.WillForceNewChunk(); }
  void SetCurrentEffectivelyInvisible(bool invisible) {
    paint_chunker_.SetCurrentEffectivelyInvisible(invisible);
  }
  bool CurrentEffectivelyInvisible() const {
    return paint_chunker_.CurrentEffectivelyInvisible();
  }
  void EnsureChunk();

  void SetShouldComputeContentsOpaque(bool should_compute) {
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    paint_chunker_.SetShouldComputeContentsOpaque(should_compute);
  }

  void RecordHitTestData(const DisplayItemClient&,
                         const IntRect&,
                         TouchAction,
                         bool);

  void RecordScrollHitTestData(
      const DisplayItemClient&,
      DisplayItem::Type,
      const TransformPaintPropertyNode* scroll_translation,
      const IntRect&);

  void RecordSelection(absl::optional<PaintedSelectionBound> start,
                       absl::optional<PaintedSelectionBound> end);

  void SetPossibleBackgroundColor(const DisplayItemClient&,
                                  Color,
                                  uint64_t area);

  wtf_size_t NumNewChunks() const {
    return new_paint_artifact_->PaintChunks().size();
  }
  const IntRect& LastChunkBounds() const {
    return new_paint_artifact_->PaintChunks().back().bounds;
  }

  void MarkClientForValidation(const DisplayItemClient& client);

  template <typename DisplayItemClass, typename... Args>
  void CreateAndAppend(const DisplayItemClient& client, Args&&... args) {
    MarkClientForValidation(client);
    DisplayItemClass& display_item =
        new_paint_artifact_->GetDisplayItemList()
            .AllocateAndConstruct<DisplayItemClass>(
                client, std::forward<Args>(args)...);
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

  // Returns the index of the new subsequence.
  wtf_size_t BeginSubsequence(const DisplayItemClient&);
  // The |subsequence_index| parameter should be the return value of the
  // corresponding BeginSubsequence().
  void EndSubsequence(wtf_size_t subsequence_index);

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

  // Returns the approximate memory usage owned by this PaintController.
  size_t ApproximateUnsharedMemoryUsage() const;

  // Get the artifact generated after the last commit.
  const PaintArtifact& GetPaintArtifact() const {
    CheckNoNewPaint();
    return *current_paint_artifact_;
  }
  scoped_refptr<const PaintArtifact> GetPaintArtifactShared() const {
    CheckNoNewPaint();
    return current_paint_artifact_;
  }
  const DisplayItemList& GetDisplayItemList() const {
    return GetPaintArtifact().GetDisplayItemList();
  }
  const Vector<PaintChunk>& PaintChunks() const {
    return GetPaintArtifact().PaintChunks();
  }

  scoped_refptr<const PaintArtifact> GetNewPaintArtifactShared() const {
    DCHECK(new_paint_artifact_);
    return new_paint_artifact_;
  }
  wtf_size_t NewPaintChunkCount() const {
    DCHECK(new_paint_artifact_);
    return new_paint_artifact_->PaintChunks().size();
  }

  class ScopedBenchmarkMode {
    STACK_ALLOCATED();

   public:
    ScopedBenchmarkMode(PaintController& paint_controller,
                        PaintBenchmarkMode mode)
        : paint_controller_(paint_controller) {
      // Nesting is not allowed.
      DCHECK_EQ(PaintBenchmarkMode::kNormal, paint_controller_.benchmark_mode_);
      paint_controller.SetBenchmarkMode(mode);
    }
    ~ScopedBenchmarkMode() {
      paint_controller_.SetBenchmarkMode(PaintBenchmarkMode::kNormal);
    }

   private:
    PaintController& paint_controller_;
  };

  PaintBenchmarkMode GetBenchmarkMode() const { return benchmark_mode_; }
  bool ShouldForcePaintForBenchmark() {
    return benchmark_mode_ >= PaintBenchmarkMode::kForcePaint;
  }

  void SetFirstPainted();
  void SetTextPainted();
  void SetImagePainted();

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
  wtf_size_t CurrentFragment() const { return current_fragment_; }
  void SetCurrentFragment(wtf_size_t fragment) { current_fragment_ = fragment; }

  class CounterForTesting {
    STACK_ALLOCATED();
   public:
    CounterForTesting() {
      DCHECK(!PaintController::counter_for_testing_);
      PaintController::counter_for_testing_ = this;
    }
    ~CounterForTesting() {
      DCHECK_EQ(this, PaintController::counter_for_testing_);
      PaintController::counter_for_testing_ = nullptr;
    }
    void Reset() { num_cached_items = num_cached_subsequences = 0; }

    size_t num_cached_items = 0;
    size_t num_cached_subsequences = 0;
  };

 private:
  friend class PaintControllerTestBase;
  friend class PaintControllerPaintTestBase;
  friend class PaintUnderInvalidationChecker;
  friend class GraphicsLayer;  // Temporary for ClientCacheIsValid().

  // Called before painting to optimize memory allocation by reserving space in
  // |new_paint_artifact_| and |new_subsequences_| based on the size of the
  // previous ones (|current_paint_artifact_| and |current_subsequences_|).
  void ReserveCapacity();

  // Called at the beginning of a paint cycle, as defined by CycleScope.
  void StartCycle(Vector<const DisplayItemClient*>& clients_to_validate);

  // Called at the end of a paint cycle, as defined by CycleScope.
  // The PaintController will cleanup data that will no longer be used for the
  // next cycle, and update status to be ready for the next cycle.
  // It updates caching status of DisplayItemClients, so if there are
  // DisplayItemClients painting on multiple PaintControllers, we should call
  // there FinishCycle() at the same time to ensure consistent caching status.
  void FinishCycle();

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

  void InvalidateAllForTesting();

  // Set new item state (cache skipping, etc) for the last new display item.
  void ProcessNewItem(DisplayItem&);

  void CheckNewItem(DisplayItem&);
  void CheckNewChunk();

  // Maps a display item id to the index of the display item or the paint chunk.
  using IdIndexMap = HashMap<DisplayItem::Id::HashKey, wtf_size_t>;

  static wtf_size_t FindItemFromIdIndexMap(const DisplayItem::Id&,
                                           const IdIndexMap&,
                                           const DisplayItemList&);
  static void AddToIdIndexMap(const DisplayItem::Id&,
                              wtf_size_t index,
                              IdIndexMap&);

  wtf_size_t FindCachedItem(const DisplayItem::Id&);
  wtf_size_t FindOutOfOrderCachedItemForward(const DisplayItem::Id&);
  void AppendSubsequenceByMoving(const DisplayItemClient&,
                                 wtf_size_t subsequence_index,
                                 wtf_size_t start_chunk_index,
                                 wtf_size_t end_chunk_index);

  struct SubsequenceMarkers {
    const DisplayItemClient* client = nullptr;
    // The start and end (not included) index of paint chunks in this
    // subsequence.
    wtf_size_t start_chunk_index = 0;
    wtf_size_t end_chunk_index = 0;
    bool is_moved_from_cached_subsequence = false;
  };

  wtf_size_t GetSubsequenceIndex(const DisplayItemClient&) const;
  const SubsequenceMarkers* GetSubsequenceMarkers(
      const DisplayItemClient&) const;

  void ValidateNewChunkId(const PaintChunk::Id&);

  PaintUnderInvalidationChecker& EnsureUnderInvalidationChecker();
  ALWAYS_INLINE bool IsCheckingUnderInvalidation() const;

#if DCHECK_IS_ON()
  void ShowDebugDataInternal(DisplayItemList::JsonFlags) const;
#endif

  void SetBenchmarkMode(PaintBenchmarkMode);
  bool ShouldInvalidateDisplayItemForBenchmark();
  bool ShouldInvalidateSubsequenceForBenchmark();

  void CheckNoNewPaint() const {
#if DCHECK_IS_ON()
    DCHECK(!new_paint_artifact_ || new_paint_artifact_->IsEmpty());
    DCHECK(paint_chunker_.IsInInitialState());
    DCHECK(current_paint_artifact_);
#endif
  }

  Usage usage_;

  // The last paint artifact after CommitNewDisplayItems().
  // It includes paint chunks as well as display items.
  // It's initially empty and is never null if usage is kMultiplePaints.
  // Otherwise it's null before CommitNewDisplayItems().
  scoped_refptr<PaintArtifact> current_paint_artifact_;

  // Data being used to build the next paint artifact.
  // It's never null and if usage is kMultiplePaints. Otherwise it's null after
  // CommitNewDisplayItems().
  scoped_refptr<PaintArtifact> new_paint_artifact_;
  PaintChunker paint_chunker_;
  Vector<const DisplayItemClient*>* clients_to_validate_ = nullptr;

  bool cache_is_all_invalid_ = true;
  bool committed_ = false;

  // A stack recording current frames' first paints.
  Vector<FrameFirstPaint> frame_first_paints_;

  unsigned skipping_cache_count_ = 0;

  wtf_size_t num_cached_new_items_ = 0;
  wtf_size_t num_cached_new_subsequences_ = 0;

  // Maps from ids to indices of valid cacheable display items in
  // current_paint_artifact_.GetDisplayItemList() that have not been matched by
  // requests of cached display items (using UseCachedItemIfPossible() and
  // UseCachedSubsequenceIfPossible()) during sequential matching. The indexed
  // items will be matched by later out-of-order requests of cached display
  // items. This ensures that when out-of-order cached display items are
  // requested, we only traverse at most once over the current display list
  // looking for potential matches. Thus we can ensure that the algorithm runs
  // in linear time.
  IdIndexMap out_of_order_item_id_index_map_;

  // The next item in the current list for sequential match.
  wtf_size_t next_item_to_match_ = 0;

  // The next item in the current list to be indexed for out-of-order cache
  // requests.
  wtf_size_t next_item_to_index_ = 0;

#if DCHECK_IS_ON()
  wtf_size_t num_indexed_items_ = 0;
  wtf_size_t num_sequential_matches_ = 0;
  wtf_size_t num_out_of_order_matches_ = 0;

  // This is used to check duplicated ids during CreateAndAppend().
  IdIndexMap new_display_item_id_index_map_;
  // This is used to check duplicated ids for new paint chunks.
  IdIndexMap new_paint_chunk_id_index_map_;
#endif

  std::unique_ptr<PaintUnderInvalidationChecker> under_invalidation_checker_;

  struct SubsequencesData {
    // Map a client to the index into |tree|.
    HashMap<const DisplayItemClient*, wtf_size_t> map;
    // A pre-order list of the subsequence tree.
    Vector<SubsequenceMarkers> tree;
  };
  SubsequencesData current_subsequences_;
  SubsequencesData new_subsequences_;

  wtf_size_t current_fragment_ = 0;

  PaintBenchmarkMode benchmark_mode_ = PaintBenchmarkMode::kNormal;
  int partial_invalidation_display_item_count_ = 0;
  int partial_invalidation_subsequence_count_ = 0;

  static CounterForTesting* counter_for_testing_;

  class PaintArtifactAsJSON;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_H_
