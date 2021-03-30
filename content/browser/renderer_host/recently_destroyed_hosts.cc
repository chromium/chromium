// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/recently_destroyed_hosts.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
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
    base::TimeDelta::FromSeconds(20);

void RecordMetric(base::TimeDelta value) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "SiteIsolation.ReusePendingOrCommittedSite."
      "TimeSinceReusableProcessDestroyed",
      value, base::TimeDelta::FromMilliseconds(1),
      kRecentlyDestroyedNotFoundSentinel, 50);
}

}  // namespace

constexpr base::TimeDelta
    RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout;

RecentlyDestroyedHosts::~RecentlyDestroyedHosts() = default;

void RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
    const base::TimeTicks& reusable_host_lookup_time,
    const ProcessLock& process_lock,
    BrowserContext* browser_context) {
  auto* instance = GetInstance(browser_context);
  instance->RemoveExpiredEntries();
  const auto iter = instance->map_.find(process_lock);
  if (iter != instance->map_.end()) {
    RecordMetric(reusable_host_lookup_time - iter->second);
    return;
  }
  RecordMetric(kRecentlyDestroyedNotFoundSentinel);
}

void RecentlyDestroyedHosts::Add(
    RenderProcessHost* host,
    const base::TimeDelta& time_spent_in_delayed_shutdown,
    BrowserContext* browser_context) {
  if (time_spent_in_delayed_shutdown > kRecentlyDestroyedStorageTimeout)
    return;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  ProcessLock process_lock = policy->GetProcessLock(host->GetID());

  // Don't record sites with an empty process lock. This includes sites on
  // Android that are not isolated, and some special cases on desktop (e.g.,
  // chrome-extension://). These sites would not be affected by increased
  // process reuse, so are irrelevant for the metric being recorded.
  if (!process_lock.is_locked_to_site())
    return;

  // Record the time before |time_spent_in_delayed_shutdown| to exclude time
  // spent running unload handlers from the metric. This makes it consistent
  // across processes that were delayed by DelayProcessShutdownForUnload(),
  // and those that weren't.
  auto* instance = GetInstance(browser_context);
  instance->map_[process_lock] =
      base::TimeTicks::Now() - time_spent_in_delayed_shutdown;

  // Clean up list of recently destroyed hosts if it's getting large. This is
  // a fallback in case a subframe process hasn't been created in a long time
  // (which would clean up |map_|), e.g., on low-memory Android where site
  // isolation is not used.
  if (instance->map_.size() > 20)
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
  for (auto iter = map_.begin(); iter != map_.end();) {
    if (iter->second < expired_cutoff_time) {
      iter = map_.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace content
