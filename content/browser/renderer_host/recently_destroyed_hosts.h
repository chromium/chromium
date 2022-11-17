// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_

#include "base/containers/flat_set.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace content {

class BrowserContext;
class ProcessLock;
class RenderProcessHost;

// Stores information about recently destroyed RenderProcessHosts in order to
// determine how often a process is created for a site that a just-destroyed
// host could have hosted. Emits the "SiteIsolation.ReusePendingOrCommittedSite.
// TimeSinceReusableProcessDestroyed" metric, which tracks this.
//
// Experimentally used to delay subframe-process shutdown. This aims to reduce
// process churn by keeping subframe processes alive for a few seconds. See
// GetSubframeProcessShutdownDelay() for details.
class CONTENT_EXPORT RecentlyDestroyedHosts
    : public base::SupportsUserData::Data {
 public:
  // Storage time for information about recently destroyed processes. Intended
  // to be long enough to capture a large portion of the process-reuse
  // opportunity.
  static constexpr base::TimeDelta kRecentlyDestroyedStorageTimeout =
      base::Seconds(15);

  ~RecentlyDestroyedHosts() override;
  RecentlyDestroyedHosts(const RecentlyDestroyedHosts& other) = delete;
  RecentlyDestroyedHosts& operator=(const RecentlyDestroyedHosts& other) =
      delete;

  // If a host matching |process_lock| was recently destroyed, records the
  // interval between its destruction and |reusable_host_lookup_time|. If not,
  // records a sentinel value.
  static void RecordMetricIfReusableHostRecentlyDestroyed(
      const base::TimeTicks& reusable_host_lookup_time,
      const ProcessLock& process_lock,
      BrowserContext* browser_context);

  // Adds |host|'s process lock to the list of recently destroyed hosts, or
  // updates its time if it's already present.
  static void Add(RenderProcessHost* host,
                  const base::TimeDelta& time_spent_running_unload_handlers,
                  BrowserContext* browser_context);

 private:
  friend class RecentlyDestroyedHostsTest;

  // The |interval| between a renderer process's destruction and the creation of
  // another renderer process for the same domain.
  struct ReuseInterval {
    base::TimeDelta interval;
    base::TimeTicks time_added;
    bool operator<(const ReuseInterval& rhs) const {
      return std::tie(interval, time_added) <
             std::tie(rhs.interval, rhs.time_added);
    }
  };

  RecentlyDestroyedHosts();

  // Returns the RecentlyDestroyedHosts instance stored in |browser_context|, or
  // creates an instance in |browser_context| and returns it if none exists.
  static RecentlyDestroyedHosts* GetInstance(BrowserContext* browser_context);

  // Removes all entries older than |kRecentlyDestroyedStorageTimeout| from
  // |recently_destroyed_hosts_|.
  void RemoveExpiredEntries();

  void AddReuseInterval(const base::TimeDelta& interval);

  // List of recent intervals between renderer-process destruction and the
  // creation of a renderer process for the same site. Sorted by interval, from
  // shortest to longest. Only a limited number are stored.
  base::flat_set<ReuseInterval> reuse_intervals_;
  // Map of ProcessLock to destruction time, for RenderProcessHosts destroyed
  // in the last |kRecentlyDestroyedStorageTimeout| seconds.
  std::map<ProcessLock, base::TimeTicks> recently_destroyed_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_
