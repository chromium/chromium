// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_EPOLL_CLOCK_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_EPOLL_CLOCK_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"

namespace epoll_server {

class SimpleEpollServer;

}  // namespace epoll_server

namespace quic {

// Clock to efficiently retrieve an approximately accurate time from an
// net::EpollServer.
class QuicEpollClock : public QuicClock {
 public:
  explicit QuicEpollClock(epoll_server::SimpleEpollServer* epoll_server);
  ~QuicEpollClock() override;

  // Returns the approximate current time as a QuicTime object.
  QuicTime ApproximateNow() const override;

  // Returns the current time as a QuicTime object.
  // Note: this uses significant resources, please use only if needed.
  QuicTime Now() const override;

  // Returns the current time as a QuicWallTime object.
  // Note: this uses significant resources, please use only if needed.
  QuicWallTime WallNow() const override;

  // Override to do less work in this implementation.  The epoll clock is
  // already based on system (unix epoch) time, no conversion required.
  QuicTime ConvertWallTimeToQuicTime(
      const QuicWallTime& walltime) const override;

 protected:
  epoll_server::SimpleEpollServer* epoll_server_;
  // Largest time returned from Now() so far.
  mutable QuicTime largest_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEpollClock);
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_EPOLL_CLOCK_H_
