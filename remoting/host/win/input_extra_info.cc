// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/input_extra_info.h"

#include <atomic>
#include <cstdint>
#include <limits>

#include "base/time/time.h"

namespace remoting {

namespace {

constexpr int64_t kActiveRemoteTouchSentinel =
    std::numeric_limits<int64_t>::max();

std::atomic<int64_t> g_remote_touch_active_until_micros{0};

}  // namespace

void RecordRemoteTouchInjectionActivity(bool has_active_touches) {
  if (has_active_touches) {
    g_remote_touch_active_until_micros.store(kActiveRemoteTouchSentinel,
                                             std::memory_order_release);
    return;
  }

  // Only start the post-release tail window if transitioning from active
  // remote touches in contact.
  int64_t current =
      g_remote_touch_active_until_micros.load(std::memory_order_acquire);
  if (current == kActiveRemoteTouchSentinel) {
    int64_t tail_until = (base::TimeTicks::Now() + kRemoteTouchTailWindow)
                             .since_origin()
                             .InMicroseconds();
    g_remote_touch_active_until_micros.compare_exchange_strong(
        current, tail_until, std::memory_order_acq_rel,
        std::memory_order_acquire);
  }
}

void ClearRemoteTouchInjectionActivity() {
  g_remote_touch_active_until_micros.store(0, std::memory_order_release);
}

bool IsRemoteTouchInjectionActive() {
  int64_t active_until =
      g_remote_touch_active_until_micros.load(std::memory_order_acquire);
  if (active_until <= 0) {
    return false;
  }
  if (active_until == kActiveRemoteTouchSentinel) {
    return true;
  }
  return base::TimeTicks::Now().since_origin().InMicroseconds() <= active_until;
}

}  // namespace remoting
