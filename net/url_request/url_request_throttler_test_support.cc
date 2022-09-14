// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_test_support.h"

#include "net/url_request/url_request_throttler_entry.h"

namespace net {

TestTickClock::TestTickClock() = default;

TestTickClock::TestTickClock(base::TimeTicks now) : now_ticks_(now) {}

TestTickClock::~TestTickClock() = default;

base::TimeTicks TestTickClock::NowTicks() const {
  return now_ticks_;
}

}  // namespace net
