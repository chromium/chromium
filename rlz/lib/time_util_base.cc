// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/time_util.h"

#include "base/time/time.h"

namespace rlz_lib {

int64_t GetSystemTimeAsInt64() {
  // Seconds since epoch (Jan 1, 1970).
  double now_seconds = base::Time::Now().ToDoubleT();
  return static_cast<int64_t>(now_seconds * 1000 * 1000 * 10);
}

}  // namespace rlz_lib
