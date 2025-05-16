// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_scheduler.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prerender/prerender_features.h"

namespace content {

namespace {

size_t GetActiveSetSizeLimitForBase() {
  // TODO(crbug.com/406403063): Update the limit for base.

  if (base::FeatureList::IsEnabled(features::kPrefetchSchedulerTesting)) {
    return features::kPrefetchSchedulerTestingActiveSetSizeLimitForBase.Get();
  }

  return 1;
}

size_t GetActiveSetSizeLimitForBurst() {
  if (base::FeatureList::IsEnabled(features::kPrefetchSchedulerTesting)) {
    return features::kPrefetchSchedulerTestingActiveSetSizeLimitForBurst.Get();
  }

  // Before prefetch/prerender integration (i.e.
  // `Prerender2FallbackPrefetchSpecRules` is disabled), prerender ran without
  // prefetch So, it was not blocked by prefetch queue. Allow
  // prefetch-ahead-of-prerender to run independently of the ordinal prefetch
  // queue so that prerendering is not blocked by queued prefetch requests.
  //
  // Note that prerenders are run sequentially. So, +1 is enough.
  if (features::kPrerender2FallbackPrefetchSchedulerPolicy.Get() ==
      features::Prerender2FallbackPrefetchSchedulerPolicy::kBurst) {
    return GetActiveSetSizeLimitForBase() + 1;
  }

  // No additional room for burst.
  return GetActiveSetSizeLimitForBase();
}

PrefetchPriority CalculatePriorityImpl(
    const PrefetchContainer& prefetch_container) {
  // Burst/prioritize if ahead of prerender.
  if (prefetch_container.IsLikelyAheadOfPrerender()) {
    switch (features::kPrerender2FallbackPrefetchSchedulerPolicy.Get()) {
      case features::Prerender2FallbackPrefetchSchedulerPolicy::kNotUse:
        break;
      case features::Prerender2FallbackPrefetchSchedulerPolicy::kPrioritize:
        return PrefetchPriority::kHighAheadOfPrerender;
      case features::Prerender2FallbackPrefetchSchedulerPolicy::kBurst:
        return PrefetchPriority::kBurstAheadOfPrerender;
    }
  }

  return PrefetchPriority::kBase;
}

bool IsReadyToStartPrefetch(const PrefetchQueue::Item& item) {
  // Keep this method as similar as much as possible to a lambda in
  // `PrefetchService::PopNextPrefetchContainer()`.
  //
  // TODO(crbug.com/406754449): Remove this comment.

  // `prefetch_container` must be valid. It will be ensured by `PrefetchService`
  // in the future.
  //
  // Return true and let it handle `PrefetchScheduler::Progress()`.
  //
  // TODO(crbug.com/400761083): Use `CHECK`.
  if (!item.prefetch_container) {
    return true;
  }

  if (!item.prefetch_container->IsRendererInitiated()) {
    // TODO(crbug.com/40946257): Revisit the resource limits and
    // conditions for starting browser-initiated prefetch.
    return true;
  }

  auto* prefetch_document_manager =
      item.prefetch_container->GetPrefetchDocumentManager();
  // If there is no manager in renderer-initiated prefetch (can happen
  // only in tests), just bypass the check.
  if (!prefetch_document_manager) {
    CHECK_IS_TEST();
    return true;
  }

  // Eviction wil be handled in `PrefetchScheduler::ProgressOne()`.
  return std::get<0>(
      prefetch_document_manager->CanPrefetchNow(item.prefetch_container.get()));
}

}  // namespace

PrefetchQueue::Item::Item(base::WeakPtr<PrefetchContainer> prefetch_container,
                          PrefetchPriority priority)
    : prefetch_container(std::move(prefetch_container)), priority(priority) {}

PrefetchQueue::Item::Item(const PrefetchQueue::Item&& other)
    : prefetch_container(std::move(other.prefetch_container)),
      priority(other.priority) {}

PrefetchQueue::Item& PrefetchQueue::Item::operator=(
    const PrefetchQueue::Item&& other) {
  prefetch_container = std::move(other.prefetch_container);
  priority = other.priority;

  return *this;
}

PrefetchQueue::Item::~Item() = default;

PrefetchQueue::PrefetchQueue() = default;

PrefetchQueue::~PrefetchQueue() = default;

void PrefetchQueue::Push(base::WeakPtr<PrefetchContainer> prefetch_container,
                         PrefetchPriority priority) {
  CHECK(prefetch_container);
  // Postcondition: Pushing registered one is not allowed.
  CHECK(!Remove(prefetch_container));

  auto mid = std::partition_point(queue_.begin(), queue_.end(),
                                  [priority](PrefetchQueue::Item& item) {
                                    return item.priority >= priority;
                                  });
  queue_.insert(mid,
                PrefetchQueue::Item(std::move(prefetch_container), priority));
}

bool PrefetchQueue::Remove(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  for (auto it = queue_.cbegin(); it != queue_.cend(); ++it) {
    if (it->prefetch_container.get() == prefetch_container.get()) {
      queue_.erase(it);
      return true;
    }
  }

  return false;
}

bool PrefetchQueue::MaybeUpdatePriority(PrefetchContainer& prefetch_container,
                                        PrefetchPriority priority) {
  for (auto it = queue_.cbegin(); it != queue_.cend(); ++it) {
    if (it->prefetch_container.get() == &prefetch_container) {
      if (it->priority != priority) {
        queue_.erase(it);
        Push(prefetch_container.GetWeakPtr(), priority);
        return true;
      } else {
        return false;
      }
    }
  }

  return false;
}

PrefetchScheduler::PrefetchScheduler(PrefetchService* prefetch_service)
    : prefetch_service_(prefetch_service) {}

PrefetchScheduler::~PrefetchScheduler() = default;

bool PrefetchScheduler::IsInActiveSet(
    const PrefetchContainer& prefetch_container) {
  for (auto& active_prefetch_container : active_set_) {
    if (&prefetch_container == active_prefetch_container.get()) {
      return true;
    }
  }

  return false;
}

PrefetchPriority PrefetchScheduler::CalculatePriority(
    const PrefetchContainer& prefetch_container) {
  if (calculate_priority_for_test_) {
    return calculate_priority_for_test_.Run(prefetch_container);
  }

  return CalculatePriorityImpl(prefetch_container);
}

void PrefetchScheduler::PushAndProgress(PrefetchContainer& prefetch_container) {
  // Precondition: Pushing already registered one is not allowed.
  for (auto& it : active_set_) {
    if (it.get() == &prefetch_container) {
      NOTREACHED();
    }
  }

  PrefetchPriority priority = CalculatePriority(prefetch_container);
  queue_.Push(prefetch_container.GetWeakPtr(), priority);

  Progress();
}

void PrefetchScheduler::PushAndProgressAsync(
    PrefetchContainer& prefetch_container) {
  // Precondition: Pushing already registered one is not allowed.
  for (auto& it : active_set_) {
    if (it.get() == &prefetch_container) {
      NOTREACHED();
    }
  }

  PrefetchPriority priority = CalculatePriority(prefetch_container);
  queue_.Push(prefetch_container.GetWeakPtr(), priority);

  ProgressAsync();
}

void PrefetchScheduler::RemoveAndProgressAsync(
    PrefetchContainer& prefetch_container,
    bool should_progress) {
  [&]() {
    for (auto it = active_set_.cbegin(); it != active_set_.cend(); ++it) {
      if (it->get() == &prefetch_container) {
        active_set_.erase(it);
        return;
      }
    }

    queue_.Remove(prefetch_container.GetWeakPtr());
  }();

  if (!should_progress) {
    return;
  }

  // This method can be called in `PrefetechService::EvictPrefetch()` called in
  // `ProcessOne()`. Don't call `ProcessAsync()` to prevent infinite loop in
  // that case.
  if (!in_eviction_) {
    ProgressAsync();
  }
}

void PrefetchScheduler::NotifyAttributeMightChangedAndProgressAsync(
    PrefetchContainer& prefetch_container,
    bool should_progress) {
  if (!should_progress) {
    return;
  }

  const bool is_changed = queue_.MaybeUpdatePriority(
      prefetch_container, CalculatePriority(prefetch_container));
  if (is_changed) {
    ProgressAsync();
  }
}

void PrefetchScheduler::ProgressAsync() {
  if (is_progress_scheduled_) {
    return;
  }
  is_progress_scheduled_ = true;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrefetchScheduler::Progress,
                                weak_method_factory_.GetWeakPtr()));
}

