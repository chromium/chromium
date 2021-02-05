// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"

namespace {

// The maximum number of breadcrumbs which are expected to be useful to store.
// NOTE: Events are "bucketed" into groups by short time intervals to make it
// more efficient to manage the continuous dropping of old events. Since events
// are only dropped at the bucket level, it is expected that the total number of
// stored breadcrumbs will exceed this value. This value should be close to the
// upper limit of useful events. (Most events + timestamp breadcrumbs are
// currently longer than 10 characters.)
constexpr unsigned long kMaxUsefulBreadcrumbEvents =
    breadcrumbs::kMaxDataLength / 10;

// The minimum number of event buckets to keep, even if they are expired.
const int kMinEventsBuckets = 2;

// Returns a Time used to bucket events for easier discarding of expired events.
base::Time EventBucket(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  exploded.millisecond = 0;
  exploded.second = 0;

  base::Time bucket_time;
  bool converted = base::Time::FromLocalExploded(exploded, &bucket_time);
  DCHECK(converted);
  return bucket_time;
}

}  // namespace

BreadcrumbManager::BreadcrumbManager() : start_time_(base::Time::Now()) {}

BreadcrumbManager::~BreadcrumbManager() = default;

size_t BreadcrumbManager::GetEventCount() {
  DropOldEvents();

  size_t count = 0;
  for (auto it = event_buckets_.rbegin(); it != event_buckets_.rend(); ++it) {
    std::list<std::string> bucket_events = it->second;
    count += bucket_events.size();
  }
  return count;
}

const std::list<std::string> BreadcrumbManager::GetEvents(
    size_t event_count_limit) {
  DropOldEvents();

  std::list<std::string> events;
  for (auto it = event_buckets_.rbegin(); it != event_buckets_.rend(); ++it) {
    std::list<std::string> bucket_events = it->second;
    for (auto event_it = bucket_events.rbegin();
         event_it != bucket_events.rend(); ++event_it) {
      std::string event = *event_it;
      events.push_front(event);
      if (event_count_limit > 0 && events.size() >= event_count_limit) {
        return events;
      }
    }
  }
  return events;
}

void BreadcrumbManager::AddEvent(const std::string& event) {
  base::Time time = base::Time::Now();
  base::Time bucket_time = EventBucket(time);

  // If bucket exists, it will be at the end of the list.
  if (event_buckets_.empty() || event_buckets_.back().first != bucket_time) {
    std::pair<base::Time, std::list<std::string>> bucket(
        bucket_time, std::list<std::string>());
    event_buckets_.push_back(bucket);
  }

  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  std::string timestamp =
      base::StringPrintf("%02d:%02d", exploded.minute, exploded.second);
  std::string event_log =
      base::StringPrintf("%s %s", timestamp.c_str(), event.c_str());
  event_buckets_.back().second.push_back(event_log);

  for (auto& observer : observers_) {
    observer.EventAdded(this, event_log);
  }

  DropOldEvents();
}

void BreadcrumbManager::DropOldEvents() {
  static const base::TimeDelta kMessageExpirationTime =
      base::TimeDelta::FromMinutes(20);

  bool old_buckets_dropped = false;
  base::Time now = base::Time::Now();
  // Drop buckets which are more than kMessageExpirationTime old.
  while (event_buckets_.size() > kMinEventsBuckets) {
    base::Time oldest_bucket_time = event_buckets_.front().first;
    if (now - oldest_bucket_time < kMessageExpirationTime) {
      break;
    }
    event_buckets_.pop_front();
    old_buckets_dropped = true;
  }

  // Drop buckets if the data is unlikely to ever be needed.
  unsigned long newer_event_count = 0;
  auto event_bucket_it = event_buckets_.rbegin();
  while (event_bucket_it != event_buckets_.rend()) {
    std::list<std::string> bucket_events = event_bucket_it->second;
    if (newer_event_count > kMaxUsefulBreadcrumbEvents) {
      event_buckets_.erase(event_buckets_.begin(), event_bucket_it.base());
      old_buckets_dropped = true;
      break;
    }
    newer_event_count += bucket_events.size();
    ++event_bucket_it;
  }

  if (old_buckets_dropped) {
    for (auto& observer : observers_) {
      observer.OldEventsRemoved(this);
    }
  }
}

void BreadcrumbManager::AddObserver(
    breadcrumbs::BreadcrumbManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BreadcrumbManager::RemoveObserver(
    breadcrumbs::BreadcrumbManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}
