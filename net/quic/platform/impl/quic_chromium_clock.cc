// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_chromium_clock.h"

#include "base/no_destructor.h"
#include "base/time/time.h"

namespace quic {

QuicChromiumClock* QuicChromiumClock::GetInstance() {
  static base::NoDestructor<QuicChromiumClock> instance;
  return instance.get();
}
QuicChromiumClock::QuicChromiumClock() {}

QuicChromiumClock::~QuicChromiumClock() {}

QuicTime QuicChromiumClock::ApproximateNow() const {
  // At the moment, Chrome does not have a distinct notion of ApproximateNow().
  // We should consider implementing this using MessageLoop::recent_time_.
  return Now();
}

QuicTime QuicChromiumClock::Now() const {
  int64_t ticks = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  DCHECK_GE(ticks, 0);
  return CreateTimeFromMicroseconds(ticks);
}

QuicWallTime QuicChromiumClock::WallNow() const {
  const base::TimeDelta time_since_unix_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  int64_t time_since_unix_epoch_micro = time_since_unix_epoch.InMicroseconds();
  DCHECK_GE(time_since_unix_epoch_micro, 0);
  return QuicWallTime::FromUNIXMicroseconds(time_since_unix_epoch_micro);
}

}  // namespace quic
