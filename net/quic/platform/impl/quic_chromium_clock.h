// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CHROMIUM_CLOCK_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CHROMIUM_CLOCK_H_

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"

namespace quic {

// Clock to efficiently retrieve an approximately accurate time from an
// net::EpollServer.
class NET_EXPORT_PRIVATE QuicChromiumClock : public QuicClock {
 public:
  static QuicChromiumClock* GetInstance();

  QuicChromiumClock();

  QuicChromiumClock(const QuicChromiumClock&) = delete;
  QuicChromiumClock& operator=(const QuicChromiumClock&) = delete;

  ~QuicChromiumClock() override;

  // QuicClock implementation:
  QuicTime ApproximateNow() const override;
  QuicTime Now() const override;
  QuicWallTime WallNow() const override;

  // Converts a QuicTime returned by QuicChromiumClock to base::TimeTicks.
  // Helper functions to safely convert between QuicTime and TimeTicks.
  static base::TimeTicks QuicTimeToTimeTicks(QuicTime quic_time);
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CHROMIUM_CLOCK_H_
