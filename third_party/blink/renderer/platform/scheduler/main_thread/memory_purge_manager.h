// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"

namespace blink {

// Manages process-wide proactive memory purging.
class PLATFORM_EXPORT MemoryPurgeManager {
 public:
  MemoryPurgeManager(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MemoryPurgeManager();

  // Called when a page is created or destroyed, to maintain the total count of
  // pages owned by a renderer.
  void OnPageCreated(PageLifecycleState state);
  void OnPageDestroyed(PageLifecycleState state);

  // Called when a page is frozen. If all pages are frozen or
  // |kFreezePurgeMemoryAllPagesFrozen| is disabled, and the renderer is
  // backgrounded, ensures that a delayed memory purge is scheduled. If the
  // timer is already running, uses the smallest requested delay.
  void OnPageFrozen();

  // Called when a page is resumed (unfrozen). Has the effect of unsuppressing
  // memory pressure notifications.
  void OnPageResumed();

  // Called when the renderer's process priority changes.
  void SetRendererBackgrounded(bool backgrounded);

  // Called when the renderer is backgrounded. Starts a timer responsible for
  // performing a memory purge upon expiry, if the
  // kPurgeRendererMemoryWhenBackgrounded feature is enabled. If the timer is
  // already running, uses the smallest requested delay.
  void OnRendererBackgrounded();

  // Called when the renderer is foregrounded. Has the effect of cancelling a
  // queued memory purge.
  void OnRendererForegrounded();

  // The time of purging after all pages have been frozen.
  static constexpr int kDefaultTimeToPurgeAfterFreezing = 0;

  // The time of first purging after a renderer is backgrounded. The value was
  // initially set to 30 minutes, but it was reduced to 1 minute because this
  // reduced the memory usage of a renderer 15 minutes after it was
  // backgrounded.
  //
  // Experiment results:
  // https://docs.google.com/document/d/1E88EYNlZE1DhmlgmjUnGnCAASm8-tWCAWXy8p53vmwc/edit?usp=sharing
  static constexpr int kDefaultMinTimeToPurgeAfterBackgrounded = 1;
  static constexpr int kDefaultMaxTimeToPurgeAfterBackgrounded = 4;

 private:
  // Starts |purge_timer_| to trigger a delayed memory purge. If the timer is
  // already running, starts the timer with the smaller of the requested delay
  // and the remaining delay.
  void RequestMemoryPurgeWithDelay(base::TimeDelta delay);

  // Called when the timer expires. Simulates a critical memory pressure signal
  // to purge memory. Suppresses memory pressure notifications if all pages
  // are frozen.
  void PerformMemoryPurge();

  // Returns true if:
  // - The kPurgeRendererMemoryWhenBackgrounded feature is enabled, and the
  //   renderer is awaiting a purge after being backgrounded.
  //
  // or if all of the following are true:
  //
  // - The renderer is backgrounded.
  // - All pages are frozen or kFreezePurgeMemoryAllPagesFrozen is disabled.
  bool CanPurge() const;

  // Returns true if |total_page_count_| == |frozen_page_count_|
  bool AreAllPagesFrozen() const;

  base::TimeDelta GetTimeToPurgeAfterBackgrounded() const;

  bool renderer_backgrounded_;

  // Keeps track of whether a memory purge was requested as a consequence of the
  // renderer transitioning to a backgrounded state. Prevents purges from being
  // cancelled if a purge was requested upon backgrounding the renderer and the
  // renderer is still backgrounded. Supports the metrics collection associated
  // with the PurgeAndSuspend experiment.
  bool backgrounded_purge_pending_;

  int total_page_count_;
  int frozen_page_count_;

  // Timer to delay memory purging.
  base::OneShotTimer purge_timer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
