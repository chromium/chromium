// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_openscreen_clock.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"

namespace media::cast {

base::SimpleTestTickClock* g_tick_clock = nullptr;
base::TimeTicks* g_origin_ticks = nullptr;

// static
void FakeOpenscreenClock::SetTickClock(base::SimpleTestTickClock* tick_clock) {
  CHECK(tick_clock);
  CHECK(!g_tick_clock);
  g_tick_clock = tick_clock;
  static base::TimeTicks origin_ticks(tick_clock->NowTicks());
  g_origin_ticks = &origin_ticks;
}

// static
void FakeOpenscreenClock::ClearTickClock() {
  CHECK(g_tick_clock);
  g_tick_clock = nullptr;
  *g_origin_ticks = base::TimeTicks();
}

// static
openscreen::Clock::time_point FakeOpenscreenClock::now() {
  CHECK(g_tick_clock);
  return openscreen::Clock::time_point(openscreen::Clock::duration(
      (g_tick_clock->NowTicks() - *g_origin_ticks).InMicroseconds()));
}

}  // namespace media::cast
