// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/recently_destroyed_hosts.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

// Maintains a list of recently destroyed processes to gather metrics on the
// potential for process reuse (crbug.com/894253).
const void* const kRecentlyDestroyedHostTrackerKey =
    "RecentlyDestroyedHostTrackerKey";
// Sentinel value indicating that no recently destroyed process matches the
// host currently seeking a process. Changing this invalidates the histogram.
constexpr base::TimeDelta kRecentlyDestroyedNotFoundSentinel =
    base::Seconds(20);

void RecordMetric(base::TimeDelta value) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "SiteIsolation.ReusePendingOrCommittedSite."
      "TimeSinceReusableProcessDestroyed",
      value, base::Milliseconds(1), kRecentlyDestroyedNotFoundSentinel, 50);
}

}  // namespace

constexpr base::TimeDelta
    RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout;

RecentlyDestroyedHosts::~RecentlyDestroyedHosts() = default;

// static
void RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
    const base::TimeTicks& reusable_host_lookup_time,
    const ProcessLock& process_lock,
    BrowserContext* browser_context) {
  auto* instance = GetInstance(browser_context);
  instance->RemoveExpiredEntries();
  const auto iter = instance->recently_destroyed_hosts_.find(process_lock);
  if (iter != instance->recently_destroyed_hosts_.end()) {
    // A host was recently destroyed that matched |process_lock|. Record the
    // interval between when the host was destroyed and when |process_lock|
    // looked for a reusable host.
    const base::TimeDelta reuse_interval =
        reusable_host_lookup_time - iter->second;
    RecordMetric(reuse_interval);
    instance->AddReuseInterval(reuse_interval);
    return;
  }
  RecordMetric(kRecentlyDestroyedNotFoundSentinel);
  // Add zero to the list of recent reuse intervals to reduce the calculated
  // delay when no reuse has been possible for a while.
  instance->AddReuseInterval(base::TimeDelta());
}

// static
void RecentlyDestroyedHosts::Add(
    RenderProcessHost* host,
    const base::TimeDelta& time_spent_running_unload_handlers,
    BrowserContext* browser_context) {
  if (time_spent_running_unload_handlers > kRecentlyDestroyedStorageTimeout)
    return;

  ProcessLock process_lock = host->GetProcessLock();

  // Don't record sites with an empty process lock. This includes sites on
  // Android that are not isolated, and some special cases on desktop (e.g.,
  // chrome-extension://). These sites would not be affected by increased
  // process reuse, so are irrelevant for the metric being recorded.
  if (!process_lock.is_locked_to_site())
    return;

  // Record the time before |time_spent_running_unload_handlers| to exclude time
  // spent in delayed-shutdown state from the metric. This makes it consistent
  // across processes that were delayed by DelayProcessShutdown(), and those
  // that weren't.
  auto* instance = GetInstance(browser_context);
  instance->recently_destroyed_hosts_[process_lock] =
      base::TimeTicks::Now() - time_spent_running_unload_handlers;

  // Clean up list of recently destroyed hosts if it's getting large. This is
  // a fallback in case a subframe process hasn't been created in a long time
  // (which would clean up |recently_destroyed_hosts_|), e.g., on low-memory
  // Android where site isolation is not used.
  if (instance->recently_destroyed_hosts_.size() > 20)
    instance->RemoveExpiredEntries();
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

void RecentlyDestroyedHosts::RemoveExpiredEntries() {
  const auto expired_cutoff_time =
      base::TimeTicks::Now() - kRecentlyDestroyedStorageTimeout;
  for (auto iter = recently_destroyed_hosts_.begin();
       iter != recently_destroyed_hosts_.end();) {
    if (iter->second < expired_cutoff_time) {
      iter = recently_destroyed_hosts_.erase(iter);
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
        std::min_element(reuse_intervals_.begin() + 1, reuse_intervals_.end(),
                         [](ReuseInterval a, ReuseInterval b) {
                           return a.time_added < b.time_added;
                         });
    reuse_intervals_.erase(oldest_entry);
  }
  DCHECK_LE(reuse_intervals_.size(), kReuseIntervalsMaxSize);
}

}  // namespace content
