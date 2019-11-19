// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_chromium_clock.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace quic {
namespace test {

TEST(QuicChromiumClockTest, Now) {
  QuicChromiumClock clock;

  QuicTime start = clock.Now();
  QuicTime now = clock.ApproximateNow();
  QuicTime end = clock.Now();

  EXPECT_LE(start, now);
  EXPECT_LE(now, end);
}

TEST(QuicChromiumClockTest, WallNow) {
  QuicChromiumClock clock;

  base::Time start = base::Time::Now();
  QuicWallTime now = clock.WallNow();
  base::Time end = base::Time::Now();

  // If end > start, then we can check now is between start and end.
  if (end > start) {
    EXPECT_LE(static_cast<uint64_t>(start.ToTimeT()), now.ToUNIXSeconds());
    EXPECT_LE(now.ToUNIXSeconds(), static_cast<uint64_t>(end.ToTimeT()));
  }
}

}  // namespace test
}  // namespace quic
