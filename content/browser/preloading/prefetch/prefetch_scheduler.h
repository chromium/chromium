// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SCHEDULER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SCHEDULER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class PrefetchContainer;
class PrefetchService;

// Priority of `PrefetchContainer`
//
// - Larger values take priority.
// - If `PrefetchContainer` has a priority larger than `kBurstThreshold`, it
//   will be executed with additional capacity. For more details, see the
//   comment #algorithm in `PrefetchScheduler::Progress()`.
// - Note that priority should be explicitly re-calculated if related attributes
//   of `PrefetchContainer` are changed. See
//   `PrefetchScheduler::CalculatePriority()`, the call sites, and
//   `PrefetchScheduler::NotifyAttributeMightChangedAndProgressAsync()`.
//
// Maintenance policy:
//
// - Put a priority per reason. Think twice before mixing multiple reasons into
//   a priority. (For ease of understanding the behavior.)
// - It's safe to renumber with preserving the order. Do it if needed.
//
// TODO(crbug.com/406403063): Rethink about granularity. We don't stick on the
// above policy. More rough priorities e.g. kBase/kHigh/kBurst might work well.
// This will eventually be done by `PrefetchPriority`(crbug.com/426404355).
//
// See also `PrefetchScheduler::NotifyAttributeMightChangedAndProgressAsync()`
// when you add a new one.
enum class PrefetchSchedulerPriority {
  // Default.
  kBase = 0,
  // For tests. Do not use outside tests.
  kHighTest = 1,
  // Priority for prefetch ahead of prerender.
  kHighAheadOfPrerender = 2,
  // It's a threshold for burst. Do not use it in
  // `PrefetchQueue::CalculatePriority()`. For burst, see a comment of
  // `PrefetchScheduler`.
  kBurstThreshold = 10,
  // For tests. Do not use outside tests.
  kBurstTest = 11,
  // Priority derived from `PrefetchPriority`.
  kBurstForPrefetchPriority = 12,
  // Burst priority for prefetch ahead of prerender.
  kBurstAheadOfPrerender = 13,
};

// Priority queue for prefetches
//
// It's a pure data structure and intended to be used in only
// `PrefetchScheduler`.
class PrefetchQueue {
 public:
  struct Item {
    Item(base::WeakPtr<PrefetchContainer> prefetch_container,
         PrefetchSchedulerPriority priority);
    ~Item();

    // Movable but not copyable.
    Item(const Item&&);
    Item& operator=(const Item&&);
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    base::WeakPtr<PrefetchContainer> prefetch_container;
    PrefetchSchedulerPriority priority;
  };

  PrefetchQueue();
  ~PrefetchQueue();

  // Not movable nor copyable.
  PrefetchQueue(const PrefetchQueue&&) = delete;
  PrefetchQueue& operator=(const PrefetchQueue&&) = delete;
  PrefetchQueue(const PrefetchQueue&) = delete;
  PrefetchQueue& operator=(const PrefetchQueue&) = delete;

  size_t size() const { return queue_.size(); }

  void Push(base::WeakPtr<PrefetchContainer> prefetch_container,
            PrefetchSchedulerPriority priority);

  // Pops `PrefetchContainer`
  //
  // Returns the most high priority and first-in one iff found satisfying:
  //
  // - `pred`
  // - Priority is larger or equal to `threshold_priority`.
  template <class Predicate>
  std::optional<PrefetchQueue::Item> Pop(
      Predicate pred,
      PrefetchSchedulerPriority threshold_priority) {
    for (auto it = queue_.cbegin(); it != queue_.cend(); ++it) {
      if (it->priority < threshold_priority) {
        break;
      }

      if (pred(*it)) {
        PrefetchQueue::Item ret = std::move(*it);
        queue_.erase(it);
        return std::move(ret);
      }
    }

    return std::nullopt;
  }

  // Removes `PrefechContainer`.
  //
  // Returns true iff `PrefetchContainer` exists and is removed.
  bool Remove(base::WeakPtr<PrefetchContainer> prefetch_container);
  // Updates priority.
  //
  // Returns true iff priority is updated.
  bool MaybeUpdatePriority(PrefetchContainer& prefetch_container,
                           PrefetchSchedulerPriority priority);

  std::optional<int> GetIndexForMetrics(
      const PrefetchContainer& prefetch_container) const;

 private:
  std::vector<PrefetchQueue::Item> queue_;
};

