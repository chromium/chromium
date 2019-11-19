// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_

#include <list>
#include <map>
#include <string>

#include "base/observer_list.h"
#import "base/time/time.h"

class BreadcrumbManagerObserver;

// Stores events logged with |AddEvent| in memory which can later be retrieved
// with |GetEvents|. Events will be silently dropped after a certain amount of
// time has passed unless no more recent events are available. The internal
// management of events aims to keep relevant events available while clearing
// stale data.
class BreadcrumbManager {
 public:
  // Returns a list of the collected breadcrumb events which are still relevant
  // up to |event_count_limit|. Passing zero for |event_count_limit| signifies
  // no limit. Events returned will have a timestamp prepended to the original
  // |event| string representing when |AddEvent| was called.
  std::list<std::string> GetEvents(size_t event_count_limit);

  // Logs a breadcrumb event with message data |event|.
  void AddEvent(const std::string& event);

  // Adds and removes observers.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

  BreadcrumbManager();
  virtual ~BreadcrumbManager();

 private:
  // Drops events which are considered stale. Note that stale events are not
  // guaranteed to be removed. Explicitly, stale events will be retained while
  // newer events are limited.
  void DropOldEvents();

  // List of events, paired with the time which they were logged to minute
  // resolution. Newer events are at the end of the list.
  std::list<std::pair<base::Time, std::list<std::string>>> event_buckets_;

  base::ObserverList<BreadcrumbManagerObserver, /*check_empty=*/true>
      observers_;

  DISALLOW_COPY_AND_ASSIGN(BreadcrumbManager);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_H_