void PrefetchScheduler::Progress() {
#if DCHECK_IS_ON()
  // Asserts that reentrancy doesn't happen.
  CHECK(!progress_reentrancy_guard_);
  base::AutoReset guard(&progress_reentrancy_guard_, true);
#endif

  // Note that this doesn't correspond to the update in `ProgressAsync()` in 1:1
  // and there is a case updating `false` to `false` as this method can be
  // called from `PrefetchService` directly.
  is_progress_scheduled_ = false;

  // #algorithm
  //
  // 1. Start prefetches with burst priority with limit for burst.
  //
  //    If the size of active set is < `GetActiveSetSizeLimitForBurst()`, pop
  //    `PrefetchContainer` with priority >= `kBurstThreshold` that can be
  //    started and start it. Continue it until active set size reaches the
  //    limit or queue becomes empty.
  //
  // 2. Start prefetches with limit.
  //
  //    If the size of active set is < `GetActiveSetSizeLimitForBase()`, pop
  //    `PrefetchContainer` (with no priority condition) that can be
  //    started and start it. Continue it until active set size reaches the
  //    limit or queue becomes empty.
  //
  // TODO(crbug.com/406403063): Consider not to limit prefetches with burst
  // priority. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/6402914/comment/8b5c845f_0b7f6f7e/

  auto internal = [&](PrefetchPriority threshold_priority,
                      size_t active_limit) {
    // Invariant: `active_set_.size() == 0 && there is a ready prefetch` is
    // false. I.e. doesn't stuck.
    while (active_set_.size() < active_limit) {
      std::optional<PrefetchQueue::Item> item =
          queue_.Pop(IsReadyToStartPrefetch, threshold_priority);
      if (!item.has_value()) {
        break;
      }

      base::WeakPtr<PrefetchContainer> prefetch_container =
          item.value().prefetch_container;
      // `prefetch_container` must be valid. It will be ensured by
      // `PrefetchService` in the future.
      //
      // TODO(crbug.com/400761083): Use `CHECK`.
      if (!prefetch_container) {
        continue;
      }

      // This call calls a method of `PrefetchService` and can incur methods of
      // `PrefetchScheduler`. It is safe as we don't hold iterators at this
      // timing.
      ProgressOne(std::move(prefetch_container));
    }
  };

  internal(PrefetchPriority::kBurstThreshold, GetActiveSetSizeLimitForBurst());
  internal(PrefetchPriority::kBase, GetActiveSetSizeLimitForBase());
}

void PrefetchScheduler::ProgressOne(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(prefetch_container);

  // Evict if needed.
  [&]() {
    auto* prefetch_document_manager =
        prefetch_container->GetPrefetchDocumentManager();
    if (!prefetch_document_manager) {
      return;
    }

    base::WeakPtr<PrefetchContainer> prefetch_to_evict = std::get<1>(
        prefetch_document_manager->CanPrefetchNow(prefetch_container.get()));
    if (!prefetch_to_evict) {
      return;
    }

    {
      base::AutoReset<bool> guard{&in_eviction_, true};
      prefetch_service_->EvictPrefetch(base::PassKey<PrefetchScheduler>(),
                                       prefetch_to_evict);
    }
  }();

  const bool is_started = prefetch_service_->StartSinglePrefetch(
      base::PassKey<PrefetchScheduler>(), prefetch_container);
  if (is_started) {
    active_set_.push_back(prefetch_container);
  }
}

void PrefetchScheduler::SetCalculatePriorityForTesting(
    base::RepeatingCallback<PrefetchPriority(const PrefetchContainer&)>
        callback) {
  calculate_priority_for_test_ = std::move(callback);
}

}  // namespace content