// Schedules prefetches with limited concurrency.
//
// - Limits concurrently running prefetches.
// - Bursts and use a different limit if a `PrefetchContainer` has priority
//   higher than `PrefetchPriority::kBurstThreshold`.
// - Manages waiting prefetches with a priority queue.
// - Prevents stuck (`queue_.size() > 0 && active_set_.size() == 0`).
//
// For more details, see
// https://docs.google.com/document/d/1W0Nk3Nq6NaUXkBppOUC5zyNmhVqMjYShm1bydGYd9qc
class CONTENT_EXPORT PrefetchScheduler {
 public:
  explicit PrefetchScheduler(PrefetchService* prefetch_service);
  ~PrefetchScheduler();

  // Not movable nor copyable.
  PrefetchScheduler(const PrefetchScheduler&&) = delete;
  PrefetchScheduler& operator=(const PrefetchScheduler&&) = delete;
  PrefetchScheduler(const PrefetchScheduler&) = delete;
  PrefetchScheduler& operator=(const PrefetchScheduler&) = delete;

  // See `active_set_`.
  bool IsInActiveSet(const PrefetchContainer& prefetch_container);

  // Modify-and-progress-async methods
  //
  // To prevent stuck, this class (and `PrefetchService`) must ensure that
  // `Progress()` will be called eventually after modification. To ensure that,
  // these modification methods automatically emit `Progress()` in async
  // fashion.
  //
  // Contract:
  //
  // - `PrefetchService` must call `RemoveAndProgressAsync()` before weak
  //   pointer invalidation of the `PrefetchContainer`.
  // - `PrefetchService` must call
  //   `NotifyAttributeMightChangedAndProgressAsync()` after modification of
  //   `PrefetchContainer` that can change priority.
  //
  // [#cant-detect-prefetch-document-manager] Note that `PrefetchScheduler`
  // can't detect changes of `PrefetchDocumentManager` and
  // `PrefetchDocumentManager::CanPrefetchNow()`. So, `PrefetchService` must
  // explicitly call `Progress()`.
  //
  // If `should_progress` is false, doesn't call `ProgressAsync()`.
  void PushAndProgress(PrefetchContainer& prefetch_container);
  void PushAndProgressAsync(PrefetchContainer& prefetch_container);
  // Note that this doesn't call `PrefetchService::ResetPrefetchContainer()`.
  void RemoveAndProgressAsync(PrefetchContainer& prefetch_container,
                              bool should_progress = true);
  void NotifyAttributeMightChangedAndProgressAsync(
      PrefetchContainer& prefetch_container,
      bool should_proggress = true);

  // Progress scheduling
  //
  // This can call methods of `PrefetchService` with
  // `PassKey<PrefetchScheduler>`, e.g. starting the load and eviction.
  //
  // Contract: See the comment [#cant-detect-prefetch-document-manager].
  void Progress();

  void SetCalculatePriorityForTesting(
      base::RepeatingCallback<
          PrefetchSchedulerPriority(const PrefetchContainer&)> callback);

  int GetQueueSizeForMetrics() const { return queue_.size(); }
  std::optional<int> GetIndexForMetrics(
      const PrefetchContainer& prefetch_container) const {
    return queue_.GetIndexForMetrics(prefetch_container);
  }

 private:
  PrefetchSchedulerPriority CalculatePriority(
      const PrefetchContainer& prefetch_container);

  void ProgressAsync();
  void ProgressOne(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Safety: This class is owned by `PrefetchService` and has the same lifetime.
  const raw_ptr<PrefetchService> prefetch_service_;

  // Max heap by priority
  PrefetchQueue queue_;
  // Active set of prefetches, i.e. prefetches that are in loading state
  // (roughly).
  //
  // We use `std::vector` as set-like containers can't use `base::WeakPtr`.
  std::vector<base::WeakPtr<PrefetchContainer>> active_set_;

  // Used to reduce unnecessary multiple `Progress()` calls.
  bool is_progress_scheduled_ = false;
  // Used to prevent `PrefetchScheduler::ProgressOne()` ->
  // `PrefetchService::EvictPrefetch()` ->
  // `PrefetchScheduler::RemoveAndProgressAsync()` trigger
  // `PrefetchScheduler::ProgressAsync()`.
  bool in_eviction_ = false;

  base::RepeatingCallback<PrefetchSchedulerPriority(const PrefetchContainer&)>
      calculate_priority_for_test_;

#if DCHECK_IS_ON()
  // Protects against `Progress()` being called recursively.
  bool progress_reentrancy_guard_ = false;
#endif

  base::WeakPtrFactory<PrefetchScheduler> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SCHEDULER_H_
