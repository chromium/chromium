// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/message_tracker.h"

#include <map>

#include "base/containers/contains.h"

namespace remoting {

// static
const base::TimeDelta MessageTracker::kCleanupInterval = base::Minutes(2);

MessageTracker::MessageTracker() = default;

MessageTracker::~MessageTracker() = default;

void MessageTracker::TrackId(const std::string& id) {
  tracked_ids_[id] = base::Time::Now();
  if (!cleanup_timer_.IsRunning()) {
    cleanup_timer_.Start(FROM_HERE, kCleanupInterval, this,
                         &MessageTracker::RemoveExpiredIds);
  }
}

bool MessageTracker::IsIdTracked(const std::string& id) const {
  return base::Contains(tracked_ids_, id);
}

void MessageTracker::RemoveExpiredIds() {
  base::Time expire_time = base::Time::Now() - kCleanupInterval;
  std::erase_if(tracked_ids_,
                [expire_time](const std::pair<std::string, base::Time>& pair) {
                  return pair.second <= expire_time;
                });
}

}  // namespace remoting
