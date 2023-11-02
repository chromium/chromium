// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_scheduler.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {

using DelayedProcessingInfoMap =
    std::map<StoragePartitionImpl*, std::unique_ptr<base::OneShotTimer>>;

// static
BackgroundSyncScheduler* BackgroundSyncScheduler::GetFor(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  return BrowserContextImpl::From(browser_context)->background_sync_scheduler();
}

BackgroundSyncScheduler::BackgroundSyncScheduler() = default;

BackgroundSyncScheduler::~BackgroundSyncScheduler() {
  for (auto& one_shot_processing_info : delayed_processing_info_one_shot_)
    one_shot_processing_info.second->Stop();

  for (auto& periodic_processing_info : delayed_processing_info_periodic_)
    periodic_processing_info.second->Stop();
}

void BackgroundSyncScheduler::ScheduleDelayedProcessing(
    StoragePartitionImpl* storage_partition,
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay,
    base::OnceClosure delayed_task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(storage_partition);

  // CancelDelayedProcessing should be called in this case.
  DCHECK(!delay.is_max());

  auto& delayed_processing_info = GetDelayedProcessingInfoMap(sync_type);
  delayed_processing_info.emplace(storage_partition,
                                  std::make_unique<base::OneShotTimer>());

  if (!delay.is_zero()) {
    delayed_processing_info[storage_partition]->Start(
        FROM_HERE, delay,
        base::BindOnce(&BackgroundSyncScheduler::RunDelayedTaskAndPruneInfoMap,
                       weak_ptr_factory_.GetWeakPtr(), sync_type,
                       storage_partition, std::move(delayed_task)));
  }

#if BUILDFLAG(IS_ANDROID)
  ScheduleOrCancelBrowserWakeupForSyncType(sync_type, storage_partition);
#endif
}

void BackgroundSyncScheduler::CancelDelayedProcessing(
    StoragePartitionImpl* storage_partition,
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(storage_partition);

  auto& delayed_processing_info = GetDelayedProcessingInfoMap(sync_type);
  if (delayed_processing_info.count(storage_partition)) {
    base::TimeTicks run_time =
        delayed_processing_info[storage_partition]->desired_run_time();

    // If this storage partition was scheduling the next wakeup, reset the
    // wakeup time for |sync_type|.
    if (scheduled_wakeup_time_[sync_type] == run_time)
      scheduled_wakeup_time_[sync_type] = base::TimeTicks::Max();

    delayed_processing_info.erase(storage_partition);
  }

#if BUILDFLAG(IS_ANDROID)
  ScheduleOrCancelBrowserWakeupForSyncType(sync_type, storage_partition);
#endif
}

DelayedProcessingInfoMap& BackgroundSyncScheduler::GetDelayedProcessingInfoMap(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
    return delayed_processing_info_one_shot_;
  else
    return delayed_processing_info_periodic_;
}

void BackgroundSyncScheduler::RunDelayedTaskAndPruneInfoMap(
    blink::mojom::BackgroundSyncType sync_type,
    StoragePartitionImpl* storage_partition,
    base::OnceClosure delayed_task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(delayed_task).Run();
  CancelDelayedProcessing(storage_partition, sync_type);
}

#if BUILDFLAG(IS_ANDROID)
void BackgroundSyncScheduler::ScheduleOrCancelBrowserWakeupForSyncType(
    blink::mojom::BackgroundSyncType sync_type,
    StoragePartitionImpl* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* browser_context = storage_partition->browser_context();
  DCHECK(browser_context);
  auto* controller = browser_context->GetBackgroundSyncController();
  DCHECK(controller);

  auto& delayed_processing_info = GetDelayedProcessingInfoMap(sync_type);

  // If no more scheduled tasks remain, cancel browser wakeup.
  // Canceling when there's no task scheduled is a no-op.
  if (delayed_processing_info.empty()) {
    scheduled_wakeup_time_[sync_type] = base::TimeTicks::Max();
    controller->CancelBrowserWakeup(sync_type);
    return;
  }

  // Schedule browser wakeup with the smallest delay required.
  auto& min_info = *std::min_element(
      delayed_processing_info.begin(), delayed_processing_info.end(),
      [](auto& lhs, auto& rhs) {
        return (lhs.second->desired_run_time() - base::TimeTicks::Now()) <
               (rhs.second->desired_run_time() - base::TimeTicks::Now());
      });

  base::TimeTicks next_time = min_info.second->desired_run_time();
  if (next_time >= scheduled_wakeup_time_[sync_type]) {
    // There's an earlier wakeup time scheduled, no need to inform the
    // scheduler.
    return;
  }

  scheduled_wakeup_time_[sync_type] = next_time;
  controller->ScheduleBrowserWakeUpWithDelay(
      sync_type, min_info.second->desired_run_time() - base::TimeTicks::Now());
}
#endif

}  // namespace content
