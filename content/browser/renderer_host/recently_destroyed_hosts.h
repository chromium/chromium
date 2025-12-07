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

// Stores information about recently destroyed RenderProcessHosts to gather
// metrics on process reuse potential. It maintains separate tracking for
// short-term subframe reuse and long-term main frame reuse.
class CONTENT_EXPORT RecentlyDestroyedHosts
    : public base::SupportsUserData::Data {
 public:
  // Describes the context for which a metric is being recorded.
  enum class Context {
    kMainFrame,
    kSubframe,
  };

  // Storage time for tracking recently destroyed processes for subframe reuse.
  // Intended to be long enough to capture a large portion of the process-reuse
  // opportunity.
  static constexpr base::TimeDelta kSubframeStorageTimeout = base::Seconds(60);

  // Long-duration storage time for the sites of recently destroyed processes,
  // specifically for the main frame reuse metric.
  // Intended to be long enough to capture session-level reuse patterns (e.g., a
  // user navigating away from and back to a site within a couple of hours).
  static constexpr base::TimeDelta kMainFrameStorageTimeout = base::Hours(2);

  ~RecentlyDestroyedHosts() override;
  RecentlyDestroyedHosts(const RecentlyDestroyedHosts& other) = delete;
  RecentlyDestroyedHosts& operator=(const RecentlyDestroyedHosts& other) =
      delete;

  // Looks for a recently destroyed host matching |process_lock| and records
  // metrics based on the |context|. For both main frames and subframes, this
  // records two histograms: a boolean for whether a potential reuse candidate
  // was found, and a timing histogram for the delay if one was found.
  static void RecordMetricIfReusableHostRecentlyDestroyed(
      Context context,
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

  // Removes all entries older than |kSubframeStorageTimeout| from
  // |recently_destroyed_hosts_for_subframe_reuse_|.
  void RemoveExpiredHostsForSubframeReuse();

  // Removes all entries older than |kMainFrameStorageTimeout| from
  // |recently_destroyed_hosts_for_main_frame_reuse_|.
  void RemoveExpiredHostsForMainFrameReuse();

  void AddReuseInterval(const base::TimeDelta& interval);

  // List of recent intervals between renderer-process destruction and the
  // creation of a renderer process for the same site. Sorted by interval, from
  // shortest to longest. Only a limited number are stored.
  base::flat_set<ReuseInterval> reuse_intervals_;

  // Map of ProcessLock to destruction time, for RenderProcessHosts destroyed
  // in the last |kSubframeStorageTimeout| seconds.
  std::map<ProcessLock, base::TimeTicks>
      recently_destroyed_hosts_for_subframe_reuse_;

  // Map of ProcessLock to destruction time for main frame reuse tracking.
  // Entries are kept for a long duration defined by |kMainFrameStorageTimeout|.
  std::map<ProcessLock, base::TimeTicks>
      recently_destroyed_hosts_for_main_frame_reuse_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_
