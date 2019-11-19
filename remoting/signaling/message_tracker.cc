// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/message_tracker.h"

#include "base/stl_util.h"

namespace remoting {

// static
const base::TimeDelta MessageTracker::kCleanupInterval =
    base::TimeDelta::FromMinutes(2);

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
  return tracked_ids_.find(id) != tracked_ids_.end();
}

void MessageTracker::RemoveExpiredIds() {
  base::Time expire_time = base::Time::Now() - kCleanupInterval;
  base::EraseIf(tracked_ids_,
                [expire_time](const std::pair<std::string, base::Time>& pair) {
                  return pair.second <= expire_time;
                });
}

}  // namespace remoting
