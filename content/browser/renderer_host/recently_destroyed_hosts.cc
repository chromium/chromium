// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/recently_destroyed_hosts.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

// Maintains a list of recently destroyed processes to gather metrics on the
// potential for process reuse (crbug.com/894253).
const void* const kRecentlyDestroyedHostTrackerKey =
    "RecentlyDestroyedHostTrackerKey";

}  // namespace

constexpr base::TimeDelta RecentlyDestroyedHosts::kSubframeStorageTimeout;
constexpr base::TimeDelta RecentlyDestroyedHosts::kMainFrameStorageTimeout;

RecentlyDestroyedHosts::~RecentlyDestroyedHosts() = default;

// static
void RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
    Context context,
    const base::TimeTicks& reusable_host_lookup_time,
    const ProcessLock& process_lock,
    BrowserContext* browser_context) {
  auto* instance = GetInstance(browser_context);

  switch (context) {
    case Context::kMainFrame: {
      instance->RemoveExpiredHostsForMainFrameReuse();
      const auto iter =
          instance->recently_destroyed_hosts_for_main_frame_reuse_.find(
              process_lock);

      const bool found =
          iter !=
          instance->recently_destroyed_hosts_for_main_frame_reuse_.end();
      base::UmaHistogramBoolean(
          "SiteIsolation.MissedReuseOpportunity.Found.MainFrame", found);

      if (found) {
        const base::TimeDelta reuse_interval =
            reusable_host_lookup_time - iter->second;
        base::UmaHistogramCustomTimes(
            "SiteIsolation.ReusePendingOrCommittedSite."
            "TimeSinceReusableProcessDestroyed.MainFrame2",
            reuse_interval, base::Milliseconds(1), kMainFrameStorageTimeout,
            50);
        instance->recently_destroyed_hosts_for_main_frame_reuse_.erase(iter);
      }
      break;
    }
    case Context::kSubframe: {
      instance->RemoveExpiredHostsForSubframeReuse();
      const auto iter =
          instance->recently_destroyed_hosts_for_subframe_reuse_.find(
              process_lock);

      const bool found =
          iter != instance->recently_destroyed_hosts_for_subframe_reuse_.end();
      base::UmaHistogramBoolean(
          "SiteIsolation.MissedReuseOpportunity.Found.Subframe", found);

      if (found) {
        const base::TimeDelta reuse_interval =
            reusable_host_lookup_time - iter->second;
        base::UmaHistogramCustomTimes(
            "SiteIsolation.ReusePendingOrCommittedSite."
            "TimeSinceReusableProcessDestroyed.Subframe2",
            reuse_interval, base::Milliseconds(1), kSubframeStorageTimeout, 50);
        instance->recently_destroyed_hosts_for_subframe_reuse_.erase(iter);
      }
      break;
    }
  }
  // Add zero to the list of recent reuse intervals to reduce the calculated
  // delay when no reuse has been possible for a while.
  instance->AddReuseInterval(base::TimeDelta());
}

// static
void RecentlyDestroyedHosts::Add(
    RenderProcessHost* host,
    const base::TimeDelta& time_spent_running_unload_handlers,
    BrowserContext* browser_context) {
  if (time_spent_running_unload_handlers > kSubframeStorageTimeout) {
    return;
  }

  ProcessLock process_lock = host->GetProcessLock();

  // Don't record sites with an empty process lock. This includes sites on
  // Android that are not isolated. These sites would not be affected by
  // increased process reuse, so are irrelevant for the metric being recorded.
  if (!process_lock.IsLockedToSite()) {
    return;
  }

  // Record the time before |time_spent_running_unload_handlers| to exclude time
  // spent in delayed-shutdown state from the metric. This makes it consistent
  // across processes that were delayed by DelayProcessShutdown(), and those
  // that weren't.
  auto* instance = GetInstance(browser_context);
  const base::TimeTicks destruction_time =
      base::TimeTicks::Now() - time_spent_running_unload_handlers;

  instance->recently_destroyed_hosts_for_subframe_reuse_[process_lock] =
      destruction_time;
  instance->recently_destroyed_hosts_for_main_frame_reuse_[process_lock] =
      destruction_time;

  // Periodically clean up the tracking maps if they get large. This is a
  // fallback in case no lookups have occurred recently to trigger the main
  // cleanup logic. The main frame map has a larger size limit because its
  // entries are retained for a long time (2 hours) to track session-level
  // reuse. The subframe map has a much shorter retention period (60 seconds),
  // so a smaller size limit is sufficient.
  if (instance->recently_destroyed_hosts_for_subframe_reuse_.size() > 20) {
    instance->RemoveExpiredHostsForSubframeReuse();
  }
  if (instance->recently_destroyed_hosts_for_main_frame_reuse_.size() > 200) {
    instance->RemoveExpiredHostsForMainFrameReuse();
  }
}

RecentlyDestroyedHosts::RecentlyDestroyedHosts() = default;

RecentlyDestroyedHosts* RecentlyDestroyedHosts::GetInstance(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecentlyDestroyedHosts* recently_destroyed_hosts =
      static_cast<RecentlyDestroyedHosts*>(
          browser_context->GetUserData(kRecentlyDestroyedHostTrackerKey));
  if (recently_destroyed_hosts)
    return recently_destroyed_hosts;

  recently_destroyed_hosts = new RecentlyDestroyedHosts;
  browser_context->SetUserData(kRecentlyDestroyedHostTrackerKey,
                               base::WrapUnique(recently_destroyed_hosts));
  return recently_destroyed_hosts;
}

void RecentlyDestroyedHosts::RemoveExpiredHostsForSubframeReuse() {
  const auto expired_cutoff_time =
      base::TimeTicks::Now() - kSubframeStorageTimeout;
  for (auto iter = recently_destroyed_hosts_for_subframe_reuse_.begin();
       iter != recently_destroyed_hosts_for_subframe_reuse_.end();) {
    if (iter->second < expired_cutoff_time) {
      iter = recently_destroyed_hosts_for_subframe_reuse_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void RecentlyDestroyedHosts::RemoveExpiredHostsForMainFrameReuse() {
  const auto expired_cutoff_time =
      base::TimeTicks::Now() - kMainFrameStorageTimeout;
  for (auto iter = recently_destroyed_hosts_for_main_frame_reuse_.begin();
       iter != recently_destroyed_hosts_for_main_frame_reuse_.end();) {
    if (iter->second < expired_cutoff_time) {
      iter = recently_destroyed_hosts_for_main_frame_reuse_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void RecentlyDestroyedHosts::AddReuseInterval(const base::TimeDelta& interval) {
  // The maximum size of |reuse_intervals_|. Kept small to ensure that delay
  // calculations that use |reuse_intervals_| are based on recent patterns.
  static constexpr size_t kReuseIntervalsMaxSize = 5;

  reuse_intervals_.insert({interval, base::TimeTicks::Now()});
  if (reuse_intervals_.size() > kReuseIntervalsMaxSize) {
    auto oldest_entry =
        std::min_element(reuse_intervals_.begin(), reuse_intervals_.end(),
                         [](ReuseInterval a, ReuseInterval b) {
                           return a.time_added < b.time_added;
                         });
    reuse_intervals_.erase(oldest_entry);
  }
  DCHECK_LE(reuse_intervals_.size(), kReuseIntervalsMaxSize);
}

}  // namespace content
