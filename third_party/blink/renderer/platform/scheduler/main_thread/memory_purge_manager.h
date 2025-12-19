// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_

#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// MemoryPurgeManager is responsible for purging memory in the renderer process.
// It does so by simulating a critical memory pressure event, which triggers
// memory release in various components (e.g., V8, Blink caches). On Android, it
// also triggers compaction of the process' memory when all pages are frozen.
//
// The primary triggers for a memory purge are:
// 1. The renderer process being backgrounded.
// 2. A page being frozen (e.g., added to the back-forward cache).
//
// This helps to reduce the memory footprint of backgrounded renderers.
class PLATFORM_EXPORT MemoryPurgeManager {
 public:
  explicit MemoryPurgeManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  MemoryPurgeManager(const MemoryPurgeManager&) = delete;
  MemoryPurgeManager& operator=(const MemoryPurgeManager&) = delete;
  ~MemoryPurgeManager();

  // Called when a page is created or destroyed, to maintain the total count of
  // pages owned by a renderer. A page is assumed to be not frozen upon
  // creation.
  void OnPageCreated();
  void OnPageDestroyed(bool frozen);

  // Called when a page is frozen. This may schedule a purge if the renderer is
  // backgrounded and, if the `kMemoryPurgeOnFreezeLimit` feature is enabled, no
  // purge occurred while a page was frozen in the current background session.
  void OnPageFrozen(base::MemoryReductionTaskContext called_from =
                        base::MemoryReductionTaskContext::kDelayExpired);

  // Called when a page is resumed (unfrozen). This cancels a delayed purge
  // scheduled by OnPageFrozen(), unsuppresses memory pressure notifications and
  // cancels pending Android self-compaction. Note that this does not cancel a
  // delayed purge scheduled by OnRendererBackgrounded().
  void OnPageResumed();

  // Tracks the renderer's background/foreground state.
  void SetRendererBackgrounded(bool backgrounded);

  // Called when the renderer is backgrounded. May schedule a delayed purge.
  void OnRendererBackgrounded();

  // Called when the renderer is foregrounded. Cancels any delayed purge.
  void OnRendererForegrounded();

  void SetPurgeDisabledForTesting(bool disabled) {
    purge_disabled_for_testing_ = disabled;
  }

  // Purge on renderer backgrounding is disabled on Android. On mobile Android,
  // it's redundant with the purge that occurs on page freezing (unlike on
  // desktop, freezing is applied to most background pages on mobile Android). A
  // field trial confirmed that an additional purge is not necessary. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1335069#c3.
  //
  // TODO(thiabaud): Since freezing is disabled on desktop Android, maybe the
  // purge on backgrounding should be enabled?
  static constexpr bool kPurgeOnBackgroundingEnabled =
#if BUILDFLAG(IS_ANDROID)
      false
#else
      true
#endif
      ;

  // Default maximum time to purge after the renderer is backgrounded. Can be
  // modified by field trials. Exposed for testing.
  static constexpr base::TimeDelta kDefaultMaxTimeToPurgeAfterBackgrounded =
      base::Minutes(4);

  // A small delay used when purging after a page freeze to ensure it runs after
  // other non-delayed tasks.
  static constexpr base::TimeDelta kFreezePurgeDelay =
      base::TimeDelta(base::Seconds(1));

  // Records a metric indicating whether all pages are currently frozen.
  void RecordAreAllPagesFrozenMetric(std::string_view name);

 private:
  // Starts `purge_timer_` to trigger a delayed memory purge if the timer is not
  // already running.
  void RequestMemoryPurgeWithDelay(base::TimeDelta delay);

  // Simulates a critical memory pressure signal to purge memory. May also
  // suppress future memory pressure notifications and trigger self-compaction.
  void PerformMemoryPurge();

  // Returns true if a memory purge is allowed. A purge is allowed if there's at
  // least one page and either a background purge is pending or the renderer is
  // backgrounded.
  bool CanPurge() const;

  // Returns true if all pages are frozen, or if there are no pages.
  bool AreAllPagesFrozen() const;

  // Returns a randomized time delta for scheduling a purge after being
  // backgrounded.
  base::TimeDelta GetTimeToPurgeAfterBackgrounded() const;

  bool purge_disabled_for_testing_ = false;
  bool renderer_backgrounded_ = kLaunchingProcessIsBackgrounded;

  // True if a memory purge was scheduled due to the renderer being
  // backgrounded and is currently pending.
  bool backgrounded_purge_pending_ = false;

  int total_page_count_ = 0;
  int frozen_page_count_ = 0;

  // True if a memory purge has been performed while at least one page was
  // frozen since the last time the renderer was backgrounded. This is used to
  // limit purges triggered by OnPageFrozen to once per background session when
  // the `kMemoryPurgeOnFreezeLimit` feature is enabled.
  bool did_purge_with_page_frozen_since_backgrounded_ = false;

  base::OneShotTimer purge_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
