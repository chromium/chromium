// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_

#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace content {

class BrowserContext;
class ProcessLock;
class RenderProcessHost;

class CONTENT_EXPORT RecentlyDestroyedHosts
    : public base::SupportsUserData::Data {
 public:
  // Storage time for information about recently destroyed processes. Intended
  // to be long enough to capture a large portion of the process-reuse
  // opportunity.
  static constexpr base::TimeDelta kRecentlyDestroyedStorageTimeout =
      base::TimeDelta::FromSeconds(10);

  ~RecentlyDestroyedHosts() override;
  RecentlyDestroyedHosts(const RecentlyDestroyedHosts& other) = delete;
  RecentlyDestroyedHosts& operator=(const RecentlyDestroyedHosts& other) =
      delete;

  // If a host matching |process_lock| was recently destroyed, records the time
  // between its destruction and |reusable_host_lookup_time|. If not, records a
  // sentinel value.
  static void RecordMetricIfReusableHostRecentlyDestroyed(
      const base::TimeTicks& reusable_host_lookup_time,
      const ProcessLock& process_lock,
      BrowserContext* browser_context);

  // Adds |host|'s process lock to the list of recently destroyed hosts, or
  // updates its time if it's already present.
  static void Add(RenderProcessHost* host,
                  const base::TimeDelta& time_spent_in_delayed_shutdown,
                  BrowserContext* browser_context);

 private:
  RecentlyDestroyedHosts();

  // Returns the RecentlyDestroyedHosts instance stored in |browser_context|, or
  // creates an instance in |browser_context| and returns it if none exists.
  static RecentlyDestroyedHosts* GetInstance(BrowserContext* browser_context);

  // Removes from |map_| all entries older than the storage timeout.
  void RemoveExpiredEntries();

  std::map<ProcessLock, base::TimeTicks> map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RECENTLY_DESTROYED_HOSTS_H_
