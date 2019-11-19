// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MESSAGE_TRACKER_H_
#define REMOTING_SIGNALING_MESSAGE_TRACKER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace remoting {

// FTL messaging doesn't guarantee a message won't be received more than once,
// so this class helps deduplicating messages based on their message ID.
class MessageTracker final {
 public:
  MessageTracker();
  ~MessageTracker();

  // Tracks |id|. If |id| is already tracked then it will update its tracking
  // time to now.
  void TrackId(const std::string& id);

  // Returns true if |id| has been tracked within the tracking window.
  //
  // The tracking window is at least (now - kCleanupInterval, now), and can be
  // up to (now - 2 * kCleanupInterval, now).
  bool IsIdTracked(const std::string& id) const;

 private:
  friend class MessageTrackerTest;

  // All IDs older than now - kCleanupInterval will be eventually removed, but
  // they are not guaranteed to be immediately removed after the interval.
  static const base::TimeDelta kCleanupInterval;

  void RemoveExpiredIds();

  std::map<std::string, base::Time> tracked_ids_;
  base::OneShotTimer cleanup_timer_;
  DISALLOW_COPY_AND_ASSIGN(MessageTracker);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MESSAGE_TRACKER_H_
