// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_

#include <list>
#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/time/time.h"

namespace breadcrumbs {
class BreadcrumbManagerObserver;
}

// Stores events logged with |AddEvent| in memory which can later be retrieved
// with |GetEvents|. Events will be silently dropped after a certain amount of
// time has passed unless no more recent events are available. The internal
// management of events aims to keep relevant events available while clearing
// stale data.
class BreadcrumbManager {
 public:
  // Returns the number of collected breadcrumb events which are still relevant.
  // Note: This method may drop old events so the value can change even when no
  // new events have been added, but time has passed.
  size_t GetEventCount();

  // Returns a list of the collected breadcrumb events which are still relevant
  // up to |event_count_limit|. Passing zero for |event_count_limit| signifies
  // no limit. Events returned will have a timestamp prepended to the original
  // |event| string representing when |AddEvent| was called.
  // Note: This method may drop old events so the returned events can change
  // even if no new events have been added, but time has passed.
  const std::list<std::string> GetEvents(size_t event_count_limit);

  // Logs a breadcrumb event with message data |event|.
  // NOTE: |event| must not include newline characters as newlines are used by
  // BreadcrumbPersistentStore as a deliminator.
  void AddEvent(const std::string& event);

  // Adds and removes observers.
  void AddObserver(breadcrumbs::BreadcrumbManagerObserver* observer);
  void RemoveObserver(breadcrumbs::BreadcrumbManagerObserver* observer);

  BreadcrumbManager();
  ~BreadcrumbManager();

 private:
  BreadcrumbManager(const BreadcrumbManager&) = delete;
  BreadcrumbManager& operator=(const BreadcrumbManager&) = delete;

  // Drops events which are considered stale. Note that stale events are not
  // guaranteed to be removed. Explicitly, stale events will be retained while
  // newer events are limited.
  void DropOldEvents();

  // Creation time of the BreadcrumbManager.
  const base::Time start_time_;

  // List of events, paired with the time which they were logged to minute
  // resolution. Newer events are at the end of the list.
  std::list<std::pair<base::Time, std::list<std::string>>> event_buckets_;

  base::ObserverList<breadcrumbs::BreadcrumbManagerObserver,
                     /*check_empty=*/true>
      observers_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_
