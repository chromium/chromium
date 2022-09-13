// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_throttle_test_support.h"

namespace extensions {

TestTickClock::TestTickClock() {}

TestTickClock::TestTickClock(base::TimeTicks now) : now_ticks_(now) {}

TestTickClock::~TestTickClock() {}

base::TimeTicks TestTickClock::NowTicks() const {
  return now_ticks_;
}

}  // namespace extensions
